#pragma once

#include "core/async_flow_solver.hpp"
#include "core/computation_settings.hpp"
#include "gui/renderer/render_settings.hpp"

#include <chrono>

namespace ggm::gui {

struct SettingsPanelResult
{
  core::ComputationSettings compSettings;
  RenderSettings renderSettings;
  bool compSettingsChanged = false;
  bool renderSettingsChanged = false;
  bool recomputeRequested = false;
  bool cancelRequested = false;
};

SettingsPanelResult
drawSettingsPanel(const core::ComputationSettings& compSettings,
                  const RenderSettings& renderSettings,
                  core::SolverStatus solverStatus,
                  std::chrono::milliseconds lastDuration,
                  unsigned int dockspaceId) noexcept;

}
