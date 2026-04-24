#include "gui/panels/settings_panel.hpp"

#include "gui/solver_status_display.hpp"
#include "gui/ui/panel_style.hpp"
#include "layout/dock_utils.hpp"

#include <algorithm>
#include <cmath>

#include <imgui.h>

namespace ggm::gui {

namespace {

bool
dragIntField(const char* id,
             const char* labelText,
             int* value,
             int minValue,
             int maxValue,
             const char* tooltip = nullptr) noexcept
{
  return style::fixedDragInt(id, labelText, value, minValue, maxValue, tooltip).changed;
}

bool
dragFloatField(const char* id,
               const char* labelText,
               float* value,
               float minValue,
               float maxValue,
               const char* format,
               const char* tooltip = nullptr) noexcept
{
  return style::fixedDragFloat(id,
                               labelText,
                               value,
                               minValue,
                               maxValue,
                               format,
                               style::PANEL_DRAG_SPEED_DEFAULT,
                               tooltip)
    .changed;
}

void
drawStatusIndicator(core::SolverStatus status, std::chrono::milliseconds lastDuration) noexcept
{
  const auto display = solverStatusPanel(status);

  // Colored dot + text
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  ImVec2 cursor = ImGui::GetCursorScreenPos();
  float dotRadius = 6.0F;
  float lineHeight = ImGui::GetTextLineHeight();
  ImVec2 dotCenter(cursor.x + dotRadius, cursor.y + (lineHeight * 0.5F));

  // Pulsating animation for Running state
  ImU32 dotColor = ImGui::ColorConvertFloat4ToU32(display.color);
  if (status == core::SolverStatus::Running) {
    float pulse = 0.5F + 0.5F * std::sin(static_cast<float>(ImGui::GetTime()) * 4.0F);
    ImVec4 pulsed = display.color;
    pulsed.w = 0.5F + 0.5F * pulse;
    dotColor = ImGui::ColorConvertFloat4ToU32(pulsed);
  }

  drawList->AddCircleFilled(dotCenter, dotRadius, dotColor, 16);
  ImGui::Dummy(ImVec2(dotRadius * 2.0F + 4.0F, lineHeight));
  ImGui::SameLine();
  ImGui::TextColored(display.color, "%s", display.label);

  if (lastDuration.count() > 0 && status == core::SolverStatus::Success) {
    ImGui::SameLine();
    ImGui::TextDisabled("(%lld мс)", static_cast<long long>(lastDuration.count()));
  }
}

} // namespace

SettingsPanelResult
drawSettingsPanel(const core::ComputationSettings& compSettings,
                  const RenderSettings& renderSettings,
                  core::SolverStatus solverStatus,
                  std::chrono::milliseconds lastDuration,
                  ImGuiID dockspaceId) noexcept
{
  SettingsPanelResult result;
  result.compSettings = compSettings;
  result.renderSettings = renderSettings;

  prepareDockedWindow("Настройки", dockspaceId);
  style::pushPanelStyle();
  ImGui::Begin("Настройки");

  auto& comp = result.compSettings;
  auto& rend = result.renderSettings;

  // Status indicator + action buttons up top
  ImGui::Text("Статус расчёта:");
  ImGui::SameLine();
  drawStatusIndicator(solverStatus, lastDuration);

  const bool running = (solverStatus == core::SolverStatus::Running);

  if (style::accentButton("Пересчитать", running)) {
    result.recomputeRequested = true;
  }
  if (style::dangerButton("Отменить", !running)) {
    result.cancelRequested = true;
  }

  ImGui::Separator();

  if (ImGui::CollapsingHeader("Расчёт", ImGuiTreeNodeFlags_DefaultOpen)) {
    dragIntField("nurbs_eval_points",
                 "Точек NURBS",
                 &comp.nurbsEvalPoints,
                 50,
                 2000,
                 "Количество точек для отображения кривых NURBS");
    dragIntField("nh", "Сечений вдоль (Nh)", &comp.nh, 50, 500, "Разрешение сетки вдоль потока");
    dragIntField("m", "Сечений поперёк (M)", &comp.m, 10, 200, "Разрешение сетки поперёк канала");
    dragIntField("streamline_count",
                 "Линий тока",
                 &comp.streamlineCount,
                 1,
                 20,
                 "Количество линий тока между втулкой и покрывным диском");
  }

  if (ImGui::CollapsingHeader("Отображение", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Координатная сетка", &rend.showCoordGrid);
    ImGui::Checkbox("Расчётная сетка", &rend.showComputationalGrid);
    ImGui::Checkbox("Тепловая карта |V|", &rend.showVelocityHeatmap);
    ImGui::Checkbox("Цвет л.тока по |V|", &rend.colorStreamlinesBySpeed);
    ImGui::Checkbox("Критические точки (вход/выход)", &rend.showCriticalMarkers);
    ImGui::Checkbox("Hover-инспектор", &rend.showHoverInspector);
    ImGui::Checkbox("Наложить снимок", &rend.showSnapshotOverlay);
    ImGui::Separator();
    dragFloatField("hub_line_width", "Толщина втулки", &rend.hubLineWidth, 1.0F, 5.0F, "%.1f");
    dragFloatField(
      "shroud_line_width", "Толщина покр. диска", &rend.shroudLineWidth, 1.0F, 5.0F, "%.1f");
    dragFloatField("mean_line_width", "Толщина ср. линии", &rend.meanLineWidth, 0.5F, 3.0F, "%.1f");
    dragFloatField(
      "streamline_width", "Толщина линий тока", &rend.streamlineWidth, 0.5F, 3.0F, "%.1f");
  }

  ImGui::End();
  style::popPanelStyle();

  result.compSettingsChanged = !(comp == compSettings);
  result.renderSettingsChanged =
    (rend.showCoordGrid != renderSettings.showCoordGrid) ||
    (rend.showComputationalGrid != renderSettings.showComputationalGrid) ||
    (rend.showVelocityHeatmap != renderSettings.showVelocityHeatmap) ||
    (rend.colorStreamlinesBySpeed != renderSettings.colorStreamlinesBySpeed) ||
    (rend.showCriticalMarkers != renderSettings.showCriticalMarkers) ||
    (rend.showHoverInspector != renderSettings.showHoverInspector) ||
    (rend.showSnapshotOverlay != renderSettings.showSnapshotOverlay) ||
    (rend.hubLineWidth != renderSettings.hubLineWidth) ||
    (rend.shroudLineWidth != renderSettings.shroudLineWidth) ||
    (rend.meanLineWidth != renderSettings.meanLineWidth) ||
    (rend.streamlineWidth != renderSettings.streamlineWidth);

  return result;
}

} // namespace ggm::gui
