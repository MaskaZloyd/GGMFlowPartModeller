#include "core/flow_solver_types.hpp"
#include "core/velocity_field.hpp"
#include "math/types.hpp"

#include <cmath>
#include <numbers>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("Streamline Velocities - Annulus flow", "[core][velocity_field]")
{
  // Annulus parameters, SI units: meters and m^3/s.
  constexpr double rh = 1.0;
  constexpr double rs = 2.0;
  constexpr double length = 10.0;
  constexpr double flowRateM3s = 10.0;
  constexpr double lengthUnitToMeters = 1.0;

  // Analytical velocity:
  //
  // psi(r) = (r^2 - rh^2) / (rs^2 - rh^2)
  //
  // v_z = Q / (pi * (rs^2 - rh^2))
  // v_r = 0
  const double expectedVz = flowRateM3s / (std::numbers::pi * (rs * rs - rh * rh));

  constexpr int nh = 10;
  constexpr int m = 100;

  ggm::core::FlowSolution sol;
  sol.grid.nh = nh;
  sol.grid.m = m;

  sol.grid.nodes.reserve(static_cast<std::size_t>(nh * m));
  sol.psi.reserve(static_cast<std::size_t>(nh * m));

  for (int i = 0; i < nh; ++i) {
    const double z = length * static_cast<double>(i) / static_cast<double>(nh - 1);

    for (int j = 0; j < m; ++j) {
      const double r = rh + (rs - rh) * static_cast<double>(j) / static_cast<double>(m - 1);

      sol.grid.nodes.push_back({z, r});

      const double psi = (r * r - rh * rh) / (rs * rs - rh * rh);

      sol.psi.push_back(psi);
    }
  }

  sol.grid.triangles.reserve(static_cast<std::size_t>((nh - 1) * (m - 1) * 2));

  for (int i = 0; i < nh - 1; ++i) {
    for (int j = 0; j < m - 1; ++j) {
      const int n00 = i * m + j;
      const int n10 = (i + 1) * m + j;
      const int n01 = i * m + (j + 1);
      const int n11 = (i + 1) * m + (j + 1);

      sol.grid.triangles.push_back({n00, n10, n11});
      sol.grid.triangles.push_back({n00, n11, n01});
    }
  }

  ggm::core::Streamline streamline;
  streamline.psiLevel = 0.5;
  streamline.points.reserve(static_cast<std::size_t>(nh));

  const double streamlineRadius = std::sqrt((0.5 * (rs * rs - rh * rh)) + rh * rh);

  for (int i = 0; i < nh; ++i) {
    const double z = length * static_cast<double>(i) / static_cast<double>(nh - 1);

    streamline.points.push_back({z, streamlineRadius});
  }

  const std::vector<ggm::core::Streamline> streamlines = {streamline};

  auto result =
    ggm::core::computeStreamlineVelocities(sol, streamlines, flowRateM3s, lengthUnitToMeters);

  REQUIRE(result.has_value());

  const auto& velocities = result.value();

  REQUIRE(velocities.size() == 1);

  const auto& lineVelocity = velocities[0];

  REQUIRE_THAT(lineVelocity.psiLevel, WithinAbs(0.5, 1e-5));
  REQUIRE(lineVelocity.samples.size() == static_cast<std::size_t>(nh));

  for (const auto& sample : lineVelocity.samples) {
    REQUIRE_THAT(sample.velocity.x(), WithinAbs(expectedVz, 0.05));
    REQUIRE_THAT(sample.velocity.y(), WithinAbs(0.0, 0.05));
    REQUIRE_THAT(sample.speed, WithinAbs(expectedVz, 0.05));
  }
}
