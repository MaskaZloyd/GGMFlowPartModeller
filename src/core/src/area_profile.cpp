#include "core/area_profile.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <optional>
#include <span>

#include <tbb/parallel_for.h>

namespace ggm::core {

namespace {

constexpr int MID_CHORD_ITERS = 8;

constexpr double MID_CHORD_TOL = 1e-9;
constexpr double INTERSECTION_EPS = 1e-8;
constexpr double DET_TOL = 1e-15;
constexpr double MIN_CHORD_LENGTH = 1e-12;
constexpr double MIN_RADIUS_SUM = 2e-9;
constexpr double MAX_CHORD_STRETCH = 3.0;
constexpr double MAX_INVALID_CHORD_FRACTION = 0.20;

double
cross2d(const math::Vec2& a, const math::Vec2& b) noexcept
{
  return a.x() * b.y() - a.y() * b.x();
}

struct ChordResult
{
  math::Vec2 hubPoint{0.0, 0.0};
  math::Vec2 shroudPoint{0.0, 0.0};
  double length{0.0};
  bool valid{false};
};

struct RowLocation
{
  int col0{0};
  double t{0.0};
};

RowLocation
nearestSegmentInRow(const StripGrid& grid, int row, const math::Vec2& point) noexcept
{
  int bestCol0 = 0;
  double bestT = 0.0;
  double bestDist2 = std::numeric_limits<double>::max();

  for (int col = 0; col + 1 < grid.m; ++col) {
    const auto idx0 = static_cast<std::size_t>(row * grid.m + col);
    const auto idx1 = static_cast<std::size_t>(row * grid.m + col + 1);

    const math::Vec2& a = grid.nodes[idx0];
    const math::Vec2& b = grid.nodes[idx1];

    const math::Vec2 ab = b - a;
    const double ab2 = ab.squaredNorm();

    if (ab2 < 1e-30) {
      continue;
    }

    const double t = std::clamp((point - a).dot(ab) / ab2, 0.0, 1.0);

    const math::Vec2 closest = a + t * ab;
    const double dist2 = (point - closest).squaredNorm();

    if (dist2 < bestDist2) {
      bestDist2 = dist2;
      bestCol0 = col;
      bestT = t;
    }
  }

  return RowLocation{.col0 = bestCol0, .t = bestT};
}

// Compute physical gradient of psi at grid node (row, col) using the Jacobian
// inverse mapping from logical (i,j) to physical (z,r) space.
//
// Logical coordinates:
//   i - row, approximately streamwise direction;
//   j - column, approximately cross-channel direction.
//
// Physical coordinates:
//   x() = z;
//   y() = r.
math::Vec2
psiGradientDir(const FlowSolution& sol, int row, int col) noexcept
{
  const auto& grid = sol.grid;

  auto nodeIdx = [&](int i, int j) -> std::size_t {
    return static_cast<std::size_t>(i * grid.m + j);
  };

  auto diffI = [&](const auto& getter) -> double {
    if (row == 0) {
      return getter(row + 1, col) - getter(row, col);
    }

    if (row == grid.nh - 1) {
      return getter(row, col) - getter(row - 1, col);
    }

    return 0.5 * (getter(row + 1, col) - getter(row - 1, col));
  };

  auto diffJ = [&](const auto& getter) -> double {
    if (col == 0) {
      return getter(row, col + 1) - getter(row, col);
    }

    if (col == grid.m - 1) {
      return getter(row, col) - getter(row, col - 1);
    }

    return 0.5 * (getter(row, col + 1) - getter(row, col - 1));
  };

  auto zAt = [&](int i, int j) noexcept -> double { return grid.nodes[nodeIdx(i, j)].x(); };

  auto rAt = [&](int i, int j) noexcept -> double { return grid.nodes[nodeIdx(i, j)].y(); };

  auto psiAt = [&](int i, int j) noexcept -> double { return sol.psi[nodeIdx(i, j)]; };

  const double zI = diffI(zAt);
  const double zJ = diffJ(zAt);
  const double rI = diffI(rAt);
  const double rJ = diffJ(rAt);
  const double psiI = diffI(psiAt);
  const double psiJ = diffJ(psiAt);

  // J = [[z_i, z_j],
  //      [r_i, r_j]]
  //
  // grad_phys(psi) = J^{-T} * [psi_i, psi_j]^T
  const double det = zI * rJ - zJ * rI;

  if (std::abs(det) < DET_TOL) {
    return {0.0, 1.0};
  }

  const double psiZ = (rJ * psiI - rI * psiJ) / det;
  const double psiR = (-zJ * psiI + zI * psiJ) / det;

  const math::Vec2 grad{psiZ, psiR};
  const double gradNorm = grad.norm();

  if (!std::isfinite(gradNorm) || gradNorm < DET_TOL) {
    return {0.0, 1.0};
  }

  return grad / gradNorm;
}

// Smoothly interpolate psi-gradient direction along the current grid row.
// This avoids discrete jumps caused by selecting only the nearest column.
math::Vec2
interpolatedPsiGradientDir(const FlowSolution& sol,
                           int row,
                           const math::Vec2& point,
                           const math::Vec2& fallbackDir) noexcept
{
  const auto& grid = sol.grid;

  const RowLocation loc = nearestSegmentInRow(grid, row, point);

  math::Vec2 g0 = psiGradientDir(sol, row, loc.col0);
  math::Vec2 g1 = psiGradientDir(sol, row, loc.col0 + 1);

  // Direction of a line is sign-invariant, but interpolation is not.
  // Prevent cancellation if neighboring gradients are oriented oppositely.
  if (g0.dot(g1) < 0.0) {
    g1 = -g1;
  }

  const math::Vec2 g = (1.0 - loc.t) * g0 + loc.t * g1;
  const double n = g.norm();

  if (std::isfinite(n) && n >= DET_TOL) {
    return g / n;
  }

  const double fallbackNorm = fallbackDir.norm();

  if (std::isfinite(fallbackNorm) && fallbackNorm >= DET_TOL) {
    return fallbackDir / fallbackNorm;
  }

  return {0.0, 1.0};
}

// Intersect infinite line:
//   point + t * dir
// with a polyline.
//
// If several intersections exist, choose the one closest to the row anchor.
// This is important for curved or locally concave geometry: we need the local
// channel wall point, not the globally nearest intersection to `point`.
std::optional<math::Vec2>
linePolylineHitNearAnchor(const math::Vec2& point,
                          const math::Vec2& dir,
                          std::span<const math::Vec2> polyline,
                          const math::Vec2& anchor) noexcept
{
  double bestScore = std::numeric_limits<double>::max();
  std::optional<math::Vec2> bestPoint;

  for (std::size_t i = 0; i + 1 < polyline.size(); ++i) {
    const math::Vec2& segA = polyline[i];
    const math::Vec2& segB = polyline[i + 1];

    const math::Vec2 segD = segB - segA;
    const double denom = cross2d(dir, segD);

    if (std::abs(denom) < DET_TOL) {
      continue;
    }

    const math::Vec2 diff = segA - point;

    // point + t * dir = segA + u * segD
    const double uRaw = cross2d(diff, dir) / denom;

    if (uRaw < -INTERSECTION_EPS || uRaw > 1.0 + INTERSECTION_EPS) {
      continue;
    }

    const double u = std::clamp(uRaw, 0.0, 1.0);
    const math::Vec2 hit = segA + u * segD;

    const double score = (hit - anchor).squaredNorm();

    if (score < bestScore) {
      bestScore = score;
      bestPoint = hit;
    }
  }

  return bestPoint;
}

// Local chord between hub and shroud along direction `dir`.
ChordResult
chordAlongDir(const math::Vec2& point,
              const math::Vec2& dir,
              std::span<const math::Vec2> hubPolyline,
              std::span<const math::Vec2> shroudPolyline,
              const math::Vec2& hubAnchor,
              const math::Vec2& shroudAnchor) noexcept
{
  const double anchorLength = (shroudAnchor - hubAnchor).norm();

  if (!std::isfinite(anchorLength) || anchorLength < MIN_CHORD_LENGTH) {
    return ChordResult{
      .hubPoint = hubAnchor, .shroudPoint = shroudAnchor, .length = 0.0, .valid = false};
  }

  const auto hitHub = linePolylineHitNearAnchor(point, dir, hubPolyline, hubAnchor);

  const auto hitShroud = linePolylineHitNearAnchor(point, dir, shroudPolyline, shroudAnchor);

  if (!hitHub.has_value() || !hitShroud.has_value()) {
    return ChordResult{
      .hubPoint = hubAnchor, .shroudPoint = shroudAnchor, .length = anchorLength, .valid = false};
  }

  const math::Vec2 chordVec = *hitShroud - *hitHub;
  const math::Vec2 anchorVec = shroudAnchor - hubAnchor;

  const double length = chordVec.norm();

  if (!std::isfinite(length) || length < MIN_CHORD_LENGTH) {
    return ChordResult{
      .hubPoint = hubAnchor, .shroudPoint = shroudAnchor, .length = anchorLength, .valid = false};
  }

  // The chord must be locally oriented from hub to shroud.
  if (chordVec.dot(anchorVec) <= 0.0) {
    return ChordResult{
      .hubPoint = hubAnchor, .shroudPoint = shroudAnchor, .length = anchorLength, .valid = false};
  }

  // Protection against hitting a remote branch of the wall polyline.
  if (length > MAX_CHORD_STRETCH * anchorLength) {
    return ChordResult{
      .hubPoint = hubAnchor, .shroudPoint = shroudAnchor, .length = anchorLength, .valid = false};
  }

  return ChordResult{
    .hubPoint = *hitHub, .shroudPoint = *hitShroud, .length = length, .valid = true};
}

ChordResult
localMidChord(const FlowSolution& sol,
              int row,
              std::span<const math::Vec2> hubPolyline,
              std::span<const math::Vec2> shroudPolyline) noexcept
{
  const auto& grid = sol.grid;

  const auto hubIdx = static_cast<std::size_t>(row * grid.m);
  const auto shroudIdx = static_cast<std::size_t>(row * grid.m + grid.m - 1);

  const math::Vec2& hubAnchor = grid.nodes[hubIdx];
  const math::Vec2& shroudAnchor = grid.nodes[shroudIdx];

  math::Vec2 midPoint = 0.5 * (hubAnchor + shroudAnchor);

  ChordResult current{.hubPoint = hubAnchor,
                      .shroudPoint = shroudAnchor,
                      .length = (shroudAnchor - hubAnchor).norm(),
                      .valid = false};

  const math::Vec2 fallbackDir = shroudAnchor - hubAnchor;

  for (int iter = 0; iter < MID_CHORD_ITERS; ++iter) {
    const math::Vec2 gradDir = interpolatedPsiGradientDir(sol, row, midPoint, fallbackDir);

    const ChordResult next =
      chordAlongDir(midPoint, gradDir, hubPolyline, shroudPolyline, hubAnchor, shroudAnchor);

    if (!next.valid) {
      return current;
    }

    const math::Vec2 nextMidPoint = 0.5 * (next.hubPoint + next.shroudPoint);

    current = next;

    if ((nextMidPoint - midPoint).norm() < MID_CHORD_TOL) {
      return current;
    }

    midPoint = nextMidPoint;
  }

  return current;
}

std::optional<double>
axisymmetricAreaOfChord(const ChordResult& chord) noexcept
{
  // Surface area generated by a straight meridional chord:
  //
  // A = 2*pi * ∫ r ds
  //
  // Along a straight segment r varies linearly, therefore:
  //
  // A = 2*pi * L * (r0 + r1)/2
  //   = pi * (r0 + r1) * L
  const double radiusSum = chord.hubPoint.y() + chord.shroudPoint.y();

  if (!std::isfinite(radiusSum) || radiusSum <= MIN_RADIUS_SUM) {
    return std::nullopt;
  }

  if (!std::isfinite(chord.length) || chord.length < MIN_CHORD_LENGTH) {
    return std::nullopt;
  }

  return std::numbers::pi * radiusSum * chord.length;
}

} // namespace

Result<AreaProfile>
computeAreaProfile(const FlowSolution& sol,
                   std::span<const math::Vec2> hubPolyline,
                   std::span<const math::Vec2> shroudPolyline,
                   const PumpParams& params)
{
  const auto& grid = sol.grid;

  if (grid.nh < 2 || grid.m < 3) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  const auto expectedNodes = static_cast<std::size_t>(grid.nh) * static_cast<std::size_t>(grid.m);

  if (grid.nodes.size() < expectedNodes || sol.psi.size() < expectedNodes) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  if (hubPolyline.size() < 2 || shroudPolyline.size() < 2) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  const auto numRows = static_cast<std::size_t>(grid.nh);

  AreaProfile profile;
  profile.midPoints.resize(numRows);
  profile.chordLengths.resize(numRows);
  profile.flowAreas.resize(numRows);

  std::atomic<int> invalidChordCount{0};
  std::atomic<int> invalidAreaCount{0};

  tbb::parallel_for(0, grid.nh, [&](int row) {
    const auto rowIdx = static_cast<std::size_t>(row);

    const ChordResult chord = localMidChord(sol, row, hubPolyline, shroudPolyline);

    if (!chord.valid) {
      invalidChordCount.fetch_add(1, std::memory_order_relaxed);
    }

    const math::Vec2 midPoint = 0.5 * (chord.hubPoint + chord.shroudPoint);

    const std::optional<double> area = axisymmetricAreaOfChord(chord);

    if (!area.has_value()) {
      invalidAreaCount.fetch_add(1, std::memory_order_relaxed);

      profile.midPoints[rowIdx] = midPoint;
      profile.chordLengths[rowIdx] = 0.0;
      profile.flowAreas[rowIdx] = 0.0;

      return;
    }

    profile.midPoints[rowIdx] = midPoint;
    profile.chordLengths[rowIdx] = chord.length;
    profile.flowAreas[rowIdx] = *area;
  });

  if (invalidAreaCount.load(std::memory_order_relaxed) > 0) {
    return std::unexpected(CoreError::GeometryBuildFailed);
  }

  const int allowedInvalidChords = std::max(
    2, static_cast<int>(std::ceil(MAX_INVALID_CHORD_FRACTION * static_cast<double>(grid.nh))));

  if (invalidChordCount.load(std::memory_order_relaxed) > allowedInvalidChords) {
    return std::unexpected(CoreError::GeometryBuildFailed);
  }

  profile.arcLengths.resize(numRows, 0.0);

  for (std::size_t i = 1; i < numRows; ++i) {
    profile.arcLengths[i] =
      profile.arcLengths[i - 1] + (profile.midPoints[i] - profile.midPoints[i - 1]).norm();
  }

  profile.f1 = std::numbers::pi / 4.0 * (params.din * params.din - params.dvt * params.dvt);

  profile.f2 = std::numbers::pi * params.d2 * params.b2;

  return profile;
}

} // namespace ggm::core
