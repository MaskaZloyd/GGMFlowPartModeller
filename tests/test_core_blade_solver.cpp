#include "core/blade_solver.hpp"
#include "core/geometry.hpp"
#include "core/pump_params.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

ggm::core::BladeInputFromMeridional
validInput()
{
  ggm::core::PumpParams params;
  auto geometry = ggm::core::buildGeometry(params);
  REQUIRE(geometry);
  return ggm::core::BladeInputFromMeridional{
    .pumpParams = params,
    .geometry = *geometry,
  };
}

}

TEST_CASE("Blade solver: auto beta1/beta2 returns finite values", "[blade]")
{
  ggm::core::BladeDesignParams blade;
  blade.autoInletAngle = true;
  blade.autoOutletAngle = true;

  ggm::core::BladeSolver solver;
  auto result = solver.solve(blade, validInput());
  REQUIRE(result);
  REQUIRE(std::isfinite(result->inletTriangle.betaDeg));
  REQUIRE(std::isfinite(result->outletTriangle.betaDeg));
  REQUIRE(result->inletTriangle.betaDeg >= 5.0);
  REQUIRE(result->outletTriangle.betaDeg <= 85.0);
}

TEST_CASE("Blade solver: H(Q) decreases with positive hydraulic losses", "[blade]")
{
  ggm::core::BladeDesignParams blade;
  blade.autoOutletAngle = false;
  blade.beta2Deg = 30.0;
  blade.hydraulicLossK = 1000.0;

  ggm::core::BladeSolver solver;
  auto result = solver.solve(blade, validInput());
  REQUIRE(result);
  REQUIRE(result->performanceCurve.size() > 3U);

  const auto& curve = result->performanceCurve;
  REQUIRE(curve.front().headRealM > curve.back().headRealM);
}

TEST_CASE("Blade solver: slip factor is clamped to valid interval", "[blade]")
{
  ggm::core::BladeDesignParams blade;
  blade.autoSlipFactor = false;
  blade.slipFactor = 2.0;

  ggm::core::BladeSolver solver;
  auto result = solver.solve(blade, validInput());
  REQUIRE(result);
  REQUIRE(result->slipFactor <= 1.0);
  REQUIRE(result->slipFactor > 0.0);
}

TEST_CASE("Blade solver: invalid inputs return error", "[blade]")
{
  ggm::core::BladeDesignParams blade;
  blade.bladeCount = 1;

  ggm::core::BladeSolver solver;
  auto result = solver.solve(blade, validInput());
  REQUIRE(!result);
}
