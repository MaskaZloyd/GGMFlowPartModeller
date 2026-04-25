#include "core/serialization.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace ggm::core {

namespace {

constexpr int FILE_VERSION = 1;

void
toJson(nlohmann::json& json, const PumpParams& params)
{
  json = nlohmann::json{
    {"version", FILE_VERSION},
    {"xa", params.xa},
    {"dvt", params.dvt},
    {"d2", params.d2},
    {"r1", params.r1},
    {"r2", params.r2},
    {"r3", params.r3},
    {"r4", params.r4},
    {"al1Deg", params.al1Deg},
    {"al2Deg", params.al2Deg},
    {"al02Deg", params.al02Deg},
    {"be1Deg", params.be1Deg},
    {"be3RawDeg", params.be3RawDeg},
    {"b2", params.b2},
    {"din", params.din},
  };
}

PumpParams
fromJson(const nlohmann::json& json)
{
  PumpParams params;
  params.xa = json.value("xa", params.xa);
  params.dvt = json.value("dvt", params.dvt);
  params.d2 = json.value("d2", params.d2);
  params.r1 = json.value("r1", params.r1);
  params.r2 = json.value("r2", params.r2);
  params.r3 = json.value("r3", params.r3);
  params.r4 = json.value("r4", params.r4);
  params.al1Deg = json.value("al1Deg", params.al1Deg);
  params.al2Deg = json.value("al2Deg", params.al2Deg);
  params.al02Deg = json.value("al02Deg", params.al02Deg);
  params.be1Deg = json.value("be1Deg", params.be1Deg);
  params.be3RawDeg = json.value("be3RawDeg", params.be3RawDeg);
  params.b2 = json.value("b2", params.b2);
  params.din = json.value("din", params.din);
  return params;
}

}

Result<void>
saveParams(const PumpParams& params, const std::filesystem::path& path) noexcept
{
  try {
    nlohmann::json json;
    toJson(json, params);

    std::ofstream file(path);
    if (!file.is_open()) {
      return std::unexpected(CoreError::FileWriteFailed);
    }
    file << json.dump(2);
    if (file.fail()) {
      return std::unexpected(CoreError::FileWriteFailed);
    }
    return {};
  } catch (...) {
    return std::unexpected(CoreError::SerializationFailed);
  }
}

Result<PumpParams>
loadParams(const std::filesystem::path& path) noexcept
{
  try {
    if (!std::filesystem::exists(path)) {
      return std::unexpected(CoreError::FileNotFound);
    }

    std::ifstream file(path);
    if (!file.is_open()) {
      return std::unexpected(CoreError::FileNotFound);
    }

    auto json = nlohmann::json::parse(file);

    if (json.value("version", 0) != FILE_VERSION) {
      return std::unexpected(CoreError::ParseError);
    }

    return fromJson(json);
  } catch (...) {
    return std::unexpected(CoreError::ParseError);
  }
}

}
