#include "core/target_area_curve.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("TargetAreaCurve keeps two-point curves linear",
          "[core][target_area_curve]")
{
  const ggm::core::TargetAreaCurve curve({{0.0, 1.0}, {1.0, 3.0}});

  REQUIRE(curve.isValid());
  REQUIRE_THAT(curve.evaluate(0.0), WithinAbs(1.0, 1e-12));
  REQUIRE_THAT(curve.evaluate(0.5), WithinAbs(2.0, 1e-12));
  REQUIRE_THAT(curve.evaluate(1.0), WithinAbs(3.0, 1e-12));
}

TEST_CASE("TargetAreaCurve uses smooth PCHIP interpolation through control points",
          "[core][target_area_curve]")
{
  const ggm::core::TargetAreaCurve curve({{0.0, 1.0}, {0.4, 2.0}, {1.0, 3.0}});

  REQUIRE(curve.isValid());
  REQUIRE_THAT(curve.evaluate(0.0), WithinAbs(1.0, 1e-12));
  REQUIRE_THAT(curve.evaluate(0.4), WithinAbs(2.0, 1e-12));
  REQUIRE_THAT(curve.evaluate(1.0), WithinAbs(3.0, 1e-12));
  REQUIRE_THAT(curve.evaluate(0.2), WithinAbs(1.5403153153153153, 1e-12));
}

TEST_CASE("TargetAreaCurve PCHIP preserves monotone target shape",
          "[core][target_area_curve]")
{
  const ggm::core::TargetAreaCurve curve({{0.0, 0.55}, {0.4, 0.78}, {0.75, 0.92}, {1.0, 1.0}});

  REQUIRE(curve.isValid());

  double previous = curve.evaluate(0.0);
  for (int i = 1; i <= 100; ++i) {
    const double xi = static_cast<double>(i) / 100.0;
    const double value = curve.evaluate(xi);
    REQUIRE(value >= previous - 1e-12);
    REQUIRE(value >= 0.55 - 1e-12);
    REQUIRE(value <= 1.0 + 1e-12);
    previous = value;
  }
}
