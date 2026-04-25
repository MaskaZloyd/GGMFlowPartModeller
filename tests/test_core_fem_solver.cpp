#include "core/fem_solver.hpp"
#include "core/logging.hpp"
#include "core/strip_grid.hpp"
#include "math/types.hpp"

#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ggm::core;
using namespace ggm::math;
using Catch::Matchers::WithinAbs;

namespace {

struct FemFixture
{
  FemFixture() { ggm::logging::init(); }
};

StripGrid
makeRectGrid(int nh, int m)
{
  std::vector<Vec2> hub;
  std::vector<Vec2> shroud;
  hub.reserve(static_cast<std::size_t>(nh));
  shroud.reserve(static_cast<std::size_t>(nh));
  for (int i = 0; i < nh; ++i) {
    const double z = static_cast<double>(i);
    hub.push_back({z, 0.5});
    shroud.push_back({z, 1.5});
  }
  return *buildStripGrid(hub, shroud, m);
}

}

TEST_CASE_METHOD(FemFixture, "solveFem: returns error on degenerate grid", "[fem]")
{
  StripGrid bad;
  bad.nh = 1;
  bad.m = 1;
  const auto result = solveFem(std::move(bad));
  REQUIRE(!result.has_value());
  REQUIRE(result.error() == CoreError::SolverFailed);
}

TEST_CASE_METHOD(FemFixture, "solveFem: succeeds on valid rectangular grid", "[fem]")
{
  auto grid = makeRectGrid(10, 6);
  const auto result = solveFem(std::move(grid));
  REQUIRE(result.has_value());
}

TEST_CASE_METHOD(FemFixture, "solveFem: psi solution size equals node count", "[fem]")
{
  constexpr int nh = 8;
  constexpr int m = 5;
  auto grid = makeRectGrid(nh, m);
  const auto result = solveFem(std::move(grid));
  REQUIRE(result.has_value());
  REQUIRE(static_cast<int>(result->psi.size()) == nh * m);
}

TEST_CASE_METHOD(FemFixture, "solveFem: Dirichlet BCs satisfied - hub psi=0, shroud psi=1", "[fem]")
{
  auto grid = makeRectGrid(10, 6);
  const auto hubNodes = grid.hubNodes;
  const auto shroudNodes = grid.shroudNodes;
  const auto result = solveFem(std::move(grid));
  REQUIRE(result.has_value());

  for (int n : hubNodes) {
    REQUIRE_THAT(result->psi[static_cast<std::size_t>(n)], WithinAbs(0.0, 1e-10));
  }
  for (int n : shroudNodes) {
    REQUIRE_THAT(result->psi[static_cast<std::size_t>(n)], WithinAbs(1.0, 1e-10));
  }
}

TEST_CASE_METHOD(FemFixture, "solveFem: psi monotonically ordered hub->shroud at mid-row", "[fem]")
{
  constexpr int nh = 12;
  constexpr int m = 8;
  auto grid = makeRectGrid(nh, m);
  const auto result = solveFem(std::move(grid));
  REQUIRE(result.has_value());

  const int midRow = nh / 2;
  for (int j = 1; j < m; ++j) {
    const double prev = result->psi[static_cast<std::size_t>(midRow * m + j - 1)];
    const double curr = result->psi[static_cast<std::size_t>(midRow * m + j)];
    REQUIRE(curr >= prev - 1e-10);
  }
}
