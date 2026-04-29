#include "core/blade_geometry.hpp"
#include "core/blade_solver.hpp"
#include "core/geometry.hpp"
#include "core/pump_params.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using ggm::core::BladeDesignParams;
using ggm::core::BladeInputFromMeridional;
using ggm::core::BladeSolver;

namespace {

BladeInputFromMeridional
validInput()
{
  ggm::core::PumpParams params;
  auto geometry = ggm::core::buildGeometry(params);
  REQUIRE(geometry);
  return BladeInputFromMeridional{
    .pumpParams = params,
    .geometry = *geometry,
  };
}

}

TEST_CASE("Blade geometry: phi is monotone for valid cylindrical blade", "[blade]")
{
  BladeDesignParams blade;
  BladeSolver solver;

  auto result = solver.solve(blade, validInput());
  REQUIRE(result);
  REQUIRE(result->sections.size() > 2U);

  for (std::size_t i = 1; i < result->sections.size(); ++i) {
    REQUIRE(result->sections[i].phiRad >= result->sections[i - 1U].phiRad);
  }
}

TEST_CASE("Blade geometry: contour is closed and finite", "[blade]")
{
  BladeDesignParams blade;
  BladeSolver solver;

  auto result = solver.solve(blade, validInput());
  REQUIRE(result);
  REQUIRE(result->singleBlade.closedContour.size() > 4U);

  const auto& contour = result->singleBlade.closedContour;
  REQUIRE(contour.front().xMm == contour.back().xMm);
  REQUIRE(contour.front().yMm == contour.back().yMm);
  for (const auto& point : contour) {
    REQUIRE(std::isfinite(point.xMm));
    REQUIRE(std::isfinite(point.yMm));
  }
}

TEST_CASE("Blade geometry: thickness remains smaller than pitch", "[blade]")
{
  BladeDesignParams blade;
  BladeSolver solver;

  auto result = solver.solve(blade, validInput());
  REQUIRE(result);
  for (const auto& section : result->sections) {
    REQUIRE(section.thicknessMm > 0.0);
    REQUIRE(section.thicknessMm < section.pitchMm);
    REQUIRE(section.blockage > 0.0);
    REQUIRE(section.blockage < 1.0);
  }
}

TEST_CASE("Blade geometry: outlet edge is trimmed to D2", "[blade]")
{
  BladeDesignParams blade;
  BladeSolver solver;

  auto result = solver.solve(blade, validInput());
  REQUIRE(result);

  const double r2 = result->outletRadiusMm;
  const auto& contour = result->singleBlade;
  REQUIRE(contour.pressureSide.size() > 2U);
  REQUIRE(contour.suctionSide.size() > 2U);
  REQUIRE(std::abs(std::hypot(contour.pressureSide.back().xMm, contour.pressureSide.back().yMm) - r2) <
          1.0e-9);
  REQUIRE(std::abs(std::hypot(contour.suctionSide.back().xMm, contour.suctionSide.back().yMm) - r2) <
          1.0e-9);

  for (const auto& point : contour.pressureSide) {
    REQUIRE(std::hypot(point.xMm, point.yMm) <= r2 + 1.0e-8);
  }
  for (const auto& point : contour.suctionSide) {
    REQUIRE(std::hypot(point.xMm, point.yMm) <= r2 + 1.0e-8);
  }
}

TEST_CASE("Blade geometry: leading edge has circular arc samples", "[blade]")
{
  BladeDesignParams blade;
  BladeSolver solver;

  auto result = solver.solve(blade, validInput());
  REQUIRE(result);

  const auto& contour = result->singleBlade;
  REQUIRE(contour.closedContour.size() > contour.pressureSide.size() + contour.suctionSide.size());

  const auto& center = contour.centerline.front();
  const double expectedRadius = 0.5 * result->sections.front().thicknessMm;
  int arcSamples = 0;
  for (const auto& point : contour.closedContour) {
    const double d = std::hypot(point.xMm - center.xMm, point.yMm - center.yMm);
    if (std::abs(d - expectedRadius) < 1.0e-8) {
      ++arcSamples;
    }
  }
  REQUIRE(arcSamples >= 6);
}
