#include "core/pump_params.hpp"
#include "core/serialization.hpp"

#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ggm::core;

namespace {
std::filesystem::path
tmpPath()
{
  return std::filesystem::temp_directory_path() / "ggm_test_params.ggm";
}
}

TEST_CASE("saveParams / loadParams: round-trip preserves all fields", "[serialization]")
{
  PumpParams params;
  params.xa = 25.0;
  params.dvt = 160.0;
  params.d2 = 420.0;
  params.r1 = 55.0;
  params.r2 = 65.0;
  params.r3 = 24.0;
  params.r4 = 33.0;
  params.al1Deg = 3.0;
  params.al2Deg = -3.0;
  params.al02Deg = -6.0;
  params.be1Deg = 50.0;
  params.be3RawDeg = 28.0;
  params.b2 = 30.0;
  params.din = 215.0;

  const auto path = tmpPath();
  const auto saveResult = saveParams(params, path);
  REQUIRE(saveResult.has_value());

  const auto loadResult = loadParams(path);
  REQUIRE(loadResult.has_value());

  REQUIRE(*loadResult == params);

  std::filesystem::remove(path);
}

TEST_CASE("loadParams: returns error for missing file", "[serialization]")
{
  const auto result = loadParams("/tmp/ggm_nonexistent_file_xyz.ggm");
  REQUIRE(!result.has_value());
}

TEST_CASE("saveParams / loadParams: default params round-trip", "[serialization]")
{
  const PumpParams defaults;
  const auto path = tmpPath();

  const auto saveResult = saveParams(defaults, path);
  REQUIRE(saveResult.has_value());

  const auto loadResult = loadParams(path);
  REQUIRE(loadResult.has_value());
  REQUIRE(*loadResult == defaults);

  std::filesystem::remove(path);
}

TEST_CASE("saveProject / loadProject: blade design params round-trip", "[serialization]")
{
  ProjectData project;
  project.pumpParams.d2 = 430.0;
  project.bladeDesign.bladeCount = 8;
  project.bladeDesign.flowRateM3s = 0.072;
  project.bladeDesign.rpm = 2900.0;
  project.bladeDesign.autoInletAngle = false;
  project.bladeDesign.beta1Deg = 18.0;
  project.bladeDesign.angleLaw = BladeAngleLaw::Bezier;
  project.bladeDesign.thicknessLaw = BladeThicknessLaw::Linear;

  const auto path = tmpPath();
  const auto saveResult = saveProject(project, path);
  REQUIRE(saveResult.has_value());

  const auto loadResult = loadProject(path);
  REQUIRE(loadResult.has_value());
  project.pumpParams.qM3s = project.bladeDesign.flowRateM3s;
  REQUIRE(loadResult->pumpParams == project.pumpParams);
  REQUIRE(loadResult->bladeDesign == project.bladeDesign);

  std::filesystem::remove(path);
}

TEST_CASE("loadProject: old JSON without bladeDesign uses defaults", "[serialization]")
{
  const auto path = tmpPath();
  {
    std::ofstream file(path);
    file << R"json({
      "version": 1,
      "d2": 420.0,
      "din": 215.0,
      "qM3s": 0.061
    })json";
  }

  const auto loadResult = loadProject(path);
  REQUIRE(loadResult.has_value());
  REQUIRE(loadResult->pumpParams.d2 == 420.0);
  REQUIRE(loadResult->pumpParams.din == 215.0);
  BladeDesignParams defaults;
  defaults.flowRateM3s = 0.061;
  REQUIRE(loadResult->bladeDesign == defaults);

  std::filesystem::remove(path);
}
