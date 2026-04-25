#pragma once

#include "core/async_flow_solver.hpp"

#include <chrono>
#include <string>
#include <string_view>

namespace ggm::gui {

struct DockspaceActions
{
  bool requestNew = false;
  bool requestOpen = false;
  bool requestSave = false;
  bool requestSaveAs = false;
  bool requestUndo = false;
  bool requestRedo = false;
  bool requestQuit = false;
};

struct DockspaceLayout
{
  DockspaceActions actions;
  unsigned int rootDockspaceId = 0;
  unsigned int meridionalDockspaceId = 0;
  unsigned int reverseDesignDockspaceId = 0;
};

/// Build fullscreen dockspace with main menu bar.
/// Returns requested actions plus nested dockspaces for each top-level module.
[[nodiscard]] DockspaceLayout
buildDockspace(bool canUndo, bool canRedo) noexcept;

/// Status bar pinned to the bottom of the screen. Shows the current file
/// name (or "без имени"), solver status with color, last computation time,
/// and FPS.
void
drawStatusBar(std::string_view fileName,
              core::SolverStatus solverStatus,
              std::chrono::milliseconds lastDuration) noexcept;

}
