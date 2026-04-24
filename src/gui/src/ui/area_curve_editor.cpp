#include "gui/ui/area_curve_editor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <span>
#include <vector>

#include <imgui.h>
#include <implot.h>

namespace ggm::ui {

namespace {

constexpr double kMinAreaValue = 1e-6;
constexpr float kPlotHeight = 320.0F;

[[nodiscard]] std::vector<ggm::core::TargetAreaPoint>
makeDefaultCurvePoints()
{
  return {
    {0.0, 0.55},
    {0.40, 0.78},
    {0.75, 0.92},
    {1.0, 1.00},
  };
}

void
resetCurve(AreaCurveEditorState& state)
{
  state.targetCurve.setPoints(makeDefaultCurvePoints());
  state.curveChanged = true;
  state.requestPreview = true;
  state.selectedPoint = -1;
  state.dragInProgress = false;
  state.editedWhileDragging = false;
}

void
enforceCurveInvariants(ggm::core::TargetAreaCurve& curve)
{
  curve.sortAndClamp();
  auto points = curve.points();
  if (points.size() < 2U) {
    curve.setPoints(makeDefaultCurvePoints());
    return;
  }

  points.front().xi = 0.0;
  points.back().xi = 1.0;

  for (auto& point : points) {
    point.xi = std::clamp(point.xi, 0.0, 1.0);
    point.value = std::max(point.value, kMinAreaValue);
  }

  curve.sortAndClamp();

  if (curve.points().size() < 2U) {
    curve.setPoints(makeDefaultCurvePoints());
  }
}

[[nodiscard]] int
findClosestPointIndex(std::span<const ggm::core::TargetAreaPoint> points,
                      const double xi,
                      const double value) noexcept
{
  if (points.empty()) {
    return -1;
  }

  double bestDistance = std::numeric_limits<double>::max();
  int bestIndex = -1;

  for (std::size_t i = 0; i < points.size(); ++i) {
    const double dx = points[i].xi - xi;
    const double dy = points[i].value - value;
    const double distance = (dx * dx) + (dy * dy);
    if (distance < bestDistance) {
      bestDistance = distance;
      bestIndex = static_cast<int>(i);
    }
  }

  return bestIndex;
}

[[nodiscard]] double
plotUpperBound(std::span<const ggm::core::TargetAreaPoint> points) noexcept
{
  double maxValue = 1.0;
  for (const auto& point : points) {
    if (std::isfinite(point.value)) {
      maxValue = std::max(maxValue, point.value);
    }
  }
  return std::max(1.2, maxValue * 1.15);
}

void
drawPointsTable(AreaCurveEditorState& state)
{
  const auto points = state.targetCurve.points();
  if (!ImGui::BeginTable(
        "##TargetAreaPoints", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    return;
  }

  ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28.0F);
  ImGui::TableSetupColumn("xi");
  ImGui::TableSetupColumn("F_norm");
  ImGui::TableHeadersRow();

  for (std::size_t i = 0; i < points.size(); ++i) {
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    const bool selected = state.selectedPoint == static_cast<int>(i);
    char label[16];
    std::snprintf(label, sizeof(label), "%zu", i);
    if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_SpanAllColumns)) {
      state.selectedPoint = static_cast<int>(i);
    }

    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%.3f", points[i].xi);

    ImGui::TableSetColumnIndex(2);
    ImGui::Text("%.3f", points[i].value);
  }

  ImGui::EndTable();
}

} // namespace

void
drawAreaCurveEditor(AreaCurveEditorState& state)
{
  state.curveChanged = false;
  state.requestPreview = false;

  if (!state.targetCurve.isValid()) {
    resetCurve(state);
  }

  enforceCurveInvariants(state.targetCurve);

  ImGui::TextDisabled(
    "Двойной щелчок по графику добавляет точку. Внутренние точки удаляются клавишей Delete.");

  if (ImGui::Button("Сбросить кривую")) {
    resetCurve(state);
  }

  ImGui::Separator();

  auto points = state.targetCurve.points();
  std::vector<double> xs(points.size(), 0.0);
  std::vector<double> ys(points.size(), 0.0);
  for (std::size_t i = 0; i < points.size(); ++i) {
    xs[i] = points[i].xi;
    ys[i] = points[i].value;
  }

  bool anyPointHeld = false;
  bool immediatePreview = false;
  double selectionXi = 0.0;
  double selectionValue = 0.0;
  bool updateSelectionFromCoordinates = false;

  if (ImPlot::BeginPlot(
        "##TargetAreaCurveEditor", ImVec2(-1, kPlotHeight), ImPlotFlags_Crosshairs)) {
    ImPlot::SetupAxes("xi", "F_target_norm", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
    ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 1.0, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, plotUpperBound(points), ImPlotCond_Always);
    ImPlot::SetupMouseText(ImPlotLocation_SouthEast);

    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.5F);
    ImPlot::PlotLine("Целевая кривая", xs.data(), ys.data(), static_cast<int>(xs.size()));
    ImPlot::PopStyleVar();

    ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 5.5F, ImVec4(0.18F, 0.42F, 0.80F, 1.0F));
    ImPlot::PlotScatter("Опорные точки", xs.data(), ys.data(), static_cast<int>(xs.size()));

    for (std::size_t i = 0; i < points.size(); ++i) {
      bool clicked = false;
      bool hovered = false;
      bool held = false;

      double xi = points[i].xi;
      double value = points[i].value;

      const bool changed = ImPlot::DragPoint(static_cast<int>(i),
                                             &xi,
                                             &value,
                                             ImVec4(0.90F, 0.28F, 0.18F, 1.0F),
                                             6.0F,
                                             ImPlotDragToolFlags_None,
                                             &clicked,
                                             &hovered,
                                             &held);

      if (clicked) {
        state.selectedPoint = static_cast<int>(i);
      }

      if (hovered) {
        ImPlot::Annotation(points[i].xi,
                           points[i].value,
                           ImVec4(0.15F, 0.15F, 0.15F, 1.0F),
                           ImVec2(10.0F, -10.0F),
                           true,
                           "xi=%.3f\nF=%.3f",
                           points[i].xi,
                           points[i].value);
      }

      if (changed) {
        if (i == 0U) {
          xi = 0.0;
        } else if (i + 1U == points.size()) {
          xi = 1.0;
        } else {
          xi = std::clamp(xi, 0.0, 1.0);
        }

        value = std::max(value, kMinAreaValue);

        points[i].xi = xi;
        points[i].value = value;
        state.curveChanged = true;
        selectionXi = xi;
        selectionValue = value;
        updateSelectionFromCoordinates = true;

        if (held) {
          state.editedWhileDragging = true;
        } else {
          immediatePreview = true;
        }
      }

      anyPointHeld = anyPointHeld || held;
    }

    if (ImPlot::IsPlotHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      const ImPlotPoint mouse = ImPlot::GetPlotMousePos();
      auto editablePoints = std::vector<ggm::core::TargetAreaPoint>(points.begin(), points.end());
      editablePoints.push_back({
        .xi = std::clamp(mouse.x, 0.0, 1.0),
        .value = std::max(mouse.y, kMinAreaValue),
      });
      state.targetCurve.setPoints(std::move(editablePoints));
      state.curveChanged = true;
      state.requestPreview = true;
      selectionXi = std::clamp(mouse.x, 0.0, 1.0);
      selectionValue = std::max(mouse.y, kMinAreaValue);
      updateSelectionFromCoordinates = true;
    }

    ImPlot::EndPlot();
  }

  enforceCurveInvariants(state.targetCurve);

  if (updateSelectionFromCoordinates) {
    state.selectedPoint =
      findClosestPointIndex(state.targetCurve.points(), selectionXi, selectionValue);
  } else if (state.selectedPoint >= static_cast<int>(state.targetCurve.points().size())) {
    state.selectedPoint = -1;
  }

  if (anyPointHeld) {
    state.dragInProgress = true;
  } else if (state.dragInProgress) {
    state.dragInProgress = false;
    if (state.editedWhileDragging) {
      state.requestPreview = true;
      state.editedWhileDragging = false;
    }
  }

  const auto currentPoints = state.targetCurve.points();
  const bool canDeleteSelected =
    state.selectedPoint > 0 && state.selectedPoint + 1 < static_cast<int>(currentPoints.size());
  const bool deletePressed = canDeleteSelected &&
                             ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                             ImGui::IsKeyPressed(ImGuiKey_Delete, false);

  if (deletePressed) {
    auto editablePoints =
      std::vector<ggm::core::TargetAreaPoint>(currentPoints.begin(), currentPoints.end());
    editablePoints.erase(editablePoints.begin() + state.selectedPoint);
    state.targetCurve.setPoints(std::move(editablePoints));
    state.selectedPoint = -1;
    state.curveChanged = true;
    state.requestPreview = true;
    state.dragInProgress = false;
    state.editedWhileDragging = false;
  }

  ImGui::BeginDisabled(!canDeleteSelected);
  if (ImGui::Button("Удалить выбранную точку")) {
    auto editablePoints =
      std::vector<ggm::core::TargetAreaPoint>(currentPoints.begin(), currentPoints.end());
    editablePoints.erase(editablePoints.begin() + state.selectedPoint);
    state.targetCurve.setPoints(std::move(editablePoints));
    state.selectedPoint = -1;
    state.curveChanged = true;
    state.requestPreview = true;
    state.dragInProgress = false;
    state.editedWhileDragging = false;
  }
  ImGui::EndDisabled();

  if (state.curveChanged && immediatePreview) {
    state.requestPreview = true;
  }

  ImGui::Spacing();
  drawPointsTable(state);
}

} // namespace ggm::ui
