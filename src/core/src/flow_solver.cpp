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

constexpr double kGeometryLengthUnitToMeters = 1e-3;

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

void
clipPolylineAtRMax(std::vector<math::Vec2>& poly, double rMax)
{
  if (poly.empty()) {
    return;
  }

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

  if (first > 0) {
    out.push_back(interpR(
      poly[static_cast<std::size_t>(first - 1)], poly[static_cast<std::size_t>(first)], rMax));
  }
  for (int i = first; i <= last; ++i) {
    out.push_back(poly[static_cast<std::size_t>(i)]);
  }

  if (last + 1 < static_cast<int>(poly.size())) {
    out.push_back(interpR(
      poly[static_cast<std::size_t>(last)], poly[static_cast<std::size_t>(last + 1)], rMax));
  }

  poly = std::move(out);
}

}

Result<FlowResults>
FlowSolver::solve(const MeridionalGeometry& geom,
                  const PumpParams& params,
                  const CancelPredicate& isCancelled) noexcept
{
  auto checkCancel = [&]() -> bool { return isCancelled && isCancelled(); };

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

  auto gridResult = buildStripGrid(hubResampled, shroudResampled, config_.m);
  if (!gridResult) {
    return std::unexpected(gridResult.error());
  }

  if (checkCancel()) {
    return std::unexpected(CoreError::Cancelled);
  }

  auto femResult = solveFem(std::move(*gridResult));
  if (!femResult) {
    return std::unexpected(femResult.error());
  }

  if (checkCancel()) {
    return std::unexpected(CoreError::Cancelled);
  }

  auto levels = equidistantLevels(config_.streamlineCount);
  auto streamlines = extractStreamlines(*femResult, levels);
  for (auto& line : streamlines) {
    clipPolylineAtRMax(line.points, rMax);
  }

  if (checkCancel()) {
    return std::unexpected(CoreError::Cancelled);
  }

  auto areaResult = computeAreaProfile(*femResult, geom.hubCurve, geom.shroudCurve, params);
  if (!areaResult) {
    return std::unexpected(areaResult.error());
  }

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
  auto velRes =
    computeStreamlineVelocities(*femResult, streamlines, params.qM3s, kGeometryLengthUnitToMeters);
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

}
