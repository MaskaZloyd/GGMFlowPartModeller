#include "math/bezier.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ggm::math;
using Catch::Matchers::WithinRel;

TEST_CASE("arcToBezier: endpoints lie on circle", "[bezier]")
{
  const Vec2 center{0.0, 0.0};
  constexpr double radius = 1.0;
  constexpr double a0 = 0.0;
  constexpr double a1 = std::numbers::pi / 2.0;

  const auto seg = arcToBezier(center, radius, a0, a1);

  REQUIRE_THAT(seg.p0.norm(), WithinRel(radius, 1e-12));
  REQUIRE_THAT(seg.p2.norm(), WithinRel(radius, 1e-12));
}

TEST_CASE("arcToBezier: start point matches expected angle", "[bezier]")
{
  const Vec2 center{0.0, 0.0};
  constexpr double radius = 2.0;
  constexpr double a0 = 0.0;
  constexpr double a1 = std::numbers::pi / 2.0;

  const auto seg = arcToBezier(center, radius, a0, a1);

  REQUIRE_THAT(seg.p0.x(), WithinRel(radius, 1e-12));
  REQUIRE_THAT(seg.p0.y(), WithinRel(0.0, 1e-9));
}

TEST_CASE("segToBezier: endpoints match input", "[bezier]")
{
  const Vec2 start{1.0, 2.0};
  const Vec2 end{4.0, 6.0};

  const auto seg = segToBezier(start, end);

  REQUIRE_THAT(seg.p0.x(), WithinRel(start.x(), 1e-12));
  REQUIRE_THAT(seg.p0.y(), WithinRel(start.y(), 1e-12));
  REQUIRE_THAT(seg.p2.x(), WithinRel(end.x(), 1e-12));
  REQUIRE_THAT(seg.p2.y(), WithinRel(end.y(), 1e-12));
  REQUIRE_THAT(seg.w1, WithinRel(1.0, 1e-12));
}

TEST_CASE("evalRationalQuadratic: t=0 and t=1 return endpoints", "[bezier]")
{
  const Vec2 p0{0.0, 0.0};
  const Vec2 p2{2.0, 0.0};
  const ArcBezier seg{p0, {1.0, 1.0}, p2, 1.0 / std::sqrt(2.0)};

  const auto atStart = evalRationalQuadratic(seg, 0.0);
  const auto atEnd = evalRationalQuadratic(seg, 1.0);

  REQUIRE_THAT(atStart.x(), WithinRel(p0.x(), 1e-12));
  REQUIRE_THAT(atEnd.x(), WithinRel(p2.x(), 1e-12));
}

TEST_CASE("evaluateSegment: produces correct point count", "[bezier]")
{
  const auto seg = segToBezier({0.0, 0.0}, {1.0, 0.0});
  constexpr int numPts = 50;

  const auto pts = evaluateSegment(seg, numPts);

  REQUIRE(static_cast<int>(pts.size()) == numPts);
}
