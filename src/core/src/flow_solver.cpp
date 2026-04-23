#include "core/flow_solver.hpp"

#include "core/area_profile.hpp"
#include "core/fem_solver.hpp"
#include "core/logging.hpp"
#include "core/streamline_extractor.hpp"
#include "core/strip_grid.hpp"
#include "core/velocity_field.hpp"
#include "math/arc_length.hpp"

#include <cstddef>
#include <numbers>
#include <vector>

namespace ggm::core {

namespace {

// Append a point past the polyline's last vertex along its tangent direction
// so the FEM domain extends a few millimeters past the outlet radius. This
// lets the stream function boundary "breathe" and removes the outlet-edge
// artifact that would otherwise compress streamlines toward the shroud.
void
extendPolylineAtEnd(std::vector<math::Vec2>& poly, double extraDist)
{
  if (poly.size() < 2 || extraDist <= 0.0) {
    return;
  }
  const auto& last = poly.back();
  const auto& prev = poly[poly.size() - 2];
  math::Vec2 dir = last - prev;
  double len = dir.norm();
  if (len < 1e-12) {
    return;
  }
  dir /= len;
  poly.push_back(last + extraDist * dir);
}

// Interpolate point onto r == rMax on the segment (pa, pb). Assumes pa.y
// and pb.y straddle rMax.
math::Vec2
interpR(const math::Vec2& pa, const math::Vec2& pb, double rMax)
{
  double denom = pb.y() - pa.y();
  if (std::abs(denom) < 1e-12) {
    return pa;
  }
  double t = (rMax - pa.y()) / denom;
  return (1.0 - t) * pa + t * pb;
}

// Keep only the portion of the polyline with r <= rMax, direction-agnostic.
// The marching squares tracer may emit polylines ordered either inlet->outlet
// or outlet->inlet, so we must scan for any contiguous in-domain segment
// and clip both ends with linear interpolation onto r == rMax.
void
clipPolylineAtRMax(std::vector<math::Vec2>& poly, double rMax)
{
  if (poly.empty()) {
    return;
  }
  // Find first and last indices with y <= rMax.
  int first = -1;
  int last = -1;
  for (int i = 0; i < static_cast<int>(poly.size()); ++i) {
    if (poly[static_cast<std::size_t>(i)].y() <= rMax) {
      if (first < 0) {
        first = i;
      }
      last = i;
    }
  }
  if (first < 0) {
    poly.clear();
    return;
  }

  std::vector<math::Vec2> out;
  out.reserve(poly.size());

  // Optional head-interpolation if the polyline enters the domain.
  if (first > 0) {
    out.push_back(interpR(
      poly[static_cast<std::size_t>(first - 1)], poly[static_cast<std::size_t>(first)], rMax));
  }
  for (int i = first; i <= last; ++i) {
    out.push_back(poly[static_cast<std::size_t>(i)]);
  }
  // Optional tail-interpolation if the polyline exits the domain.
  if (last + 1 < static_cast<int>(poly.size())) {
    out.push_back(interpR(
      poly[static_cast<std::size_t>(last)], poly[static_cast<std::size_t>(last + 1)], rMax));
  }

  poly = std::move(out);
}

} // namespace

Result<FlowResults>
FlowSolver::solve(const MeridionalGeometry& geom,
                  const PumpParams& params,
                  const CancelPredicate& isCancelled) noexcept
{
  auto checkCancel = [&]() -> bool { return isCancelled && isCancelled(); };

  // 1. Resample hub/shroud to equal arc-length with nh points.
  //    Extend each curve by a small margin past the outlet so the FEM
  //    domain does not terminate exactly on r = d2/2, then clip the
  //    derived results (streamlines, midline) back to r = d2/2 before
  //    returning them for display.
  constexpr double OUTLET_EXTENSION_MM = 3.0;
  auto hubExtended = geom.hubCurve;
  auto shroudExtended = geom.shroudCurve;
  extendPolylineAtEnd(hubExtended, OUTLET_EXTENSION_MM);
  extendPolylineAtEnd(shroudExtended, OUTLET_EXTENSION_MM);

  auto hubResampled = math::resampleArcLength(hubExtended, config_.nh);
  auto shroudResampled = math::resampleArcLength(shroudExtended, config_.nh);

  if (hubResampled.empty() || shroudResampled.empty()) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  const double rMax = params.d2 / 2.0;

  if (checkCancel()) {
    return std::unexpected(CoreError::Cancelled);
  }

  // 2. Build strip grid
  auto gridResult = buildStripGrid(hubResampled, shroudResampled, config_.m);
  if (!gridResult) {
    return std::unexpected(gridResult.error());
  }

  if (checkCancel()) {
    return std::unexpected(CoreError::Cancelled);
  }

  // 3. Solve FEM
  auto femResult = solveFem(std::move(*gridResult));
  if (!femResult) {
    return std::unexpected(femResult.error());
  }

  if (checkCancel()) {
    return std::unexpected(CoreError::Cancelled);
  }

  // 4. Extract streamlines and clip each at r <= d2/2.
  auto levels = equidistantLevels(config_.streamlineCount);
  auto streamlines = extractStreamlines(*femResult, levels);
  for (auto& line : streamlines) {
    clipPolylineAtRMax(line.points, rMax);
  }

  if (checkCancel()) {
    return std::unexpected(CoreError::Cancelled);
  }

  // 5. Compute area profile. Use the ORIGINAL (non-extended) curves so
  //    the hub/shroud polylines the area bisection sees end exactly at
  //    r = d2/2.
  auto areaResult = computeAreaProfile(*femResult, geom.hubCurve, geom.shroudCurve, params);
  if (!areaResult) {
    return std::unexpected(areaResult.error());
  }
  // Trim the midline/area arrays together so the chart's domain ends
  // cleanly at r = d2/2. Find the first row that crosses the outlet,
  // interpolate the last entry, then resize all parallel arrays to match.
  {
    auto& ap = *areaResult;
    std::size_t keep = ap.midPoints.size();
    for (std::size_t i = 0; i < ap.midPoints.size(); ++i) {
      if (ap.midPoints[i].y() > rMax) {
        keep = i;
        break;
      }
    }
    if (keep == 0) {
      ap.midPoints.clear();
      ap.chordLengths.clear();
      ap.flowAreas.clear();
      ap.arcLengths.clear();
    } else if (keep < ap.midPoints.size()) {
      // Interpolate a terminal point exactly on r = rMax, copying the
      // adjacent chord/area values (they vary slowly there).
      const auto& prev = ap.midPoints[keep - 1];
      const auto& next = ap.midPoints[keep];
      double denom = next.y() - prev.y();
      math::Vec2 clipped = prev;
      if (std::abs(denom) > 1e-12) {
        double t = (rMax - prev.y()) / denom;
        clipped = (1.0 - t) * prev + t * next;
      }
      ap.midPoints.resize(keep + 1);
      ap.midPoints[keep] = clipped;
      ap.chordLengths.resize(keep + 1);
      ap.chordLengths[keep] = ap.chordLengths[keep - 1];
      ap.flowAreas.resize(keep + 1);
      // Recompute the outlet area cleanly: 2*pi*r*chord with r=rMax.
      ap.flowAreas[keep] = 2.0 * std::numbers::pi * rMax * ap.chordLengths[keep];
      ap.arcLengths.resize(keep + 1);
      ap.arcLengths[keep] = ap.arcLengths[keep - 1] + (clipped - prev).norm();
    }
  }

  if (!areaResult->flowAreas.empty()) {
    logging::core()->debug("area F[0]={:.1f} F[1]={:.1f} F[last]={:.1f} (F1={:.1f} F2={:.1f})",
                           areaResult->flowAreas[0],
                           areaResult->flowAreas.size() > 1 ? areaResult->flowAreas[1] : 0.0,
                           areaResult->flowAreas.back(),
                           areaResult->f1,
                           areaResult->f2);
  }

  std::vector<StreamlineVelocity> velocities;
  auto velRes = computeStreamlineVelocities(*femResult, streamlines, params.qM3s);
  if (velRes) {
    velocities = std::move(*velRes);
  }
  return FlowResults{
    .solution = std::move(*femResult),
    .streamlines = std::move(streamlines),
    .velocities = std::move(velocities),
    .areaProfile = std::move(*areaResult),
  };
}

} // namespace ggm::core
