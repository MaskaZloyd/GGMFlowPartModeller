#include "math/bezier.hpp"
#include "math/nurbs.hpp"

#include <array>
#include <cmath>
#include <numbers>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ggm::math;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("evaluate: single arc segment - all points on circle", "[nurbs]")
{
  const Vec2 center{0.0, 0.0};
  constexpr double radius = 1.0;
  const std::array<ArcBezier, 1> segs = {arcToBezier(center, radius, 0.0, std::numbers::pi / 2.0)};

  const auto curve = buildFromSegments(segs);
  const auto pts = evaluate(curve, 100);

  REQUIRE(pts.size() == 100);
  for (const auto& pt : pts) {
    REQUIRE_THAT(pt.norm(), WithinRel(radius, 1e-10));
  }
}

TEST_CASE("evaluate: straight segment - collinear points", "[nurbs]")
{
  const Vec2 p0{0.0, 0.0};
  const Vec2 p1{3.0, 4.0};
  const std::array<ArcBezier, 1> segs = {segToBezier(p0, p1)};

  const auto curve = buildFromSegments(segs);
  const auto pts = evaluate(curve, 50);

  REQUIRE(pts.size() == 50);
  const Vec2 dir = (p1 - p0).normalized();
  for (const auto& pt : pts) {
    const double cross = dir.x() * (pt.y() - p0.y()) - dir.y() * (pt.x() - p0.x());
    REQUIRE_THAT(cross, WithinAbs(0.0, 1e-10));
  }
}

TEST_CASE("evaluate: first and last points match curve endpoints", "[nurbs]")
{
  const Vec2 center{5.0, 5.0};
  constexpr double radius = 3.0;
  const std::array<ArcBezier, 2> segs = {
    arcToBezier(center, radius, 0.0, std::numbers::pi / 2.0),
    arcToBezier(center, radius, std::numbers::pi / 2.0, std::numbers::pi),
  };

  const auto curve = buildFromSegments(segs);
  constexpr int numPts = 200;
  const auto pts = evaluate(curve, numPts);

  REQUIRE(static_cast<int>(pts.size()) == numPts);
  REQUIRE_THAT(pts.front().x(), WithinRel(center.x() + radius, 1e-10));
  REQUIRE_THAT(pts.back().x(), WithinRel(center.x() - radius, 1e-10));
}

TEST_CASE("buildFromSegments: correct knot and control-point count", "[nurbs]")
{
  const std::array<ArcBezier, 3> segs = {
    segToBezier({0.0, 0.0}, {1.0, 0.0}),
    segToBezier({1.0, 0.0}, {2.0, 0.0}),
    segToBezier({2.0, 0.0}, {3.0, 0.0}),
  };

  const auto curve = buildFromSegments(segs);

  constexpr std::size_t n = 3;
  REQUIRE(curve.controlPoints.size() == 2 * n + 1);
  REQUIRE(curve.weights.size() == 2 * n + 1);
  REQUIRE(curve.knots.size() == 2 * n + 1 + static_cast<std::size_t>(curve.degree) + 1);
}

TEST_CASE("evaluate: two-arc half-circle matches analytic radius", "[nurbs]")
{
  const Vec2 center{2.0, 3.0};
  constexpr double radius = 1.5;
  const std::array<ArcBezier, 2> segs = {
    arcToBezier(center, radius, 0.0, std::numbers::pi / 2.0),
    arcToBezier(center, radius, std::numbers::pi / 2.0, std::numbers::pi),
  };

  const auto curve = buildFromSegments(segs);
  const auto pts = evaluate(curve, 500);

  REQUIRE(pts.size() == 500);
  for (const auto& point : pts) {
    const double diffX = point.x() - center.x();
    const double diffY = point.y() - center.y();
    const double dist = std::sqrt((diffX * diffX) + (diffY * diffY));
    REQUIRE_THAT(dist, WithinRel(radius, 1e-10));
  }
}
