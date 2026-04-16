#include "core/strip_grid.hpp"
#include "math/types.hpp"

#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ggm::core;
using namespace ggm::math;
using Catch::Matchers::WithinAbs;

namespace {
// Minimal straight hub and shroud, nh points each.
std::vector<Vec2>
straightLine(double r, int nh)
{
  std::vector<Vec2> pts;
  pts.reserve(static_cast<std::size_t>(nh));
  for (int i = 0; i < nh; ++i) {
    pts.push_back({static_cast<double>(i), r});
  }
  return pts;
}
} // namespace

TEST_CASE("buildStripGrid: returns error on mismatched input sizes", "[strip_grid]")
{
  const auto hub = straightLine(0.0, 5);
  const auto shroud = straightLine(1.0, 4); // different size
  const auto result = buildStripGrid(hub, shroud, 5);
  REQUIRE(!result.has_value());
}

TEST_CASE("buildStripGrid: returns error when m < 3", "[strip_grid]")
{
  const auto hub = straightLine(0.0, 4);
  const auto shroud = straightLine(1.0, 4);
  const auto result = buildStripGrid(hub, shroud, 2);
  REQUIRE(!result.has_value());
}

TEST_CASE("buildStripGrid: correct node and triangle counts", "[strip_grid]")
{
  constexpr int nh = 6;
  constexpr int m = 5;
  const auto hub = straightLine(0.0, nh);
  const auto shroud = straightLine(1.0, nh);

  const auto result = buildStripGrid(hub, shroud, m);
  REQUIRE(result.has_value());

  REQUIRE(result->nh == nh);
  REQUIRE(result->m == m);
  REQUIRE(static_cast<int>(result->nodes.size()) == nh * m);
  REQUIRE(static_cast<int>(result->triangles.size()) == 2 * (nh - 1) * (m - 1));
}

TEST_CASE("buildStripGrid: boundary node counts equal nh", "[strip_grid]")
{
  constexpr int nh = 8;
  constexpr int m = 4;
  const auto hub = straightLine(0.0, nh);
  const auto shroud = straightLine(2.0, nh);

  const auto result = buildStripGrid(hub, shroud, m);
  REQUIRE(result.has_value());

  REQUIRE(static_cast<int>(result->hubNodes.size()) == nh);
  REQUIRE(static_cast<int>(result->shroudNodes.size()) == nh);
}

TEST_CASE("buildStripGrid: hub nodes have r=0, shroud nodes have r=2 (before smoothing)",
          "[strip_grid]")
{
  // Use a large nh so that boundary pinning dominates over smoothing
  constexpr int nh = 10;
  constexpr int m = 3;
  const auto hub = straightLine(0.0, nh);
  const auto shroud = straightLine(2.0, nh);

  const auto result = buildStripGrid(hub, shroud, m);
  REQUIRE(result.has_value());

  // Laplace smoothing pins boundary rows — first and last rows intact
  for (int i = 0; i < nh; ++i) {
    const auto hubIdx = static_cast<std::size_t>(result->hubNodes[static_cast<std::size_t>(i)]);
    REQUIRE_THAT(result->nodes[hubIdx].y(), WithinAbs(0.0, 1e-10));
    const auto shrIdx = static_cast<std::size_t>(result->shroudNodes[static_cast<std::size_t>(i)]);
    REQUIRE_THAT(result->nodes[shrIdx].y(), WithinAbs(2.0, 1e-10));
  }
}
