#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "math/arc_length.hpp"
#include "math/types.hpp"

#include <numbers>
#include <vector>

using namespace ggm::math;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

TEST_CASE("resampleArcLength: empty input returns empty output", "[arc_length]")
{
  const std::vector<Vec2> empty;
  const auto result = resampleArcLength(empty, 10);
  REQUIRE(result.empty());
}

TEST_CASE("resampleArcLength: single point returns empty output", "[arc_length]")
{
  const std::vector<Vec2> single{{0.0, 0.0}};
  const auto result = resampleArcLength(single, 5);
  REQUIRE(result.empty());
}

TEST_CASE("resampleArcLength: output has requested point count", "[arc_length]")
{
  // Horizontal segment of length 4
  const std::vector<Vec2> line{{0.0, 0.0}, {2.0, 0.0}, {4.0, 0.0}};
  constexpr int n = 9;
  const auto result = resampleArcLength(line, n);
  REQUIRE(static_cast<int>(result.size()) == n);
}

TEST_CASE("resampleArcLength: endpoints preserved for straight line", "[arc_length]")
{
  const std::vector<Vec2> line{{0.0, 0.0}, {1.0, 0.0}, {3.0, 0.0}};
  const auto result = resampleArcLength(line, 5);

  REQUIRE_THAT(result.front().x(), WithinAbs(0.0, 1e-12));
  REQUIRE_THAT(result.back().x(), WithinRel(3.0, 1e-12));
}

TEST_CASE("resampleArcLength: equal spacing on straight line", "[arc_length]")
{
  // Straight line: resampled points must be equally spaced
  const std::vector<Vec2> line{{0.0, 0.0}, {10.0, 0.0}};
  constexpr int n = 6;
  const auto result = resampleArcLength(line, n);

  REQUIRE(static_cast<int>(result.size()) == n);
  constexpr double expectedStep = 10.0 / (n - 1);
  for (int i = 1; i < n; ++i) {
    const double dist = (result[static_cast<std::size_t>(i)] -
                         result[static_cast<std::size_t>(i - 1)]).norm();
    REQUIRE_THAT(dist, WithinRel(expectedStep, 1e-10));
  }
}
