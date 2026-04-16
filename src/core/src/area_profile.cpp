#include "core/area_profile.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

#include <tbb/parallel_for.h>

namespace ggm::core {

namespace {

constexpr int BISECTION_ITERS = 64;
constexpr double BISECTION_TOL = 1e-10;

// Find the minimum distance from point P to a polyline (set of segments).
double
distToPolyline(const math::Vec2& point, std::span<const math::Vec2> polyline) noexcept
{
  double minDist = std::numeric_limits<double>::max();
  for (std::size_t i = 0; i + 1 < polyline.size(); ++i) {
    const auto& segA = polyline[i];
    const auto& segB = polyline[i + 1];
    math::Vec2 segAB = segB - segA;
    math::Vec2 segAP = point - segA;
    double dotAB = segAB.squaredNorm();
    if (dotAB < 1e-30) {
      minDist = std::min(minDist, segAP.norm());
      continue;
    }
    double param = std::clamp(segAP.dot(segAB) / dotAB, 0.0, 1.0);
    math::Vec2 closest = segA + param * segAB;
    minDist = std::min(minDist, (point - closest).norm());
  }
  return minDist;
}

// Golden section minimization of |phi(t)| on [0,1]. Fallback when no sign change.
double
goldenSectionMin(const auto& absPhi) noexcept
{
  constexpr double INV_PHI = 0.6180339887498949; // 1/golden ratio
  double lo = 0.0;
  double hi = 1.0;
  double loEval = lo + (1.0 - INV_PHI) * (hi - lo);
  double hiEval = lo + INV_PHI * (hi - lo);
  double fLo = absPhi(loEval);
  double fHi = absPhi(hiEval);

  for (int iter = 0; iter < 64; ++iter) {
    if ((hi - lo) < 1e-10) {
      break;
    }
    if (fLo < fHi) {
      hi = hiEval;
      hiEval = loEval;
      fHi = fLo;
      loEval = lo + (1.0 - INV_PHI) * (hi - lo);
      fLo = absPhi(loEval);
    } else {
      lo = loEval;
      loEval = hiEval;
      fLo = fHi;
      hiEval = lo + INV_PHI * (hi - lo);
      fHi = absPhi(hiEval);
    }
  }
  return (fLo < fHi) ? loEval : hiEval;
}

// Bisection (with golden-section fallback) to find equidistant point.
double
bisectEquidistant(const math::Vec2& hubPt,
                  const math::Vec2& shroudPt,
                  std::span<const math::Vec2> hubPolyline,
                  std::span<const math::Vec2> shroudPolyline) noexcept
{
  auto phi = [&](double param) -> double {
    math::Vec2 point = (1.0 - param) * hubPt + param * shroudPt;
    return distToPolyline(point, hubPolyline) - distToPolyline(point, shroudPolyline);
  };

  double fLo = phi(0.0);
  double fHi = phi(1.0);

  if (fLo * fHi > 0.0) {
    // No sign change — fall back to minimizing |phi|
    return goldenSectionMin([&](double t) { return std::abs(phi(t)); });
  }

  double lo = 0.0;
  double hi = 1.0;
  for (int iter = 0; iter < BISECTION_ITERS; ++iter) {
    double mid = 0.5 * (lo + hi);
    if ((hi - lo) < BISECTION_TOL) {
      return mid;
    }
    double fMid = phi(mid);
    if (fMid * fLo < 0.0) {
      hi = mid;
    } else {
      lo = mid;
      fLo = fMid;
    }
  }
  return 0.5 * (lo + hi);
}

// Compute physical gradient of psi at grid node (row, col) using the Jacobian
// inverse mapping from logical (i,j) to physical (z,r) space.
// Central finite differences in the interior, one-sided at boundaries.
math::Vec2
psiGradientDir(const FlowSolution& sol, int row, int col) noexcept
{
  const auto& grid = sol.grid;

  auto nodeIdx = [&](int i, int j) -> std::size_t {
    return static_cast<std::size_t>(i * grid.m + j);
  };

  // Partial derivatives in logical space (i = stream-wise, j = transverse)
  auto centralDiffI = [&](auto getter) -> double {
    if (row == 0) {
      return getter(row + 1, col) - getter(row, col);
    }
    if (row == grid.nh - 1) {
      return getter(row, col) - getter(row - 1, col);
    }
    return 0.5 * (getter(row + 1, col) - getter(row - 1, col));
  };
  auto centralDiffJ = [&](auto getter) -> double {
    if (col == 0) {
      return getter(row, col + 1) - getter(row, col);
    }
    if (col == grid.m - 1) {
      return getter(row, col) - getter(row, col - 1);
    }
    return 0.5 * (getter(row, col + 1) - getter(row, col - 1));
  };

  auto zAt = [&](int i, int j) { return grid.nodes[nodeIdx(i, j)].x(); };
  auto rAt = [&](int i, int j) { return grid.nodes[nodeIdx(i, j)].y(); };
  auto psiAt = [&](int i, int j) { return sol.psi[nodeIdx(i, j)]; };

  double zI = centralDiffI(zAt);
  double zJ = centralDiffJ(zAt);
  double rI = centralDiffI(rAt);
  double rJ = centralDiffJ(rAt);
  double psiI = centralDiffI(psiAt);
  double psiJ = centralDiffJ(psiAt);

  // Jacobian J = [[z_i, z_j], [r_i, r_j]]. Physical grad = J^{-T} * [psi_i; psi_j].
  // J^{-T} * [a; b] = (1/det) * [ r_j*a - r_i*b ; -z_j*a + z_i*b ]
  double det = zI * rJ - zJ * rI;
  if (std::abs(det) < 1e-15) {
    return {0.0, 1.0};
  }
  double psiZ = (rJ * psiI - rI * psiJ) / det;
  double psiR = (-zJ * psiI + zI * psiJ) / det;

  math::Vec2 grad{psiZ, psiR};
  double gradNorm = grad.norm();
  if (gradNorm < 1e-15) {
    return {0.0, 1.0};
  }
  return grad / gradNorm;
}

// Intersect the ray (point, dir) with `poly`. Returns the closest hit by
// |rayParam|. If no segment of `poly` intersects the ray, returns
// std::nullopt — the caller must then fall back to a known-good endpoint.
// At boundary rows the gradient direction can slightly miss the polyline
// end, so robust failure reporting is required.
std::optional<math::Vec2>
raySegmentHit(const math::Vec2& point,
              const math::Vec2& dir,
              std::span<const math::Vec2> poly) noexcept
{
  double bestDist = std::numeric_limits<double>::max();
  std::optional<math::Vec2> bestPt;
  for (std::size_t i = 0; i + 1 < poly.size(); ++i) {
    const auto& segA = poly[i];
    const auto& segB = poly[i + 1];
    math::Vec2 segD = segB - segA;
    double det = dir.x() * (-segD.y()) - dir.y() * (-segD.x());
    if (std::abs(det) < 1e-15) {
      continue;
    }
    math::Vec2 diff = segA - point;
    double segParam = (dir.x() * diff.y() - dir.y() * diff.x()) / det;
    if (segParam < -1e-6 || segParam > 1.0 + 1e-6) {
      continue;
    }
    double rayParam = ((-segD.y()) * diff.x() - (-segD.x()) * diff.y()) / det;
    double dist = std::abs(rayParam);
    if (dist < bestDist) {
      bestDist = dist;
      segParam = std::clamp(segParam, 0.0, 1.0);
      bestPt = segA + segParam * segD;
    }
  }
  return bestPt;
}

// Chord length across the channel from `point` in `dir`. If either polyline
// doesn't intersect the ray (happens at inlet/outlet rows where a slightly
// tilted gradient runs past the polyline's open end), fall back to the
// supplied hub/shroud anchor points — they are guaranteed to be on the
// polylines since the grid is built from them.
double
chordAlongDir(const math::Vec2& point,
              const math::Vec2& dir,
              std::span<const math::Vec2> hubPolyline,
              std::span<const math::Vec2> shroudPolyline,
              const math::Vec2& hubAnchor,
              const math::Vec2& shroudAnchor) noexcept
{
  math::Vec2 hitHub = raySegmentHit(point, dir, hubPolyline).value_or(hubAnchor);
  math::Vec2 hitShroud = raySegmentHit(point, dir, shroudPolyline).value_or(shroudAnchor);
  return (hitShroud - hitHub).norm();
}

} // namespace

Result<AreaProfile>
computeAreaProfile(const FlowSolution& sol,
                   std::span<const math::Vec2> hubPolyline,
                   std::span<const math::Vec2> shroudPolyline,
                   const PumpParams& params) noexcept
{
  const auto& grid = sol.grid;
  if (grid.nh < 2 || grid.m < 3) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  auto numRows = static_cast<std::size_t>(grid.nh);
  AreaProfile profile;
  profile.midPoints.resize(numRows);
  profile.chordLengths.resize(numRows);
  profile.flowAreas.resize(numRows);

  // Parallel bisection + chord computation over rows
  tbb::parallel_for(0, grid.nh, [&](int row) {
    auto rowU = static_cast<std::size_t>(row);
    auto hubIdx = static_cast<std::size_t>(row * grid.m);
    auto shrIdx = static_cast<std::size_t>(row * grid.m + grid.m - 1);
    const auto& hubPt = grid.nodes[hubIdx];
    const auto& shrPt = grid.nodes[shrIdx];

    // Bisect for equidistant point
    double param = bisectEquidistant(hubPt, shrPt, hubPolyline, shroudPolyline);
    math::Vec2 midPt = (1.0 - param) * hubPt + param * shrPt;
    profile.midPoints[rowU] = midPt;

    // Chord along psi gradient direction, evaluated at the grid column
    // nearest to the equidistant midpoint (matches Python reference which
    // uses the grid node closest to the midline point).
    int col = std::clamp(static_cast<int>(std::round(param * (grid.m - 1))), 0, grid.m - 1);
    math::Vec2 gradDir = psiGradientDir(sol, row, col);
    double chord = chordAlongDir(midPt, gradDir, hubPolyline, shroudPolyline, hubPt, shrPt);
    profile.chordLengths[rowU] = chord;

    // Area = 2*pi*r*chord
    double radius = std::max(midPt.y(), 1e-9);
    profile.flowAreas[rowU] = 2.0 * std::numbers::pi * radius * chord;
  });

  // Compute arc lengths along midline
  profile.arcLengths.resize(numRows, 0.0);
  for (std::size_t i = 1; i < numRows; ++i) {
    profile.arcLengths[i] =
      profile.arcLengths[i - 1] + (profile.midPoints[i] - profile.midPoints[i - 1]).norm();
  }

  // Reference areas
  profile.f1 = std::numbers::pi / 4.0 * (params.din * params.din - params.dvt * params.dvt);
  profile.f2 = std::numbers::pi * params.d2 * params.b2;

  return profile;
}

} // namespace ggm::core
