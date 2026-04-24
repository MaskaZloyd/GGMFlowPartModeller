#include "core/target_area_curve.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("TargetAreaCurve linearly interpolates endpoint and midpoint values",
          "[core][target_area_curve]")
{
  const ggm::core::TargetAreaCurve curve({{0.0, 1.0}, {1.0, 3.0}});

  REQUIRE(curve.isValid());
  REQUIRE_THAT(curve.evaluate(0.0), WithinAbs(1.0, 1e-12));
  REQUIRE_THAT(curve.evaluate(0.5), WithinAbs(2.0, 1e-12));
  REQUIRE_THAT(curve.evaluate(1.0), WithinAbs(3.0, 1e-12));
}
