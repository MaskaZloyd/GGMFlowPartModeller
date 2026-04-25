#pragma once

#include <imgui.h>
#include <imgui_internal.h>

namespace ggm::gui {

[[nodiscard]] inline ImGuiWindowClass
makeDockspaceWindowClass(const ImGuiID dockspaceId) noexcept
{
  ImGuiWindowClass windowClass;
  windowClass.ClassId = dockspaceId;
  windowClass.DockingAllowUnclassed = false;
  return windowClass;
}

inline void
prepareDockedWindow(const char*, const ImGuiID dockspaceId) noexcept
{
  if (dockspaceId == 0) {
    return;
  }

  /// Restrict docking to windows of the same class (same module). Cross-module
  /// drops are rejected by ImGui based on this class, not by forcing the dock
  /// ID every frame — which would also prevent the user from rearranging
  /// windows within their own module.
  const ImGuiWindowClass windowClass = makeDockspaceWindowClass(dockspaceId);
  ImGui::SetNextWindowClass(&windowClass);

  /// Initial placement only. After the first frame, the user freely moves,
  /// retabs, or floats the window within the module's dockspace.
  ImGui::SetNextWindowDockID(dockspaceId, ImGuiCond_FirstUseEver);
}

}
