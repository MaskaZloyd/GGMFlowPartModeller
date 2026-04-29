#include "core/serialization.hpp"

#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

namespace ggm::core {

namespace {

constexpr int FILE_VERSION = 1;

[[nodiscard]] const char*
toString(BladeLatticeType value) noexcept
{
  switch (value) {
    case BladeLatticeType::Cylindrical:
      return "cylindrical";
    case BladeLatticeType::Spatial:
      return "spatial";
  }
  return "cylindrical";
}

[[nodiscard]] const char*
toString(BladeAngleLaw value) noexcept
{
  switch (value) {
    case BladeAngleLaw::Constant:
      return "constant";
    case BladeAngleLaw::Linear:
      return "linear";
    case BladeAngleLaw::Quadratic:
      return "quadratic";
    case BladeAngleLaw::Bezier:
      return "bezier";
  }
  return "linear";
}

[[nodiscard]] const char*
toString(BladeThicknessLaw value) noexcept
{
  switch (value) {
    case BladeThicknessLaw::Constant:
      return "constant";
    case BladeThicknessLaw::Linear:
      return "linear";
    case BladeThicknessLaw::Parabolic:
      return "parabolic";
    case BladeThicknessLaw::Bezier:
      return "bezier";
  }
  return "parabolic";
}

[[nodiscard]] BladeLatticeType
bladeLatticeFromString(const std::string& value) noexcept
{
  return value == "spatial" ? BladeLatticeType::Spatial : BladeLatticeType::Cylindrical;
}

[[nodiscard]] BladeAngleLaw
bladeAngleLawFromString(const std::string& value) noexcept
{
  if (value == "constant") {
    return BladeAngleLaw::Constant;
  }
  if (value == "quadratic") {
    return BladeAngleLaw::Quadratic;
  }
  if (value == "bezier") {
    return BladeAngleLaw::Bezier;
  }
  return BladeAngleLaw::Linear;
}

[[nodiscard]] BladeThicknessLaw
bladeThicknessLawFromString(const std::string& value) noexcept
{
  if (value == "constant") {
    return BladeThicknessLaw::Constant;
  }
  if (value == "linear") {
    return BladeThicknessLaw::Linear;
  }
  if (value == "bezier") {
    return BladeThicknessLaw::Bezier;
  }
  return BladeThicknessLaw::Parabolic;
}

void
toJson(nlohmann::json& json, const PumpParams& params)
{
  json = nlohmann::json{
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
    {"qM3s", params.qM3s},
  };
}

void
toJson(nlohmann::json& json, const BladeDesignParams& params)
{
  json = nlohmann::json{
    {"latticeType", toString(params.latticeType)},
    {"bladeCount", params.bladeCount},
    {"flowRateM3s", params.flowRateM3s},
    {"rpm", params.rpm},
    {"designHeadM", params.designHeadM},
    {"autoInletAngle", params.autoInletAngle},
    {"autoOutletAngle", params.autoOutletAngle},
    {"beta1Deg", params.beta1Deg},
    {"beta2Deg", params.beta2Deg},
    {"angleLaw", toString(params.angleLaw)},
    {"s1Mm", params.s1Mm},
    {"s2Mm", params.s2Mm},
    {"sMaxMm", params.sMaxMm},
    {"thicknessLaw", toString(params.thicknessLaw)},
    {"leadingEdgeBulgeMm", params.leadingEdgeBulgeMm},
    {"trailingEdgeBulgeMm", params.trailingEdgeBulgeMm},
    {"blockageFactor", params.blockageFactor},
    {"slipFactor", params.slipFactor},
    {"autoSlipFactor", params.autoSlipFactor},
    {"hydraulicLossK", params.hydraulicLossK},
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
  params.qM3s = json.value("qM3s", params.qM3s);
  return params;
}

BladeDesignParams
bladeParamsFromJson(const nlohmann::json& json)
{
  BladeDesignParams params;
  params.latticeType = bladeLatticeFromString(json.value("latticeType", std::string("cylindrical")));
  params.bladeCount = json.value("bladeCount", params.bladeCount);
  params.flowRateM3s = json.value("flowRateM3s", params.flowRateM3s);
  params.rpm = json.value("rpm", params.rpm);
  params.designHeadM = json.value("designHeadM", params.designHeadM);
  params.autoInletAngle = json.value("autoInletAngle", params.autoInletAngle);
  params.autoOutletAngle = json.value("autoOutletAngle", params.autoOutletAngle);
  params.beta1Deg = json.value("beta1Deg", params.beta1Deg);
  params.beta2Deg = json.value("beta2Deg", params.beta2Deg);
  params.angleLaw = bladeAngleLawFromString(json.value("angleLaw", std::string("linear")));
  params.s1Mm = json.value("s1Mm", params.s1Mm);
  params.s2Mm = json.value("s2Mm", params.s2Mm);
  params.sMaxMm = json.value("sMaxMm", params.sMaxMm);
  params.thicknessLaw =
    bladeThicknessLawFromString(json.value("thicknessLaw", std::string("parabolic")));
  params.leadingEdgeBulgeMm = json.value("leadingEdgeBulgeMm", params.leadingEdgeBulgeMm);
  params.trailingEdgeBulgeMm = json.value("trailingEdgeBulgeMm", params.trailingEdgeBulgeMm);
  params.blockageFactor = json.value("blockageFactor", params.blockageFactor);
  params.slipFactor = json.value("slipFactor", params.slipFactor);
  params.autoSlipFactor = json.value("autoSlipFactor", params.autoSlipFactor);
  params.hydraulicLossK = json.value("hydraulicLossK", params.hydraulicLossK);
  return params;
}

nlohmann::json
projectToJson(const ProjectData& project)
{
  PumpParams pumpParams = project.pumpParams;
  pumpParams.qM3s = project.bladeDesign.flowRateM3s;
  nlohmann::json pumpJson;
  nlohmann::json bladeJson;
  toJson(pumpJson, pumpParams);
  toJson(bladeJson, project.bladeDesign);
  return nlohmann::json{
    {"version", FILE_VERSION},
    {"pumpParams", std::move(pumpJson)},
    {"bladeDesign", std::move(bladeJson)},
  };
}

ProjectData
projectFromJson(const nlohmann::json& json)
{
  ProjectData project;
  if (json.contains("pumpParams")) {
    project.pumpParams = fromJson(json.at("pumpParams"));
  } else {
    project.pumpParams = fromJson(json);
  }
  if (json.contains("bladeDesign")) {
    project.bladeDesign = bladeParamsFromJson(json.at("bladeDesign"));
    project.pumpParams.qM3s = project.bladeDesign.flowRateM3s;
  } else {
    project.bladeDesign.flowRateM3s = project.pumpParams.qM3s;
  }
  return project;
}

}

Result<void>
saveParams(const PumpParams& params, const std::filesystem::path& path) noexcept
{
  ProjectData project;
  project.pumpParams = params;
  project.bladeDesign.flowRateM3s = params.qM3s;
  return saveProject(project, path);
}

Result<PumpParams>
loadParams(const std::filesystem::path& path) noexcept
{
  auto project = loadProject(path);
  if (!project) {
    return std::unexpected(project.error());
  }
  return project->pumpParams;
}

Result<void>
saveProject(const ProjectData& project, const std::filesystem::path& path) noexcept
{
  try {
    nlohmann::json json = projectToJson(project);

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

Result<ProjectData>
loadProject(const std::filesystem::path& path) noexcept
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

    return projectFromJson(json);
  } catch (...) {
    return std::unexpected(CoreError::ParseError);
  }
}

}
