#include "core/flow_solver_types.hpp"
#include "core/velocity_field.hpp"
#include "math/types.hpp"

#include <cmath>
#include <numbers>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ggm::core;
using namespace std;
using Catch::Matchers::WithinAbs;

TEST_CASE("Streamline Velocities - Annulus flow", "[core][velocity_field]")
{
  // Annulus parameters
  double rh = 1.0;
  double rs = 2.0;
  double L = 10.0;
  double Q = 10.0; // 10 m^3/s

  // Analytical velocity
  double expected_vz = Q / (std::numbers::pi * (rs * rs - rh * rh));
  double expected_vr = 0.0;

  // Build a simple grid
  int nh = 10;
  int m = 100;
  FlowSolution sol;
  sol.grid.nh = nh;
  sol.grid.m = m;

  // Nodes
  for (int i = 0; i < nh; ++i) {
    double z = L * static_cast<double>(i) / (nh - 1);
    for (int j = 0; j < m; ++j) {
      double r = rh + (rs - rh) * static_cast<double>(j) / (m - 1);
      sol.grid.nodes.push_back({z, r});

      // Analytical psi: psi(r) = (r^2 - rh^2) / (rs^2 - rh^2)
      double psi = (r * r - rh * rh) / (rs * rs - rh * rh);
      sol.psi.push_back(psi);
    }
  }

  // Triangles
  for (int i = 0; i < nh - 1; ++i) {
    for (int j = 0; j < m - 1; ++j) {
      int n00 = i * m + j;
      int n10 = (i + 1) * m + j;
      int n01 = i * m + (j + 1);
      int n11 = (i + 1) * m + (j + 1);

      sol.grid.triangles.push_back({n00, n10, n11});
      sol.grid.triangles.push_back({n00, n11, n01});
    }
  }

  // Create a streamline in the middle
  Streamline sl;
  sl.psiLevel = 0.5;
  for (int i = 0; i < nh; ++i) {
    double z = L * static_cast<double>(i) / (nh - 1);
    // r for psi = 0.5: r^2 = 0.5 * (rs^2 - rh^2) + rh^2
    double r = std::sqrt(0.5 * (rs * rs - rh * rh) + rh * rh);
    sl.points.push_back({z, r});
  }

  std::vector<Streamline> streamlines = {sl};

  auto res = computeStreamlineVelocities(sol, streamlines, Q);
  REQUIRE(res.has_value());

  const auto& velocities = res.value();
  REQUIRE(velocities.size() == 1);

  const auto& sv = velocities[0];
  REQUIRE_THAT(sv.psiLevel, WithinAbs(0.5, 1e-5));
  REQUIRE(sv.samples.size() == static_cast<size_t>(nh));
  for (const auto& sample : sv.samples) {
    // v_z should be near expected
    REQUIRE_THAT(sample.velocity.x(), WithinAbs(expected_vz, 0.05));
    // v_r should be near 0
    REQUIRE_THAT(sample.velocity.y(), WithinAbs(0.0, 0.05));
    REQUIRE_THAT(sample.speed, WithinAbs(expected_vz, 0.05));
  }
}
