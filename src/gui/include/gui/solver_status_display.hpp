#pragma once

#include "core/async_flow_solver.hpp"

#include <imgui.h>

namespace ggm::gui {

struct StatusDisplay
{
  ImVec4 color;
  const char* label;
};

/// Status-bar styling: saturated utility colors for the compact footer.
[[nodiscard]] StatusDisplay
solverStatusBar(core::SolverStatus status) noexcept;

/// Settings panel styling: soft UI-theme colors for the indicator block.
[[nodiscard]] StatusDisplay
solverStatusPanel(core::SolverStatus status) noexcept;

}
