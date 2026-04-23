#include "gui/panels/charts_panel.hpp"

#include "core/flow_solver_types.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <vector>

#include <imgui.h>
#include <implot.h>

namespace ggm::gui {

namespace {

constexpr float CHART_WINDOW_MIN_WIDTH = 440.0F;
constexpr float CHART_WINDOW_MIN_HEIGHT = 260.0F;
constexpr float CHART_WINDOW_WIDTH_RATIO = 0.36F;
constexpr float CHART_WINDOW_HEIGHT_RATIO = 0.29F;
constexpr float CHART_WINDOW_BASE_X = 28.0F;
constexpr float CHART_WINDOW_BASE_Y = 54.0F;
constexpr float CHART_WINDOW_CASCADE_X = 24.0F;
constexpr float CHART_WINDOW_CASCADE_Y = 28.0F;

// For each midline row, take ψ at the grid node closest to that row's
// geometric midpoint. Produces the ψ value along the mean streamline —
// useful to verify that the FEM solution actually reaches 0.5 somewhere
// in the middle of the channel.
std::vector<double>
psiAlongMidline(const core::FlowResults& flow)
{
  const auto& sol = flow.solution;
  const auto& mid = flow.areaProfile.midPoints;
  std::vector<double> out;
  if (mid.empty() || sol.grid.nh == 0 || sol.grid.m == 0) {
    return out;
  }
  const int mm = sol.grid.m;
  out.reserve(mid.size());
  for (std::size_t i = 0; i < mid.size(); ++i) {
    int row = std::min(static_cast<int>(i), sol.grid.nh - 1);
    double bestDist = std::numeric_limits<double>::max();
    int bestCol = 0;
    for (int j = 0; j < mm; ++j) {
      double d = (sol.grid.nodes[static_cast<std::size_t>(row * mm + j)] - mid[i]).squaredNorm();
      if (d < bestDist) {
        bestDist = d;
        bestCol = j;
      }
    }
    out.push_back(sol.psi[static_cast<std::size_t>(row * mm + bestCol)]);
  }
  return out;
}

void
setupChartWindow(int cascadeIndex) noexcept
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (viewport == nullptr) {
    return;
  }

  const float width =
    std::max(CHART_WINDOW_MIN_WIDTH, viewport->WorkSize.x * CHART_WINDOW_WIDTH_RATIO);
  const float height =
    std::max(CHART_WINDOW_MIN_HEIGHT, viewport->WorkSize.y * CHART_WINDOW_HEIGHT_RATIO);

  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + CHART_WINDOW_BASE_X +
                                   static_cast<float>(cascadeIndex) * CHART_WINDOW_CASCADE_X,
                                 viewport->WorkPos.y + CHART_WINDOW_BASE_Y +
                                   static_cast<float>(cascadeIndex) * CHART_WINDOW_CASCADE_Y),
                          ImGuiCond_FirstUseEver);
}

[[nodiscard]] bool
beginChartWindow(const char* title, int cascadeIndex) noexcept
{
  setupChartWindow(cascadeIndex);
  return ImGui::Begin(title);
}

void
drawMissingDataText(const char* message) noexcept
{
  ImGui::TextDisabled("%s", message);
}

void
drawAreaProfileChart(const core::AreaProfile& area) noexcept
{
  const int count = static_cast<int>(area.arcLengths.size());
  if (!ImPlot::BeginPlot("##area_profile_plot", ImVec2(-1, -1), ImPlotFlags_Crosshairs)) {
    return;
  }

  ImPlot::SetupAxes(
    "Длина дуги s, мм", "Площадь F, мм²", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
  ImPlot::SetupLegend(ImPlotLocation_NorthWest);
  ImPlot::SetupMouseText(ImPlotLocation_SouthEast);

  ImPlot::PushStyleColor(ImPlotCol_Fill, ImVec4(0.32F, 0.47F, 0.87F, 0.12F));
  ImPlot::PlotShaded("##area_fill", area.arcLengths.data(), area.flowAreas.data(), count, 0.0);
  ImPlot::PopStyleColor();

  ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.32F, 0.47F, 0.87F, 1.0F));
  ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.2F);
  ImPlot::PlotLine(
    "Площадь проходного сечения F(s)", area.arcLengths.data(), area.flowAreas.data(), count);
  ImPlot::PopStyleVar();
  ImPlot::PopStyleColor();

  ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.80F, 0.45F, 0.20F, 0.75F));
  double f1 = area.f1;
  ImPlot::PlotInfLines("F₁ (на входе)", &f1, 1, ImPlotInfLinesFlags_Horizontal);
  double f2 = area.f2;
  ImPlot::PlotInfLines("F₂ (на выходе)", &f2, 1, ImPlotInfLinesFlags_Horizontal);
  ImPlot::PopStyleColor();

  double sStart[] = {area.arcLengths.front()};
  double fStart[] = {area.flowAreas.front()};
  double sEnd[] = {area.arcLengths.back()};
  double fEnd[] = {area.flowAreas.back()};
  ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 5.5F, ImVec4(0.12F, 0.70F, 0.30F, 1.0F));
  ImPlot::PlotScatter("вход", sStart, fStart, 1);
  ImPlot::SetNextMarkerStyle(ImPlotMarker_Diamond, 5.5F, ImVec4(0.85F, 0.22F, 0.18F, 1.0F));
  ImPlot::PlotScatter("выход", sEnd, fEnd, 1);

  ImPlot::EndPlot();
}

void
drawVelocityChart(const std::vector<core::StreamlineVelocity>& velocities) noexcept
{
  if (velocities.empty()) {
    drawMissingDataText("Линии тока не рассчитаны");
    return;
  }

  if (!ImPlot::BeginPlot("##streamline_velocity_plot", ImVec2(-1, -1), ImPlotFlags_Crosshairs)) {
    return;
  }

  ImPlot::SetupAxes("Длина дуги линии тока s, мм",
                    "Модуль скорости |V|, м/с",
                    ImPlotAxisFlags_AutoFit,
                    ImPlotAxisFlags_AutoFit);
  ImPlot::SetupLegend(ImPlotLocation_NorthWest);
  ImPlot::SetupMouseText(ImPlotLocation_SouthEast);
  ImPlot::PushColormap(ImPlotColormap_Spectral);

  for (const auto& streamline : velocities) {
    const int count = static_cast<int>(streamline.samples.size());
    if (count == 0) {
      continue;
    }

    std::vector<double> arcLengths(static_cast<std::size_t>(count));
    std::vector<double> speeds(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
      const auto& sample = streamline.samples[static_cast<std::size_t>(i)];
      arcLengths[static_cast<std::size_t>(i)] = sample.arcLength;
      speeds[static_cast<std::size_t>(i)] = sample.speed;
    }

    char label[64];
    std::snprintf(label, sizeof(label), "ψ = %.2f", streamline.psiLevel);
    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.5F);
    ImPlot::PlotLine(label, arcLengths.data(), speeds.data(), count);
    ImPlot::PopStyleVar();
  }

  ImPlot::PopColormap();
  ImPlot::EndPlot();
}

void
drawMidlinePsiChart(const core::FlowResults& flow) noexcept
{
  const auto& area = flow.areaProfile;
  auto psiMid = psiAlongMidline(flow);
  const int count =
    std::min(static_cast<int>(psiMid.size()), static_cast<int>(area.arcLengths.size()));
  if (count <= 1) {
    drawMissingDataText("Недостаточно точек для графика ψ по средней линии");
    return;
  }

  if (!ImPlot::BeginPlot("##midline_psi_plot", ImVec2(-1, -1), ImPlotFlags_Crosshairs)) {
    return;
  }

  ImPlot::SetupAxes("Длина дуги s, мм",
                    "ψ (нормированная относительная)",
                    ImPlotAxisFlags_AutoFit,
                    ImPlotAxisFlags_None);
  ImPlot::SetupAxisLimits(ImAxis_Y1, -0.05, 1.05, ImPlotCond_Always);
  ImPlot::SetupLegend(ImPlotLocation_SouthEast);
  ImPlot::SetupMouseText(ImPlotLocation_NorthEast);

  ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.20F, 0.62F, 0.40F, 1.0F));
  ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.5F);
  ImPlot::PlotLine("ψ(s)", area.arcLengths.data(), psiMid.data(), count);
  ImPlot::PopStyleVar();
  ImPlot::PopStyleColor();

  double refY = 0.5;
  ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.70F, 0.70F, 0.74F, 0.8F));
  ImPlot::PlotInfLines("Идеальная средняя линия (0.5)", &refY, 1, ImPlotInfLinesFlags_Horizontal);
  ImPlot::PopStyleColor();

  ImPlot::EndPlot();
}

} // namespace

void
drawChartsPanel(const core::FlowResults* flow, bool flowValid) noexcept
{
  if (beginChartWindow("График: Площадь F(s)", 0)) {
    if (!flowValid || flow == nullptr) {
      drawMissingDataText("Нет данных расчёта");
    } else if (flow->areaProfile.arcLengths.empty()) {
      drawMissingDataText("Профиль площади не рассчитан");
    } else {
      drawAreaProfileChart(flow->areaProfile);
    }
  }
  ImGui::End();

  if (beginChartWindow("График: Скорость |V|", 1)) {
    if (!flowValid || flow == nullptr) {
      drawMissingDataText("Нет данных расчёта");
    } else {
      drawVelocityChart(flow->velocities);
    }
  }
  ImGui::End();

  if (beginChartWindow("График: ψ средней линии", 2)) {
    if (!flowValid || flow == nullptr) {
      drawMissingDataText("Нет данных расчёта");
    } else if (flow->areaProfile.arcLengths.empty()) {
      drawMissingDataText("Профиль площади не рассчитан");
    } else {
      drawMidlinePsiChart(*flow);
    }
  }
  ImGui::End();
}

} // namespace ggm::gui
