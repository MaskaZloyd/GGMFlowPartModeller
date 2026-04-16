#pragma once

#include "core/error.hpp"
#include "core/pump_params.hpp"

#include <filesystem>

namespace ggm::core {

[[nodiscard]] Result<void>
saveParams(const PumpParams& params, const std::filesystem::path& path) noexcept;

[[nodiscard]] Result<PumpParams>
loadParams(const std::filesystem::path& path) noexcept;

} // namespace ggm::core
