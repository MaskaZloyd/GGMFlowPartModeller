#include "core/area_profile.hpp"

#include <algorithm>
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

int
nearestColumnInRow(const StripGrid& grid, int row, const math::Vec2& point) noexcept
{
  int bestCol = 0;
  double bestDist2 = std::numeric_limits<double>::max();

  for (int col = 0; col < grid.m; ++col) {
    const auto idx = static_cast<std::size_t>(row * grid.m + col);
    const double dist2 = (grid.nodes[idx] - point).squaredNorm();

    if (dist2 < bestDist2) {
      bestDist2 = dist2;
      bestCol = col;
    }
  }

  return bestCol;
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
  const auto hitHub = linePolylineHitNearAnchor(point, dir, hubPolyline, hubAnchor);

  const auto hitShroud = linePolylineHitNearAnchor(point, dir, shroudPolyline, shroudAnchor);

  if (!hitHub.has_value() || !hitShroud.has_value()) {
    const double fallbackLength = (shroudAnchor - hubAnchor).norm();

    return ChordResult{
      .hubPoint = hubAnchor, .shroudPoint = shroudAnchor, .length = fallbackLength, .valid = false};
  }

  const double length = (*hitShroud - *hitHub).norm();

  if (!std::isfinite(length) || length < MIN_CHORD_LENGTH) {
    const double fallbackLength = (shroudAnchor - hubAnchor).norm();

    return ChordResult{
      .hubPoint = hubAnchor, .shroudPoint = shroudAnchor, .length = fallbackLength, .valid = false};
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

  for (int iter = 0; iter < MID_CHORD_ITERS; ++iter) {
    const int col = nearestColumnInRow(grid, row, midPoint);
    const math::Vec2 gradDir = psiGradientDir(sol, row, col);

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

double
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
  const double radiusSum = std::max(chord.hubPoint.y() + chord.shroudPoint.y(), MIN_RADIUS_SUM);

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

  if (hubPolyline.size() < 2 || shroudPolyline.size() < 2) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  const auto numRows = static_cast<std::size_t>(grid.nh);

  AreaProfile profile;
  profile.midPoints.resize(numRows);
  profile.chordLengths.resize(numRows);
  profile.flowAreas.resize(numRows);

  tbb::parallel_for(0, grid.nh, [&](int row) {
    const auto rowIdx = static_cast<std::size_t>(row);

    const ChordResult chord = localMidChord(sol, row, hubPolyline, shroudPolyline);

    const math::Vec2 midPoint = 0.5 * (chord.hubPoint + chord.shroudPoint);

    profile.midPoints[rowIdx] = midPoint;
    profile.chordLengths[rowIdx] = chord.length;
    profile.flowAreas[rowIdx] = axisymmetricAreaOfChord(chord);
  });

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
