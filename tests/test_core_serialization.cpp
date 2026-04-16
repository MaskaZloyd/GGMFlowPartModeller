#include "core/pump_params.hpp"
#include "core/serialization.hpp"

#include <filesystem>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ggm::core;

namespace {
std::filesystem::path
tmpPath()
{
  return std::filesystem::temp_directory_path() / "ggm_test_params.ggm";
}
} // namespace

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
