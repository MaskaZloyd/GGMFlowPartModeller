#include "core/geometry_optimizer.hpp"

#include <cmath>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

[[nodiscard]] ggm::core::TargetAreaCurve
makeTargetCurveFromSamples(std::span<const double> samples)
{
  std::vector<ggm::core::TargetAreaPoint> points;
  points.reserve(samples.size());

  for (std::size_t i = 0; i < samples.size(); ++i) {
    const double xi =
      samples.size() > 1U ? static_cast<double>(i) / static_cast<double>(samples.size() - 1U) : 0.0;
    points.push_back({xi, samples[i]});
  }

  return ggm::core::TargetAreaCurve(std::move(points));
}

[[nodiscard]] ggm::core::Result<ggm::core::TargetAreaCurve>
makeSelfTargetCurve(const ggm::core::PumpParams& params, const int sampleCount)
{
  ggm::core::GeometryOptimizationSettings settings;
  settings.sampleCount = sampleCount;
  settings.populationSize = 4;
  settings.maxGenerations = 1;
  settings.mask = ggm::core::GeometryVariableMask{};

  const auto candidate =
    ggm::core::evaluateGeometryCandidate(params, ggm::core::TargetAreaCurve{}, settings);
  if (!candidate) {
    return std::unexpected(candidate.error());
  }

  const auto samples = ggm::core::normalizedAreaSamples(candidate->areaProfile, sampleCount);
  if (!samples) {
    return std::unexpected(samples.error());
  }

  return makeTargetCurveFromSamples(*samples);
}

}

TEST_CASE("normalizedAreaSamples normalizes by outlet area", "[core][geometry_optimizer]")
{
  ggm::core::AreaProfile profile;
  profile.flowAreas = {2.0, 4.0, 6.0};
  profile.arcLengths = {0.0, 0.5, 1.0};

  const auto samples = ggm::core::normalizedAreaSamples(profile, 3);

  REQUIRE(samples.has_value());
  REQUIRE(samples->size() == 3U);
  REQUIRE_THAT((*samples)[0], WithinAbs(2.0 / 6.0, 1e-12));
  REQUIRE_THAT((*samples)[1], WithinAbs(4.0 / 6.0, 1e-12));
  REQUIRE_THAT((*samples)[2], WithinAbs(1.0, 1e-12));
}

TEST_CASE("evaluateGeometryObjective rejects obviously invalid geometry parameters",
          "[core][geometry_optimizer]")
{
  ggm::core::PumpParams params;
  params.d2 = params.dvt;

  const auto objective =
    ggm::core::evaluateGeometryObjective(params, ggm::core::TargetAreaCurve{}, {});

  REQUIRE_FALSE(objective.valid);
  REQUIRE(objective.total > 1e20);
}

TEST_CASE("evaluateGeometryObjective returns finite value for a valid self target",
          "[core][geometry_optimizer]")
{
  ggm::core::PumpParams params;
  ggm::core::GeometryOptimizationSettings settings;
  settings.sampleCount = 16;
  settings.populationSize = 4;
  settings.maxGenerations = 1;
  settings.mask = ggm::core::GeometryVariableMask{};

  const auto target = makeSelfTargetCurve(params, settings.sampleCount);
  REQUIRE(target.has_value());

  const auto objective = ggm::core::evaluateGeometryObjective(params, *target, settings);

  REQUIRE(objective.valid);
  REQUIRE(std::isfinite(objective.total));
  REQUIRE_THAT(objective.areaError, WithinAbs(0.0, 1e-10));
}

TEST_CASE("optimizeGeometryForTargetArea keeps D2 and Dvt fixed", "[core][geometry_optimizer]")
{
  ggm::core::PumpParams initialParams;
  ggm::core::GeometryOptimizationSettings settings;
  settings.sampleCount = 12;
  settings.populationSize = 4;
  settings.maxGenerations = 1;
  settings.mask = ggm::core::GeometryVariableMask{};

  const auto target = makeSelfTargetCurve(initialParams, settings.sampleCount);
  REQUIRE(target.has_value());
  const auto bounds = ggm::core::makeDefaultBounds(initialParams.d2, initialParams.dvt);
  const auto result =
    ggm::core::optimizeGeometryForTargetArea(initialParams, *target, bounds, settings);

  REQUIRE(result.has_value());
  REQUIRE(result->objective.valid);
  REQUIRE(std::isfinite(result->objective.total));
  REQUIRE_THAT(result->params.d2, WithinRel(initialParams.d2, 1e-12));
  REQUIRE_THAT(result->params.dvt, WithinRel(initialParams.dvt, 1e-12));
}
