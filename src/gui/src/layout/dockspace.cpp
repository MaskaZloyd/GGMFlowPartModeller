#include "gui/layout/dockspace.hpp"

#include "gui/solver_status_display.hpp"

#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

namespace ggm::gui {

namespace {

constexpr const char* ROOT_DOCKSPACE_WINDOW = "##DockspaceRoot";
constexpr const char* ROOT_DOCKSPACE_NAME = "MainDockSpace";
constexpr const char* MODULE_WINDOW_TITLE = "Меридианное сечение";
constexpr const char* MODULE_DOCKSPACE_NAME = "MeridionalSectionDockSpace";

void
buildMeridionalSectionLayout(ImGuiID dockspaceId, ImVec2 size) noexcept
{
  if (dockspaceId == 0 || ImGui::DockBuilderGetNode(dockspaceId) != nullptr) {
    return;
  }

  ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspaceId, size);

  ImGuiID geometryNode = dockspaceId;
  ImGuiID logNode =
    ImGui::DockBuilderSplitNode(geometryNode, ImGuiDir_Down, 0.24F, nullptr, &geometryNode);
  ImGuiID sidebarNode =
    ImGui::DockBuilderSplitNode(geometryNode, ImGuiDir_Left, 0.24F, nullptr, &geometryNode);
  ImGuiID chartsNode =
    ImGui::DockBuilderSplitNode(geometryNode, ImGuiDir_Right, 0.30F, nullptr, &geometryNode);
  ImGuiID settingsNode =
    ImGui::DockBuilderSplitNode(sidebarNode, ImGuiDir_Down, 0.40F, nullptr, &sidebarNode);

  ImGui::DockBuilderDockWindow("Параметры", sidebarNode);
  ImGui::DockBuilderDockWindow("Настройки", settingsNode);
  ImGui::DockBuilderDockWindow("Геометрия", geometryNode);
  ImGui::DockBuilderDockWindow("Log", logNode);
  ImGui::DockBuilderDockWindow("График: Площадь F(s)", chartsNode);
  ImGui::DockBuilderDockWindow("График: Скорость |V|", chartsNode);
  ImGui::DockBuilderDockWindow("График: ψ средней линии", chartsNode);
  ImGui::DockBuilderFinish(dockspaceId);
}

[[nodiscard]] ImGuiID
drawMeridionalSectionModule(ImGuiID rootDockspaceId) noexcept
{
  if (rootDockspaceId != 0) {
    ImGui::SetNextWindowDockID(rootDockspaceId, ImGuiCond_FirstUseEver);
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (viewport != nullptr) {
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_FirstUseEver);
  }

  ImGuiWindowFlags moduleFlags =
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

  ImGuiID moduleDockspaceId = 0;
  if (ImGui::Begin(MODULE_WINDOW_TITLE, nullptr, moduleFlags)) {
    moduleDockspaceId = ImGui::GetID(MODULE_DOCKSPACE_NAME);
    buildMeridionalSectionLayout(moduleDockspaceId, ImGui::GetContentRegionAvail());
    ImGui::DockSpace(moduleDockspaceId, ImVec2(0.0F, 0.0F));
  }
  ImGui::End();

  return moduleDockspaceId;
}

} // namespace

DockspaceLayout
buildDockspace(bool canUndo, bool canRedo) noexcept
{
  DockspaceLayout layout;

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

  ImGui::Begin(ROOT_DOCKSPACE_WINDOW, nullptr, windowFlags);
  ImGui::PopStyleVar(3);

  ImGuiID dockspaceId = ImGui::GetID(ROOT_DOCKSPACE_NAME);
  ImGui::DockSpace(dockspaceId, ImVec2(0.0F, 0.0F), ImGuiDockNodeFlags_PassthruCentralNode);

  // Main menu bar
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New", "Ctrl+N")) {
        layout.actions.requestNew = true;
      }
      if (ImGui::MenuItem("Open...", "Ctrl+O")) {
        layout.actions.requestOpen = true;
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Save", "Ctrl+S")) {
        layout.actions.requestSave = true;
      }
      if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
        layout.actions.requestSaveAs = true;
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Quit", "Ctrl+Q")) {
        layout.actions.requestQuit = true;
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo)) {
        layout.actions.requestUndo = true;
      }
      if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo)) {
        layout.actions.requestRedo = true;
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
    layout.actions.requestUndo = true;
  }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y) && canRedo) {
    layout.actions.requestRedo = true;
  }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
    if (imguiIo.KeyShift) {
      layout.actions.requestSaveAs = true;
    } else {
      layout.actions.requestSave = true;
    }
  }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
    layout.actions.requestOpen = true;
  }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
    layout.actions.requestNew = true;
  }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Q)) {
    layout.actions.requestQuit = true;
  }

  layout.moduleDockspaceId = drawMeridionalSectionModule(dockspaceId);

  return layout;
}

void
drawStatusBar(std::string_view fileName,
              core::SolverStatus solverStatus,
              std::chrono::milliseconds lastDuration) noexcept
{
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
