#include "core/geometry_optimizer.hpp"

#include "core/area_profile.hpp"
#include "core/fem_solver.hpp"
#include "core/strip_grid.hpp"
#include "math/arc_length.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <random>
#include <span>

#include <Eigen/Dense>
#include <utility>
#include <vector>

namespace ggm::core {

namespace {

constexpr double kLargePenalty = 1e30;
constexpr double kMinimumPositive = 1e-6;
constexpr int kObjectiveNh = 160;
constexpr int kObjectiveM = 48;
constexpr double kConstraintTolerance = 1e-6;

constexpr auto kAllDesignVariables = std::to_array<DesignVariable>({
  { "din",
    &PumpParams::din,
    &GeometryDesignBounds::dinMin,
    &GeometryDesignBounds::dinMax,
    &GeometryVariableMask::din },
  { "b2",
    &PumpParams::b2,
    &GeometryDesignBounds::b2Min,
    &GeometryDesignBounds::b2Max,
    &GeometryVariableMask::b2 },
  { "r1",
    &PumpParams::r1,
    &GeometryDesignBounds::r1Min,
    &GeometryDesignBounds::r1Max,
    &GeometryVariableMask::r1 },
  { "r2",
    &PumpParams::r2,
    &GeometryDesignBounds::r2Min,
    &GeometryDesignBounds::r2Max,
    &GeometryVariableMask::r2 },
  { "r3",
    &PumpParams::r3,
    &GeometryDesignBounds::r3Min,
    &GeometryDesignBounds::r3Max,
    &GeometryVariableMask::r3 },
  { "r4",
    &PumpParams::r4,
    &GeometryDesignBounds::r4Min,
    &GeometryDesignBounds::r4Max,
    &GeometryVariableMask::r4 },
  { "al1",
    &PumpParams::al1Deg,
    &GeometryDesignBounds::al1DegMin,
    &GeometryDesignBounds::al1DegMax,
    &GeometryVariableMask::al1 },
  { "al2",
    &PumpParams::al2Deg,
    &GeometryDesignBounds::al2DegMin,
    &GeometryDesignBounds::al2DegMax,
    &GeometryVariableMask::al2 },
  { "al02",
    &PumpParams::al02Deg,
    &GeometryDesignBounds::al02DegMin,
    &GeometryDesignBounds::al02DegMax,
    &GeometryVariableMask::al02 },
  { "be1",
    &PumpParams::be1Deg,
    &GeometryDesignBounds::be1DegMin,
    &GeometryDesignBounds::be1DegMax,
    &GeometryVariableMask::be1 },
  { "be3",
    &PumpParams::be3RawDeg,
    &GeometryDesignBounds::be3RawDegMin,
    &GeometryDesignBounds::be3RawDegMax,
    &GeometryVariableMask::be3 },
});

struct DesignVector
{
  std::vector<double> x;
};

struct Candidate
{
  DesignVector design;
  GeometryObjectiveBreakdown objective;
};

struct EvaluationArtifacts
{
  MeridionalGeometry geometry;
  AreaProfile areaProfile;
};

struct NormalizedAreaInterpolator
{
  std::vector<double> xiNodes;
  std::vector<double> normalizedAreas;
  double outletArea{ 0.0 };
};

struct ObjectiveSample
{
  double xi{ 0.0 };
  double weight{ 1.0 };
};

struct AreaObjectiveComponents
{
  std::vector<double> weightedResiduals;
  double areaError{ 0.0 };
  double residualSlopePenalty{ 0.0 };
  double monotonicityPenalty{ 0.0 };
};

} // namespace

std::span<const DesignVariable>
allDesignVariables() noexcept
{
  return { kAllDesignVariables.data(), kAllDesignVariables.size() };
}

namespace {

// Returns only the variables whose enable-flag in the mask is true.
// Returns a small scratch vector — allocations here are off the hot path
// (once per optimize call, not per iteration).
[[nodiscard]] std::vector<DesignVariable>
enabledVariables(const GeometryVariableMask& mask)
{
  std::vector<DesignVariable> result;
  result.reserve(kAllDesignVariables.size());
  for (const auto& var : kAllDesignVariables) {
    if (mask.*(var.enabled)) {
      result.push_back(var);
    }
  }
  return result;
}

} // namespace

namespace {

[[nodiscard]] double
safePositive(double value, double fallback) noexcept
{
  if (!std::isfinite(value) || value <= kMinimumPositive) {
    return fallback;
  }
  return value;
}

void
ensureAscending(double& minValue, double& maxValue) noexcept
{
  if (!std::isfinite(minValue)) {
    minValue = 0.0;
  }
  if (!std::isfinite(maxValue)) {
    maxValue = minValue + 1.0;
  }
  if (maxValue <= minValue) {
    maxValue = minValue + std::max(std::abs(minValue) * 0.1, 1.0);
  }
}

[[nodiscard]] bool
isFiniteParams(const PumpParams& params) noexcept
{
  const auto values = std::to_array<double>({
    params.xa,
    params.dvt,
    params.d2,
    params.r1,
    params.r2,
    params.r3,
    params.r4,
    params.al1Deg,
    params.al2Deg,
    params.al02Deg,
    params.be1Deg,
    params.be3RawDeg,
    params.b2,
    params.din,
    params.qM3s,
  });

  return std::all_of(
    values.begin(), values.end(), [](double value) { return std::isfinite(value); });
}

[[nodiscard]] bool
isSettingsValid(const GeometryOptimizationSettings& settings) noexcept
{
  return settings.sampleCount >= 2 && settings.maxGenerations >= 1 &&
         std::isfinite(settings.sigmaInitial) && settings.sigmaInitial > 0.0 &&
         settings.populationSize >= 0 && std::isfinite(settings.areaWeight) &&
         settings.areaWeight >= 0.0 &&
         std::isfinite(settings.residualSlopeWeight) && settings.residualSlopeWeight >= 0.0 &&
         std::isfinite(settings.monotonicityWeight) && settings.monotonicityWeight >= 0.0 &&
         std::isfinite(settings.smoothnessWeight) && settings.smoothnessWeight >= 0.0 &&
         std::isfinite(settings.constraintWeight) && settings.constraintWeight >= 0.0 &&
         std::isfinite(settings.targetPointWeight) && settings.targetPointWeight >= 0.0 &&
         settings.localPolishIterations >= 0 &&
         std::isfinite(settings.maxInvalidChordFraction) &&
         settings.maxInvalidChordFraction >= 0.0 && settings.maxInvalidChordFraction <= 1.0;
}

[[nodiscard]] bool
areBoundsValid(const GeometryDesignBounds& bounds,
               std::span<const DesignVariable> members) noexcept
{
  for (const auto& member : members) {
    const double minValue = bounds.*(member.minBound);
    const double maxValue = bounds.*(member.maxBound);
    if (!std::isfinite(minValue) || !std::isfinite(maxValue) || minValue >= maxValue) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] double
clampToBounds(double value, double minValue, double maxValue) noexcept
{
  return std::clamp(value, minValue, maxValue);
}

[[nodiscard]] DesignVector
encodeParamsToVector(const PumpParams& params,
                     const GeometryDesignBounds& bounds,
                     std::span<const DesignVariable> members)
{
  DesignVector vector;
  vector.x.resize(members.size(), 0.0);

  for (std::size_t i = 0; i < members.size(); ++i) {
    const auto& member = members[i];
    const double minValue = bounds.*(member.minBound);
    const double maxValue = bounds.*(member.maxBound);
    vector.x[i] = clampToBounds(params.*(member.param), minValue, maxValue);
  }

  return vector;
}

[[nodiscard]] PumpParams
decodeVectorToParams(const DesignVector& vector,
                     const PumpParams& baseParams,
                     const GeometryDesignBounds& bounds,
                     std::span<const DesignVariable> members)
{
  PumpParams params = baseParams;
  const auto count = std::min(members.size(), vector.x.size());

  for (std::size_t i = 0; i < count; ++i) {
    const auto& member = members[i];
    const double minValue = bounds.*(member.minBound);
    const double maxValue = bounds.*(member.maxBound);
    params.*(member.param) = clampToBounds(vector.x[i], minValue, maxValue);
  }

  params.d2 = baseParams.d2;
  params.dvt = baseParams.dvt;

  // Hard constraint: din > dvt. Enforce even if the user dragged dinMin
  // below dvt in the bounds editor — the geometry builder produces garbage
  // otherwise.
  const double dinFloor = params.dvt * 1.001;
  if (params.din < dinFloor) {
    params.din = dinFloor;
  }

  return params;
}

[[nodiscard]] Result<EvaluationArtifacts>
buildEvaluationArtifacts(const PumpParams& params)
{
  auto geometryResult = buildGeometry(params);
  if (!geometryResult) {
    return std::unexpected(geometryResult.error());
  }

  auto hubResampled = math::resampleArcLength(geometryResult->hubCurve, kObjectiveNh);
  auto shroudResampled = math::resampleArcLength(geometryResult->shroudCurve, kObjectiveNh);
  if (hubResampled.size() < 2U || shroudResampled.size() < 2U) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  auto gridResult = buildStripGrid(hubResampled, shroudResampled, kObjectiveM);
  if (!gridResult) {
    return std::unexpected(gridResult.error());
  }

  // There is no separate "fast profile without FEM" path in the project yet,
  // so candidate validation always runs through the same FEM + area-profile
  // pipeline as the production solver.
  auto flowResult = solveFem(std::move(*gridResult));
  if (!flowResult) {
    return std::unexpected(flowResult.error());
  }

  auto areaResult =
    computeAreaProfile(*flowResult, geometryResult->hubCurve, geometryResult->shroudCurve, params);
  if (!areaResult) {
    return std::unexpected(areaResult.error());
  }

  return EvaluationArtifacts{
    .geometry = std::move(*geometryResult),
    .areaProfile = std::move(*areaResult),
  };
}

[[nodiscard]] double
curveSmoothnessPenalty(std::span<const math::Vec2> curve, const double lengthScale)
{
  if (curve.size() < 3U || !std::isfinite(lengthScale) || lengthScale <= kMinimumPositive) {
    return 0.0;
  }

  double penalty = 0.0;
  std::size_t count = 0U;

  for (std::size_t i = 1; i + 1U < curve.size(); ++i) {
    const math::Vec2 secondDifference = curve[i + 1U] - (2.0 * curve[i]) + curve[i - 1U];
    penalty += secondDifference.squaredNorm();
    ++count;
  }

  if (count == 0U) {
    return 0.0;
  }

  return penalty / (static_cast<double>(count) * lengthScale * lengthScale);
}

[[nodiscard]] double
evaluateSmoothnessPenalty(const MeridionalGeometry& geometry, const PumpParams& params)
{
  const double lengthScale = std::max(params.d2, kMinimumPositive);
  return curveSmoothnessPenalty(geometry.hubCurve, lengthScale) +
         curveSmoothnessPenalty(geometry.shroudCurve, lengthScale);
}

[[nodiscard]] double
evaluateConstraintPenalty(const MeridionalGeometry& geometry,
                          const AreaProfile& areaProfile,
                          const PumpParams& params,
                          const GeometryOptimizationSettings& settings)
{
  const double radiusMin = params.dvt * 0.5;
  const double radiusMax = params.d2 * 0.5;
  const double radialSpan = std::max(radiusMax - radiusMin, kMinimumPositive);
  const double minGap = std::max(0.01 * radialSpan, 0.1);
  double penalty = 0.0;

  const auto accumulateViolation = [](double& bucket, double violation, double scale) {
    if (violation > 0.0 && std::isfinite(violation) && std::isfinite(scale) && scale > 0.0) {
      const double normalized = violation / scale;
      bucket += normalized * normalized;
    }
  };

  const auto pairCount = std::min(geometry.hubCurve.size(), geometry.shroudCurve.size());
  if (pairCount < 2U) {
    return kLargePenalty;
  }

  double wallPenalty = 0.0;
  for (std::size_t i = 0; i < pairCount; ++i) {
    const auto& hub = geometry.hubCurve[i];
    const auto& shroud = geometry.shroudCurve[i];

    if (!std::isfinite(hub.x()) || !std::isfinite(hub.y()) || !std::isfinite(shroud.x()) ||
        !std::isfinite(shroud.y())) {
      return kLargePenalty;
    }

    accumulateViolation(wallPenalty, (radiusMin - kConstraintTolerance) - hub.y(), radialSpan);
    accumulateViolation(wallPenalty, shroud.y() - (radiusMax + kConstraintTolerance), radialSpan);
    accumulateViolation(wallPenalty, minGap - (shroud.y() - hub.y()), radialSpan);
  }
  penalty += wallPenalty / static_cast<double>(pairCount);

  if (areaProfile.flowAreas.size() != areaProfile.arcLengths.size() ||
      areaProfile.chordLengths.size() != areaProfile.flowAreas.size() ||
      areaProfile.midPoints.size() != areaProfile.flowAreas.size()) {
    return kLargePenalty;
  }

  double profilePenalty = 0.0;
  std::size_t invalidChordCount = 0U;
  for (std::size_t i = 0; i < areaProfile.flowAreas.size(); ++i) {
    const double area = areaProfile.flowAreas[i];
    const double arcLength = areaProfile.arcLengths[i];
    const double chord = areaProfile.chordLengths[i];
    const auto& midPoint = areaProfile.midPoints[i];

    if (!std::isfinite(area) || !std::isfinite(arcLength) || !std::isfinite(chord) ||
        !std::isfinite(midPoint.x()) || !std::isfinite(midPoint.y())) {
      return kLargePenalty;
    }

    if (area <= 0.0) {
      profilePenalty += 10.0;
    }

    if (chord <= 0.0) {
      ++invalidChordCount;
      profilePenalty += 5.0;
    }

    if (midPoint.y() < radiusMin - kConstraintTolerance ||
        midPoint.y() > radiusMax + kConstraintTolerance) {
      profilePenalty += 5.0;
    }

    if (i > 0U && areaProfile.arcLengths[i] < areaProfile.arcLengths[i - 1U]) {
      profilePenalty += 20.0;
    }
  }

  if (!areaProfile.flowAreas.empty()) {
    penalty += profilePenalty / static_cast<double>(areaProfile.flowAreas.size());
  }

  if (!areaProfile.chordLengths.empty()) {
    const double invalidFraction =
      static_cast<double>(invalidChordCount) / static_cast<double>(areaProfile.chordLengths.size());
    if (invalidFraction > settings.maxInvalidChordFraction) {
      const double excess = invalidFraction - settings.maxInvalidChordFraction;
      penalty += 100.0 * excess * excess;
    }
  }

  return penalty;
}

[[nodiscard]] GeometryObjectiveBreakdown
makeFailureObjective() noexcept
{
  return GeometryObjectiveBreakdown{
    .total = kLargePenalty,
    .areaError = 0.0,
    .residualSlopePenalty = 0.0,
    .monotonicityPenalty = 0.0,
    .smoothnessPenalty = 0.0,
    .constraintPenalty = kLargePenalty,
    .valid = false,
  };
}

} // namespace

GeometryDesignBounds
makeBoundsFromValues(const PumpParams& params) noexcept
{
  // ±50 % of each current value. `halfWidth` falls back to 1 for near-zero
  // params so the interval is non-degenerate.
  const auto interval = [](double value) {
    const double halfWidth = std::max(std::abs(value) * 0.5, 1.0);
    return std::pair<double, double>{ value - halfWidth, value + halfWidth };
  };

  GeometryDesignBounds b{};
  std::tie(b.dinMin, b.dinMax) = interval(params.din);
  // Hard geometric constraint: din must stay strictly greater than dvt.
  const double dinFloor = params.dvt * 1.001;
  b.dinMin = std::max(b.dinMin, dinFloor);
  if (b.dinMax <= b.dinMin) {
    b.dinMax = b.dinMin + std::max(std::abs(b.dinMin) * 0.1, 1.0);
  }
  std::tie(b.b2Min, b.b2Max) = interval(params.b2);
  std::tie(b.xaMin, b.xaMax) = interval(params.xa);
  std::tie(b.r1Min, b.r1Max) = interval(params.r1);
  std::tie(b.r2Min, b.r2Max) = interval(params.r2);
  std::tie(b.r3Min, b.r3Max) = interval(params.r3);
  std::tie(b.r4Min, b.r4Max) = interval(params.r4);
  std::tie(b.al1DegMin, b.al1DegMax) = interval(params.al1Deg);
  std::tie(b.al2DegMin, b.al2DegMax) = interval(params.al2Deg);
  std::tie(b.al02DegMin, b.al02DegMax) = interval(params.al02Deg);
  std::tie(b.be1DegMin, b.be1DegMax) = interval(params.be1Deg);
  std::tie(b.be3RawDegMin, b.be3RawDegMax) = interval(params.be3RawDeg);
  return b;
}

GeometryDesignBounds
makeDefaultBounds(double d2, double dvt)
{
  const double safeD2 = safePositive(d2, 100.0);
  double safeDvt = safePositive(dvt, safeD2 * 0.4);
  if (safeDvt >= safeD2) {
    safeDvt = safeD2 * 0.5;
  }

  const double radiusMin = safeDvt * 0.5;
  const double radiusMax = safeD2 * 0.5;
  const double radialSpan = std::max(radiusMax - radiusMin, safeD2 * 0.1);

  GeometryDesignBounds bounds;
  bounds.dinMin = safeDvt * 1.05;
  bounds.dinMax = safeD2 * 0.98;

  bounds.b2Min = 0.02 * safeD2;
  bounds.b2Max = 0.30 * safeD2;

  bounds.xaMin = 0.02 * safeD2;
  bounds.xaMax = 0.80 * safeD2;

  bounds.r1Min = 0.01 * safeD2;
  bounds.r1Max = 2.00 * safeD2;

  bounds.r2Min = 0.01 * safeD2;
  bounds.r2Max = 2.00 * safeD2;

  bounds.r3Min = 0.01 * safeD2;
  bounds.r3Max = 2.00 * safeD2;

  bounds.r4Min = 0.01 * safeD2;
  bounds.r4Max = 2.00 * safeD2;

  bounds.al1DegMin = -30.0;
  bounds.al1DegMax = 30.0;

  bounds.al2DegMin = -30.0;
  bounds.al2DegMax = 30.0;

  bounds.al02DegMin = -30.0;
  bounds.al02DegMax = 30.0;

  bounds.be1DegMin = 5.0;
  bounds.be1DegMax = 85.0;

  bounds.be3RawDegMin = 5.0;
  bounds.be3RawDegMax = 85.0;

  ensureAscending(bounds.dinMin, bounds.dinMax);
  ensureAscending(bounds.b2Min, bounds.b2Max);
  ensureAscending(bounds.xaMin, bounds.xaMax);
  ensureAscending(bounds.r1Min, bounds.r1Max);
  ensureAscending(bounds.r2Min, bounds.r2Max);
  ensureAscending(bounds.r3Min, bounds.r3Max);
  ensureAscending(bounds.r4Min, bounds.r4Max);
  ensureAscending(bounds.al1DegMin, bounds.al1DegMax);
  ensureAscending(bounds.al2DegMin, bounds.al2DegMax);
  ensureAscending(bounds.al02DegMin, bounds.al02DegMax);
  ensureAscending(bounds.be1DegMin, bounds.be1DegMax);
  ensureAscending(bounds.be3RawDegMin, bounds.be3RawDegMax);

  bounds.b2Min = std::max(bounds.b2Min, 0.02 * radialSpan);
  bounds.xaMin = std::max(bounds.xaMin, 0.02 * radialSpan);

  return bounds;
}

namespace {

[[nodiscard]] Result<NormalizedAreaInterpolator>
makeNormalizedAreaInterpolator(const AreaProfile& profile)
{
  if (profile.flowAreas.size() < 2U || profile.arcLengths.size() < 2U ||
      profile.flowAreas.size() != profile.arcLengths.size()) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  const double finalArea = profile.flowAreas.back();
  const double finalArcLength = profile.arcLengths.back();
  if (!std::isfinite(finalArea) || !std::isfinite(finalArcLength) || finalArea <= 0.0 ||
      finalArcLength <= 0.0) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  NormalizedAreaInterpolator interpolator;
  interpolator.outletArea = finalArea;
  interpolator.normalizedAreas.resize(profile.flowAreas.size(), 0.0);
  interpolator.xiNodes.resize(profile.arcLengths.size(), 0.0);

  for (std::size_t i = 0; i < profile.flowAreas.size(); ++i) {
    const double area = profile.flowAreas[i];
    const double arcLength = profile.arcLengths[i];
    if (!std::isfinite(area) || !std::isfinite(arcLength) || area <= 0.0 || arcLength < 0.0) {
      return std::unexpected(CoreError::InvalidParameter);
    }
    if (i > 0U && arcLength < profile.arcLengths[i - 1U]) {
      return std::unexpected(CoreError::InvalidParameter);
    }

    interpolator.normalizedAreas[i] = area / finalArea;
    interpolator.xiNodes[i] = arcLength / finalArcLength;
  }

  return interpolator;
}

[[nodiscard]] double
evaluateNormalizedAreaAtXi(const NormalizedAreaInterpolator& interpolator, double xi) noexcept
{
  if (interpolator.xiNodes.size() < 2U ||
      interpolator.xiNodes.size() != interpolator.normalizedAreas.size()) {
    return 0.0;
  }

  const double clampedXi = std::clamp(std::isfinite(xi) ? xi : 0.0, 0.0, 1.0);
  auto segmentIndex = std::size_t{ 0U };
  while (segmentIndex + 1U < interpolator.xiNodes.size() - 1U &&
         interpolator.xiNodes[segmentIndex + 1U] < clampedXi) {
    ++segmentIndex;
  }

  const double leftXi = interpolator.xiNodes[segmentIndex];
  const double rightXi = interpolator.xiNodes[segmentIndex + 1U];
  const double leftArea = interpolator.normalizedAreas[segmentIndex];
  const double rightArea = interpolator.normalizedAreas[segmentIndex + 1U];
  const double dx = rightXi - leftXi;

  if (std::abs(dx) <= kMinimumPositive) {
    return rightArea;
  }

  const double t = std::clamp((clampedXi - leftXi) / dx, 0.0, 1.0);
  return ((1.0 - t) * leftArea) + (t * rightArea);
}

[[nodiscard]] std::vector<ObjectiveSample>
makeObjectiveSamples(const TargetAreaCurve& target, const GeometryOptimizationSettings& settings)
{
  std::vector<ObjectiveSample> samples;
  samples.reserve(static_cast<std::size_t>(settings.sampleCount) + target.points().size());

  for (int sampleIndex = 0; sampleIndex < settings.sampleCount; ++sampleIndex) {
    const double xi =
      static_cast<double>(sampleIndex) / static_cast<double>(settings.sampleCount - 1);
    samples.push_back(ObjectiveSample{ .xi = xi, .weight = 1.0 });
  }

  if (settings.targetPointWeight > 0.0) {
    for (const auto& point : target.points()) {
      if (std::isfinite(point.xi)) {
        samples.push_back(ObjectiveSample{
          .xi = std::clamp(point.xi, 0.0, 1.0),
          .weight = settings.targetPointWeight,
        });
      }
    }
  }

  std::sort(samples.begin(),
            samples.end(),
            [](const ObjectiveSample& lhs, const ObjectiveSample& rhs) {
              return lhs.xi < rhs.xi;
            });

  std::vector<ObjectiveSample> merged;
  merged.reserve(samples.size());
  for (const auto& sample : samples) {
    if (!std::isfinite(sample.weight) || sample.weight <= 0.0) {
      continue;
    }
    if (!merged.empty() && std::abs(merged.back().xi - sample.xi) <= kMinimumPositive) {
      merged.back().weight += sample.weight;
      continue;
    }
    merged.push_back(sample);
  }

  return merged;
}

[[nodiscard]] AreaObjectiveComponents
evaluateAreaObjectiveComponents(const NormalizedAreaInterpolator& interpolator,
                                const TargetAreaCurve& target,
                                const GeometryOptimizationSettings& settings)
{
  AreaObjectiveComponents components;
  const auto samples = makeObjectiveSamples(target, settings);
  if (samples.empty()) {
    return components;
  }

  const bool useAbsoluteAnchor =
    settings.referenceOutletArea > 0.0 && std::isfinite(settings.referenceOutletArea);
  const double currentScale =
    useAbsoluteAnchor ? interpolator.outletArea / settings.referenceOutletArea : 1.0;

  std::vector<double> currentValues(samples.size(), 0.0);
  std::vector<double> targetValues(samples.size(), 0.0);

  double totalWeight = 0.0;
  double weightedAreaError = 0.0;
  for (std::size_t i = 0; i < samples.size(); ++i) {
    currentValues[i] = evaluateNormalizedAreaAtXi(interpolator, samples[i].xi) * currentScale;
    targetValues[i] = target.evaluate(samples[i].xi);
    const double diff = currentValues[i] - targetValues[i];

    totalWeight += samples[i].weight;
    weightedAreaError += samples[i].weight * diff * diff;
  }

  if (totalWeight > 0.0) {
    components.areaError = weightedAreaError / totalWeight;
    if (settings.areaWeight > 0.0) {
      const double scale = std::sqrt(settings.areaWeight / totalWeight);
      for (std::size_t i = 0; i < samples.size(); ++i) {
        components.weightedResiduals.push_back(
          std::sqrt(samples[i].weight) * scale * (currentValues[i] - targetValues[i]));
      }
    }
  }

  std::vector<double> slopeDiffs;
  std::vector<double> slopeWeights;
  std::vector<double> monotonicityViolations;
  std::vector<double> monotonicityWeights;
  slopeDiffs.reserve(samples.size());
  slopeWeights.reserve(samples.size());
  monotonicityViolations.reserve(samples.size());
  monotonicityWeights.reserve(samples.size());

  for (std::size_t i = 0; i + 1U < samples.size(); ++i) {
    const double dx = samples[i + 1U].xi - samples[i].xi;
    if (dx <= kMinimumPositive) {
      continue;
    }

    const double currentDelta = currentValues[i + 1U] - currentValues[i];
    const double targetDelta = targetValues[i + 1U] - targetValues[i];
    const double segmentWeight = 0.5 * (samples[i].weight + samples[i + 1U].weight);

    slopeDiffs.push_back(currentDelta - targetDelta);
    slopeWeights.push_back(segmentWeight);

    if (targetDelta >= -kMinimumPositive) {
      const double drop = currentValues[i] - currentValues[i + 1U];
      if (drop > 0.0) {
        monotonicityViolations.push_back(drop);
        monotonicityWeights.push_back(segmentWeight);
      }
    }
  }

  const auto appendWeightedResiduals = [&](std::span<const double> values,
                                           std::span<const double> weights,
                                           double objectiveWeight,
                                           double& penalty) {
    const double totalLocalWeight = std::accumulate(weights.begin(), weights.end(), 0.0);
    if (values.empty() || totalLocalWeight <= 0.0) {
      return;
    }

    double weightedPenalty = 0.0;
    for (std::size_t i = 0; i < values.size(); ++i) {
      weightedPenalty += weights[i] * values[i] * values[i];
    }
    penalty = weightedPenalty / totalLocalWeight;

    if (objectiveWeight <= 0.0) {
      return;
    }
    const double scale = std::sqrt(objectiveWeight / totalLocalWeight);
    for (std::size_t i = 0; i < values.size(); ++i) {
      components.weightedResiduals.push_back(std::sqrt(weights[i]) * scale * values[i]);
    }
  };

  appendWeightedResiduals(slopeDiffs,
                          slopeWeights,
                          settings.residualSlopeWeight,
                          components.residualSlopePenalty);
  appendWeightedResiduals(monotonicityViolations,
                          monotonicityWeights,
                          settings.monotonicityWeight,
                          components.monotonicityPenalty);

  return components;
}

[[nodiscard]] GeometryObjectiveBreakdown
evaluateObjectiveFromArtifacts(const EvaluationArtifacts& artifacts,
                               const PumpParams& params,
                               const TargetAreaCurve& target,
                               const GeometryOptimizationSettings& settings,
                               std::vector<double>* weightedResiduals)
{
  auto interpolatorResult = makeNormalizedAreaInterpolator(artifacts.areaProfile);
  if (!interpolatorResult) {
    return makeFailureObjective();
  }

  auto areaComponents = evaluateAreaObjectiveComponents(*interpolatorResult, target, settings);
  const double smoothnessPenalty = evaluateSmoothnessPenalty(artifacts.geometry, params);
  const double constraintPenalty =
    evaluateConstraintPenalty(artifacts.geometry, artifacts.areaProfile, params, settings);

  if (!std::isfinite(areaComponents.areaError) ||
      !std::isfinite(areaComponents.residualSlopePenalty) ||
      !std::isfinite(areaComponents.monotonicityPenalty) || !std::isfinite(smoothnessPenalty) ||
      !std::isfinite(constraintPenalty)) {
    return makeFailureObjective();
  }

  const double total = (settings.areaWeight * areaComponents.areaError) +
                       (settings.residualSlopeWeight * areaComponents.residualSlopePenalty) +
                       (settings.monotonicityWeight * areaComponents.monotonicityPenalty) +
                       (settings.smoothnessWeight * smoothnessPenalty) +
                       (settings.constraintWeight * constraintPenalty);

  if (!std::isfinite(total)) {
    return makeFailureObjective();
  }

  if (weightedResiduals != nullptr) {
    *weightedResiduals = std::move(areaComponents.weightedResiduals);
  }

  return GeometryObjectiveBreakdown{
    .total = total,
    .areaError = areaComponents.areaError,
    .residualSlopePenalty = areaComponents.residualSlopePenalty,
    .monotonicityPenalty = areaComponents.monotonicityPenalty,
    .smoothnessPenalty = smoothnessPenalty,
    .constraintPenalty = constraintPenalty,
    .valid = true,
  };
}

} // namespace

Result<std::vector<double>>
normalizedAreaSamples(const AreaProfile& profile, const int sampleCount)
{
  if (sampleCount < 2) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  auto interpolatorResult = makeNormalizedAreaInterpolator(profile);
  if (!interpolatorResult) {
    return std::unexpected(interpolatorResult.error());
  }

  std::vector<double> samples(static_cast<std::size_t>(sampleCount), 0.0);
  for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
    const double xi = static_cast<double>(sampleIndex) / static_cast<double>(sampleCount - 1);
    samples[static_cast<std::size_t>(sampleIndex)] =
      evaluateNormalizedAreaAtXi(*interpolatorResult, xi);
  }

  return samples;
}

GeometryObjectiveBreakdown
evaluateGeometryObjective(const PumpParams& params,
                          const TargetAreaCurve& target,
                          const GeometryOptimizationSettings& settings)
{
  if (!isFiniteParams(params) || params.d2 <= params.dvt || params.dvt <= 0.0 || params.d2 <= 0.0 ||
      !target.isValid() || !isSettingsValid(settings)) {
    return makeFailureObjective();
  }

  auto artifactsResult = buildEvaluationArtifacts(params);
  if (!artifactsResult) {
    return makeFailureObjective();
  }

  return evaluateObjectiveFromArtifacts(*artifactsResult, params, target, settings, nullptr);
}

Result<GeometryOptimizationResult>
evaluateGeometryCandidate(const PumpParams& params,
                          const TargetAreaCurve& target,
                          const GeometryOptimizationSettings& settings)
{
  const auto objective = evaluateGeometryObjective(params, target, settings);
  if (!objective.valid) {
    return std::unexpected(CoreError::GeometryBuildFailed);
  }

  auto artifactsResult = buildEvaluationArtifacts(params);
  if (!artifactsResult) {
    return std::unexpected(artifactsResult.error());
  }

  return GeometryOptimizationResult{
    .params = params,
    .geometry = std::move(artifactsResult->geometry),
    .areaProfile = std::move(artifactsResult->areaProfile),
    .objective = objective,
    .converged = true,
    .generations = 0,
  };
}

namespace {

// CMA-ES driver state. Kept to-gether so the main function stays
// readable and the heavy linear-algebra lives in one place.
struct CmaEsState
{
  Eigen::VectorXd mean;       // current distribution mean, in normalized coords
  Eigen::MatrixXd covariance; // covariance matrix C
  Eigen::MatrixXd eigenVec;   // eigenvectors of C (B)
  Eigen::VectorXd eigenVal;   // sqrt of eigenvalues of C (D)
  Eigen::VectorXd pathSigma;  // evolution path for σ
  Eigen::VectorXd pathC;      // evolution path for C
  double sigma{ 0.0 };        // step-size

  // Strategy parameters derived from dimensionality N and population λ.
  int lambda{ 0 };
  int mu{ 0 };
  Eigen::VectorXd weights;
  double muEff{ 0.0 };
  double cSigma{ 0.0 };
  double dSigma{ 0.0 };
  double cCov{ 0.0 };    // c_c in Hansen's notation
  double c1{ 0.0 };
  double cMu{ 0.0 };
  double expectedNorm{ 0.0 }; // E[||N(0, I)||]
};

// Sets up λ, μ, weights, and CSA/C update constants from N.
CmaEsState
makeCmaEsState(int dim, int lambdaOverride)
{
  CmaEsState s;
  const double N = static_cast<double>(dim);
  s.lambda = lambdaOverride > 0
               ? lambdaOverride
               : 4 + static_cast<int>(std::floor(3.0 * std::log(std::max(N, 1.0))));
  s.lambda = std::max(s.lambda, 4);
  s.mu = s.lambda / 2;

  Eigen::VectorXd rawWeights(s.mu);
  for (int i = 0; i < s.mu; ++i) {
    rawWeights(i) = std::log(static_cast<double>(s.mu) + 1.0) - std::log(i + 1.0);
  }
  const double wSum = rawWeights.sum();
  s.weights = rawWeights / wSum;

  const double wSq = s.weights.squaredNorm();
  s.muEff = 1.0 / wSq;

  s.cSigma = (s.muEff + 2.0) / (N + s.muEff + 5.0);
  s.dSigma =
    1.0 + (2.0 * std::max(0.0, std::sqrt((s.muEff - 1.0) / (N + 1.0)) - 1.0)) + s.cSigma;
  s.cCov = (4.0 + s.muEff / N) / (N + 4.0 + 2.0 * s.muEff / N);
  s.c1 = 2.0 / (((N + 1.3) * (N + 1.3)) + s.muEff);
  const double rankMu = 2.0 * (s.muEff - 2.0 + 1.0 / s.muEff) / (((N + 2.0) * (N + 2.0)) + s.muEff);
  s.cMu = std::min(1.0 - s.c1, rankMu);

  s.expectedNorm = std::sqrt(N) * (1.0 - (1.0 / (4.0 * N)) + 1.0 / (21.0 * N * N));

  s.covariance = Eigen::MatrixXd::Identity(dim, dim);
  s.eigenVec = Eigen::MatrixXd::Identity(dim, dim);
  s.eigenVal = Eigen::VectorXd::Ones(dim);
  s.pathSigma = Eigen::VectorXd::Zero(dim);
  s.pathC = Eigen::VectorXd::Zero(dim);

  return s;
}

// Refresh B, D from the current C. Called periodically (not every gen) to
// amortize the O(N³) eigendecomposition.
void
refreshEigenDecomposition(CmaEsState& s)
{
  // Symmetrize to fight accumulated floating-point drift.
  s.covariance = 0.5 * (s.covariance + s.covariance.transpose());

  Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(s.covariance);
  if (solver.info() != Eigen::Success) {
    // Safeguard: fall back to identity.
    const int n = static_cast<int>(s.covariance.rows());
    s.covariance = Eigen::MatrixXd::Identity(n, n);
    s.eigenVec = Eigen::MatrixXd::Identity(n, n);
    s.eigenVal = Eigen::VectorXd::Ones(n);
    return;
  }

  s.eigenVec = solver.eigenvectors();
  Eigen::VectorXd values = solver.eigenvalues();
  // Clamp tiny / negative eigenvalues to keep the sqrt well-defined.
  for (int i = 0; i < values.size(); ++i) {
    values(i) = std::max(values(i), 1e-20);
  }
  s.eigenVal = values.array().sqrt();
}

} // namespace

Result<GeometryOptimizationResult>
optimizeGeometryForTargetArea(const PumpParams& initialParams,
                              const TargetAreaCurve& target,
                              const GeometryDesignBounds& bounds,
                              const GeometryOptimizationSettings& settings,
                              const CancelPredicate& isCancelled)
{
  auto cancelled = [&]() -> bool { return isCancelled && isCancelled(); };

  if (!isFiniteParams(initialParams) || initialParams.d2 <= initialParams.dvt ||
      initialParams.dvt <= 0.0 || initialParams.d2 <= 0.0 || !target.isValid() ||
      !isSettingsValid(settings)) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  const auto activeVars = enabledVariables(settings.mask);
  if (activeVars.empty()) {
    // Nothing to optimize — return the initial state as the "optimized" result.
    auto direct = evaluateGeometryCandidate(initialParams, target, settings);
    if (!direct) {
      return std::unexpected(direct.error());
    }
    direct->converged = true;
    direct->generations = 0;
    return direct;
  }

  std::span<const DesignVariable> members{ activeVars.data(), activeVars.size() };

  if (!areBoundsValid(bounds, members)) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  const int dim = static_cast<int>(members.size());

  // Normalize design space so each coordinate lives in roughly [-1, 1].
  // Midpoint maps to 0; a sampled value x_raw = midpoint + halfRange * x_norm.
  Eigen::VectorXd midpoint(dim);
  Eigen::VectorXd halfRange(dim);
  for (int i = 0; i < dim; ++i) {
    const auto idx = static_cast<std::size_t>(i);
    const double lo = bounds.*(members[idx].minBound);
    const double hi = bounds.*(members[idx].maxBound);
    midpoint(i) = 0.5 * (lo + hi);
    halfRange(i) = std::max(0.5 * (hi - lo), 1e-9);
  }

  auto denormalize = [&](const Eigen::VectorXd& xNorm) {
    DesignVector v;
    v.x.resize(static_cast<std::size_t>(dim));
    for (int i = 0; i < dim; ++i) {
      const auto idx = static_cast<std::size_t>(i);
      const double raw = midpoint(i) + halfRange(i) * xNorm(i);
      const double lo = bounds.*(members[idx].minBound);
      const double hi = bounds.*(members[idx].maxBound);
      v.x[idx] = clampToBounds(raw, lo, hi);
    }
    return v;
  };

  // Initial mean: the imported params, clamped inside bounds, then normalized.
  const DesignVector initialEncoded = encodeParamsToVector(initialParams, bounds, members);
  CmaEsState cma = makeCmaEsState(dim, settings.populationSize);
  cma.mean.resize(dim);
  for (int i = 0; i < dim; ++i) {
    cma.mean(i) = (initialEncoded.x[static_cast<std::size_t>(i)] - midpoint(i)) / halfRange(i);
  }
  cma.sigma = settings.sigmaInitial;
  refreshEigenDecomposition(cma);

  // Evaluate initial point for the "best so far" seed.
  DesignVector bestDesign = initialEncoded;
  auto bestObjective = evaluateGeometryObjective(
    decodeVectorToParams(bestDesign, initialParams, bounds, members), target, settings);
  if (!bestObjective.valid) {
    return std::unexpected(CoreError::GeometryBuildFailed);
  }

  std::mt19937 rng(settings.seed);
  std::normal_distribution<double> standardNormal(0.0, 1.0);

  // How often (in generations) to refresh B, D. Hansen's suggested heuristic:
  // recompute every 1 / (10·dim·(c1+cμ)) generations — for our N ≤ 10 this is ~1.
  const int eigenRefreshInterval =
    std::max(1, static_cast<int>(std::floor(1.0 / (10.0 * dim * (cma.c1 + cma.cMu)))));

  struct Sample
  {
    Eigen::VectorXd z;    // raw normal draw
    Eigen::VectorXd y;    // y = B·D·z
    Eigen::VectorXd x;    // x = mean + σ·y, clamped to [-1, 1]
    GeometryObjectiveBreakdown objective;
  };

  std::vector<Sample> samples(static_cast<std::size_t>(cma.lambda));

  int generation = 0;
  for (; generation < settings.maxGenerations; ++generation) {
    if (cancelled()) {
      return std::unexpected(CoreError::Cancelled);
    }

    // --- Sample λ candidates ----------------------------------------------
    for (auto& sample : samples) {
      sample.z.resize(dim);
      for (int i = 0; i < dim; ++i) {
        sample.z(i) = standardNormal(rng);
      }
      sample.y = cma.eigenVec * (cma.eigenVal.asDiagonal() * sample.z);
      sample.x = cma.mean + cma.sigma * sample.y;
      // Clamp to the normalized box [-1, 1] so decode() produces in-range params.
      for (int i = 0; i < dim; ++i) {
        sample.x(i) = std::clamp(sample.x(i), -1.0, 1.0);
      }
      // The actual candidate may be clipped by the box. Keep the CMA paths and
      // covariance update consistent with the step that was really evaluated.
      sample.y = (sample.x - cma.mean) / cma.sigma;
    }

    // --- Evaluate ---------------------------------------------------------
    for (auto& sample : samples) {
      if (cancelled()) {
        return std::unexpected(CoreError::Cancelled);
      }
      const DesignVector decodedVec = denormalize(sample.x);
      const PumpParams params = decodeVectorToParams(decodedVec, initialParams, bounds, members);
      sample.objective = evaluateGeometryObjective(params, target, settings);
      if (sample.objective.valid && sample.objective.total < bestObjective.total) {
        bestObjective = sample.objective;
        bestDesign = decodedVec;
      }
    }

    // --- Sort by fitness (ascending) --------------------------------------
    std::sort(samples.begin(), samples.end(), [](const Sample& a, const Sample& b) {
      return a.objective.total < b.objective.total;
    });

    // --- Update mean: weighted recombination of the μ best ---------------
    Eigen::VectorXd newMean = Eigen::VectorXd::Zero(dim);
    Eigen::VectorXd yMean = Eigen::VectorXd::Zero(dim);
    for (int i = 0; i < cma.mu; ++i) {
      newMean += cma.weights(i) * samples[static_cast<std::size_t>(i)].x;
      yMean += cma.weights(i) * samples[static_cast<std::size_t>(i)].y;
    }

    // C^(-1/2) · yMean = B · diag(1/D) · Bᵀ · yMean
    Eigen::VectorXd cInvSqrtY =
      cma.eigenVec *
      (cma.eigenVal.array().inverse().matrix().asDiagonal() * (cma.eigenVec.transpose() * yMean));

    // --- Evolution paths --------------------------------------------------
    cma.pathSigma =
      ((1.0 - cma.cSigma) * cma.pathSigma) +
      (std::sqrt(cma.cSigma * (2.0 - cma.cSigma) * cma.muEff) * cInvSqrtY);

    const double psNormSq = cma.pathSigma.squaredNorm();
    const double hsigThreshold =
      (1.4 + (2.0 / (dim + 1))) * cma.expectedNorm *
      std::sqrt(1.0 - std::pow(1.0 - cma.cSigma, 2.0 * (generation + 1)));
    const bool hSig = std::sqrt(psNormSq) < hsigThreshold;

    cma.pathC = ((1.0 - cma.cCov) * cma.pathC) +
                ((hSig ? 1.0 : 0.0) * std::sqrt(cma.cCov * (2.0 - cma.cCov) * cma.muEff) * yMean);

    // --- Covariance update: rank-1 + rank-μ -------------------------------
    Eigen::MatrixXd rankMu = Eigen::MatrixXd::Zero(dim, dim);
    for (int i = 0; i < cma.mu; ++i) {
      const auto& yi = samples[static_cast<std::size_t>(i)].y;
      rankMu += cma.weights(i) * (yi * yi.transpose());
    }

    const double hSigCorrection = (hSig ? 0.0 : cma.cCov * (2.0 - cma.cCov));
    cma.covariance = ((1.0 - cma.c1 - cma.cMu) * cma.covariance) +
                     (cma.c1 * ((cma.pathC * cma.pathC.transpose()) +
                                (hSigCorrection * cma.covariance))) +
                     (cma.cMu * rankMu);

    // --- σ update (CSA) ---------------------------------------------------
    cma.sigma *=
      std::exp((cma.cSigma / cma.dSigma) * ((std::sqrt(psNormSq) / cma.expectedNorm) - 1.0));
    // Guardrails: σ should not explode or vanish.
    cma.sigma = std::clamp(cma.sigma, 1e-8, 10.0);

    cma.mean = newMean;

    if ((generation + 1) % eigenRefreshInterval == 0) {
      refreshEigenDecomposition(cma);
    }
  }

  struct PolishEvaluation
  {
    Eigen::VectorXd xNorm;
    DesignVector design;
    std::vector<double> residuals;
    GeometryObjectiveBreakdown objective;
  };

  auto normalizeDesign = [&](const DesignVector& design) {
    Eigen::VectorXd xNorm(dim);
    for (int i = 0; i < dim; ++i) {
      const auto idx = static_cast<std::size_t>(i);
      xNorm(i) = (design.x[idx] - midpoint(i)) / halfRange(i);
      xNorm(i) = std::clamp(xNorm(i), -1.0, 1.0);
    }
    return xNorm;
  };

  auto evaluatePolishPoint = [&](const Eigen::VectorXd& xNorm) -> Result<PolishEvaluation> {
    const DesignVector decodedVec = denormalize(xNorm);
    const PumpParams params = decodeVectorToParams(decodedVec, initialParams, bounds, members);

    auto artifactsResult = buildEvaluationArtifacts(params);
    if (!artifactsResult) {
      return std::unexpected(artifactsResult.error());
    }

    std::vector<double> residuals;
    const auto objective =
      evaluateObjectiveFromArtifacts(*artifactsResult, params, target, settings, &residuals);
    if (!objective.valid || residuals.empty()) {
      return std::unexpected(CoreError::GeometryBuildFailed);
    }

    return PolishEvaluation{
      .xNorm = xNorm,
      .design = decodedVec,
      .residuals = std::move(residuals),
      .objective = objective,
    };
  };

  if (settings.localPolishIterations > 0) {
    constexpr double kPolishFiniteDiffStep = 1e-3;
    constexpr double kPolishMinLambda = 1e-9;
    constexpr double kPolishMaxLambda = 1e12;
    double damping = 1e-3;

    auto currentResult = evaluatePolishPoint(normalizeDesign(bestDesign));
    if (currentResult) {
      auto current = std::move(*currentResult);
      if (current.objective.total < bestObjective.total) {
        bestObjective = current.objective;
        bestDesign = current.design;
      }

      for (int polishIteration = 0; polishIteration < settings.localPolishIterations;
           ++polishIteration) {
        if (cancelled()) {
          return std::unexpected(CoreError::Cancelled);
        }

        const auto residualCount = static_cast<int>(current.residuals.size());
        if (residualCount == 0) {
          break;
        }

        Eigen::VectorXd residual(residualCount);
        for (int row = 0; row < residualCount; ++row) {
          residual(row) = current.residuals[static_cast<std::size_t>(row)];
        }

        Eigen::MatrixXd jacobian = Eigen::MatrixXd::Zero(residualCount, dim);
        for (int col = 0; col < dim; ++col) {
          if (cancelled()) {
            return std::unexpected(CoreError::Cancelled);
          }

          Eigen::VectorXd plus = current.xNorm;
          Eigen::VectorXd minus = current.xNorm;
          plus(col) = std::clamp(plus(col) + kPolishFiniteDiffStep, -1.0, 1.0);
          minus(col) = std::clamp(minus(col) - kPolishFiniteDiffStep, -1.0, 1.0);

          const double centralDenom = plus(col) - minus(col);
          if (std::abs(centralDenom) <= kMinimumPositive) {
            continue;
          }

          auto plusResult = evaluatePolishPoint(plus);
          auto minusResult = evaluatePolishPoint(minus);

          if (plusResult && minusResult &&
              plusResult->residuals.size() == current.residuals.size() &&
              minusResult->residuals.size() == current.residuals.size()) {
            for (int row = 0; row < residualCount; ++row) {
              const auto idx = static_cast<std::size_t>(row);
              jacobian(row, col) = (plusResult->residuals[idx] - minusResult->residuals[idx]) /
                                   centralDenom;
            }
            continue;
          }

          if (plusResult && plusResult->residuals.size() == current.residuals.size() &&
              std::abs(plus(col) - current.xNorm(col)) > kMinimumPositive) {
            const double denom = plus(col) - current.xNorm(col);
            for (int row = 0; row < residualCount; ++row) {
              const auto idx = static_cast<std::size_t>(row);
              jacobian(row, col) = (plusResult->residuals[idx] - current.residuals[idx]) / denom;
            }
            continue;
          }

          if (minusResult && minusResult->residuals.size() == current.residuals.size() &&
              std::abs(current.xNorm(col) - minus(col)) > kMinimumPositive) {
            const double denom = current.xNorm(col) - minus(col);
            for (int row = 0; row < residualCount; ++row) {
              const auto idx = static_cast<std::size_t>(row);
              jacobian(row, col) = (current.residuals[idx] - minusResult->residuals[idx]) / denom;
            }
          }
        }

        Eigen::MatrixXd normal = jacobian.transpose() * jacobian;
        normal += damping * Eigen::MatrixXd::Identity(dim, dim);
        const Eigen::VectorXd rhs = -(jacobian.transpose() * residual);
        const Eigen::VectorXd step = normal.ldlt().solve(rhs);
        if (!step.allFinite() || step.norm() < 1e-7) {
          break;
        }

        bool accepted = false;
        double stepScale = 1.0;
        for (int attempt = 0; attempt < 6; ++attempt) {
          Eigen::VectorXd candidateX = current.xNorm + stepScale * step;
          for (int i = 0; i < dim; ++i) {
            candidateX(i) = std::clamp(candidateX(i), -1.0, 1.0);
          }
          if ((candidateX - current.xNorm).norm() < 1e-8) {
            break;
          }

          auto candidateResult = evaluatePolishPoint(candidateX);
          if (candidateResult && candidateResult->objective.total < current.objective.total) {
            current = std::move(*candidateResult);
            bestObjective = current.objective;
            bestDesign = current.design;
            damping = std::max(kPolishMinLambda, damping * 0.3);
            accepted = true;
            break;
          }
          stepScale *= 0.5;
        }

        if (!accepted) {
          damping *= 10.0;
          if (damping > kPolishMaxLambda) {
            break;
          }
        }
      }
    }
  }

  const PumpParams bestParams = decodeVectorToParams(bestDesign, initialParams, bounds, members);
  auto result = evaluateGeometryCandidate(bestParams, target, settings);
  if (!result) {
    return std::unexpected(result.error());
  }

  result->objective = bestObjective;
  result->converged = true;
  result->generations = generation;
  return result;
}

} // namespace ggm::core
