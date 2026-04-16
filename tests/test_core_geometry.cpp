#include "core/geometry.hpp"
#include "core/pump_params.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ggm::core;
using Catch::Matchers::WithinRel;

TEST_CASE("buildGeometry: succeeds with default parameters", "[geometry]")
{
  const PumpParams params;
  const auto result = buildGeometry(params);
  REQUIRE(result.has_value());
}

TEST_CASE("buildGeometry: hub and shroud curves are non-empty", "[geometry]")
{
  const PumpParams params;
  const auto result = buildGeometry(params);
  REQUIRE(result.has_value());
  REQUIRE(!result->hubCurve.empty());
  REQUIRE(!result->shroudCurve.empty());
}

TEST_CASE("buildGeometry: hub and shroud end at outlet radius d2/2", "[geometry]")
{
  const PumpParams params;
  const auto result = buildGeometry(params);
  REQUIRE(result.has_value());

  const double rOutlet = params.d2 / 2.0;
  REQUIRE_THAT(result->hubCurve.back().y(), WithinRel(rOutlet, 1e-6));
  REQUIRE_THAT(result->shroudCurve.back().y(), WithinRel(rOutlet, 1e-6));
}

TEST_CASE("buildGeometry: hub starts at shaft bore radius dvt/2", "[geometry]")
{
  const PumpParams params;
  const auto result = buildGeometry(params);
  REQUIRE(result.has_value());

  REQUIRE_THAT(result->hubCurve.front().y(), WithinRel(params.dvt / 2.0, 1e-6));
}

TEST_CASE("buildGeometry: NURBS control points are non-empty", "[geometry]")
{
  const PumpParams params;
  const auto result = buildGeometry(params);
  REQUIRE(result.has_value());
  REQUIRE(!result->hubNurbs.controlPoints.empty());
  REQUIRE(!result->shroudNurbs.controlPoints.empty());
}
