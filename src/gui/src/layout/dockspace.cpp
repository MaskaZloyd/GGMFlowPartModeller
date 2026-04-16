#include "gui/layout/dockspace.hpp"

#include "gui/solver_status_display.hpp"

#include <imgui.h>

#include <cstdio>

namespace ggm::gui {

DockspaceActions buildDockspace(bool canUndo, bool canRedo) noexcept {
  DockspaceActions actions;

  // Fullscreen dockspace window
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                 ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));

  ImGui::Begin("##DockspaceRoot", nullptr, windowFlags);
  ImGui::PopStyleVar(3);

  ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
  ImGui::DockSpace(dockspaceId, ImVec2(0.0F, 0.0F), ImGuiDockNodeFlags_PassthruCentralNode);

  // Main menu bar
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New", "Ctrl+N")) {
        actions.requestNew = true;
      }
      if (ImGui::MenuItem("Open...", "Ctrl+O")) {
        actions.requestOpen = true;
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Save", "Ctrl+S")) {
        actions.requestSave = true;
      }
      if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
        actions.requestSaveAs = true;
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Quit", "Ctrl+Q")) {
        actions.requestQuit = true;
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo)) {
        actions.requestUndo = true;
      }
      if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo)) {
        actions.requestRedo = true;
      }
      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  ImGui::End();

  // Keyboard shortcuts
  auto& imguiIo = ImGui::GetIO();
  bool ctrl = imguiIo.KeyCtrl;
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z) && canUndo) {
    actions.requestUndo = true;
  }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y) && canRedo) {
    actions.requestRedo = true;
  }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
    if (imguiIo.KeyShift) {
      actions.requestSaveAs = true;
    } else {
      actions.requestSave = true;
    }
  }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
    actions.requestOpen = true;
  }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
    actions.requestNew = true;
  }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Q)) {
    actions.requestQuit = true;
  }

  return actions;
}

void drawStatusBar(std::string_view fileName,
                   core::SolverStatus solverStatus,
                   std::chrono::milliseconds lastDuration) noexcept {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float height = ImGui::GetFrameHeight() + 6.0F;
  ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - height));
  ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, height));
  ImGui::SetNextWindowViewport(viewport->ID);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                           ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoBringToFrontOnFocus;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0F, 3.0F));
  ImGui::Begin("##StatusBar", nullptr, flags);
  ImGui::PopStyleVar(3);

  // File: filename or placeholder.
  ImGui::TextDisabled("Файл:");
  ImGui::SameLine();
  if (fileName.empty()) {
    ImGui::TextUnformatted("(без имени)");
  } else {
    ImGui::TextUnformatted(fileName.data());
  }

  ImGui::SameLine();
  ImGui::TextDisabled(" | ");
  ImGui::SameLine();
  ImGui::TextDisabled("Статус:");
  ImGui::SameLine();
  const auto display = solverStatusBar(solverStatus);
  ImGui::TextColored(display.color, "%s", display.label);

  if (lastDuration.count() > 0) {
    ImGui::SameLine();
    ImGui::TextDisabled("(%lld мс)", static_cast<long long>(lastDuration.count()));
  }

  // FPS on the right.
  float fps = ImGui::GetIO().Framerate;
  char fpsBuf[32];
  std::snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %.0f", static_cast<double>(fps));
  float textW = ImGui::CalcTextSize(fpsBuf).x;
  ImGui::SameLine(ImGui::GetWindowWidth() - textW - 14.0F);
  ImGui::TextDisabled("%s", fpsBuf);

  ImGui::End();
}

} // namespace ggm::gui
