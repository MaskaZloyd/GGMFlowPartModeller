#pragma once

#include "core/blade_params.hpp"
#include "core/error.hpp"
#include "core/pump_params.hpp"

#include <filesystem>

namespace ggm::core {

struct ProjectData
{
  PumpParams pumpParams;
  BladeDesignParams bladeDesign;
};

[[nodiscard]] Result<void>
saveParams(const PumpParams& params, const std::filesystem::path& path) noexcept;

[[nodiscard]] Result<PumpParams>
loadParams(const std::filesystem::path& path) noexcept;

[[nodiscard]] Result<void>
saveProject(const ProjectData& project, const std::filesystem::path& path) noexcept;

[[nodiscard]] Result<ProjectData>
loadProject(const std::filesystem::path& path) noexcept;

}
