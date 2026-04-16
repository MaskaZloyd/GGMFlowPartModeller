#include "gui/panels/settings_panel.hpp"

#include "gui/solver_status_display.hpp"

#include <imgui.h>

#include <cmath>

namespace ggm::gui {

namespace {

void drawStatusIndicator(core::SolverStatus status,
                         std::chrono::milliseconds lastDuration) noexcept {
  const auto display = solverStatusPanel(status);

  // Colored dot + text
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  ImVec2 cursor = ImGui::GetCursorScreenPos();
  float dotRadius = 6.0F;
  float lineHeight = ImGui::GetTextLineHeight();
  ImVec2 dotCenter(cursor.x + dotRadius,
                   cursor.y + (lineHeight * 0.5F));

  // Pulsating animation for Running state
  ImU32 dotColor = ImGui::ColorConvertFloat4ToU32(display.color);
  if (status == core::SolverStatus::Running) {
    float pulse =
        0.5F + 0.5F * std::sin(static_cast<float>(ImGui::GetTime()) * 4.0F);
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

SettingsPanelResult drawSettingsPanel(const core::ComputationSettings& compSettings,
                                     const RenderSettings& renderSettings,
                                     core::SolverStatus solverStatus,
                                     std::chrono::milliseconds lastDuration) noexcept {
  SettingsPanelResult result;
  result.compSettings = compSettings;
  result.renderSettings = renderSettings;

  ImGui::Begin("Настройки");

  auto& comp = result.compSettings;
  auto& rend = result.renderSettings;

  // Status indicator + action buttons up top
  ImGui::Text("Статус расчёта:");
  ImGui::SameLine();
  drawStatusIndicator(solverStatus, lastDuration);

  bool running = (solverStatus == core::SolverStatus::Running);

  if (running) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Пересчитать", ImVec2(-1, 30))) {
    result.recomputeRequested = true;
  }
  if (running) {
    ImGui::EndDisabled();
  }

  if (!running) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Отменить", ImVec2(-1, 0))) {
    result.cancelRequested = true;
  }
  if (!running) {
    ImGui::EndDisabled();
  }

  ImGui::Separator();

  if (ImGui::CollapsingHeader("Расчёт", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::SliderInt("Точек NURBS", &comp.nurbsEvalPoints, 50, 2000);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Количество точек для отображения кривых NURBS");
    }

    ImGui::SliderInt("Сечений вдоль (Nh)", &comp.nh, 50, 500);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Разрешение сетки вдоль потока");
    }

    ImGui::SliderInt("Сечений поперёк (M)", &comp.m, 10, 200);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Разрешение сетки поперёк канала");
    }

    ImGui::SliderInt("Линий тока", &comp.streamlineCount, 1, 20);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Количество линий тока между втулкой и покрывным диском");
    }
  }

  if (ImGui::CollapsingHeader("Отображение", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Координатная сетка", &rend.showCoordGrid);
    ImGui::Checkbox("Расчётная сетка", &rend.showComputationalGrid);
    ImGui::SliderFloat("Толщина втулки", &rend.hubLineWidth, 1.0F, 5.0F, "%.1f");
    ImGui::SliderFloat("Толщина покр. диска", &rend.shroudLineWidth, 1.0F, 5.0F, "%.1f");
    ImGui::SliderFloat("Толщина ср. линии", &rend.meanLineWidth, 0.5F, 3.0F, "%.1f");
    ImGui::SliderFloat("Толщина линий тока", &rend.streamlineWidth, 0.5F, 3.0F, "%.1f");
  }

  ImGui::End();

  result.compSettingsChanged = !(comp == compSettings);
  result.renderSettingsChanged =
      (rend.showCoordGrid != renderSettings.showCoordGrid) ||
      (rend.showComputationalGrid != renderSettings.showComputationalGrid) ||
      (rend.hubLineWidth != renderSettings.hubLineWidth) ||
      (rend.shroudLineWidth != renderSettings.shroudLineWidth) ||
      (rend.meanLineWidth != renderSettings.meanLineWidth) ||
      (rend.streamlineWidth != renderSettings.streamlineWidth);

  return result;
}

} // namespace ggm::gui
