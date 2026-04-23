#include "gui/panels/charts_panel.hpp"

#include "core/flow_solver_types.hpp"

#include <algorithm>
#include <limits>
#include <vector>
#include <cstdio>

#include <imgui.h>
#include <implot.h>

namespace ggm::gui {

namespace {

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

} // namespace

void
drawChartsPanel(const core::FlowResults* flow, bool flowValid) noexcept
{
  ImGui::Begin("Графики");

  if (!flowValid || flow == nullptr) {
    ImGui::TextDisabled("Нет данных расчёта");
    ImGui::End();
    return;
  }

  const auto& area = flow->areaProfile;
  if (area.arcLengths.empty()) {
    ImGui::TextDisabled("Профиль площади не рассчитан");
    ImGui::End();
    return;
  }

  auto count = static_cast<int>(area.arcLengths.size());
  ImVec2 avail = ImGui::GetContentRegionAvail();
  // 3 plots, so divide by 3. Minimum 200px per plot.
  float chartH = std::max(200.0F, (avail.y - 32.0F) / 3.0F);

  // ---- Area profile F(s) ----
  if (ImPlot::BeginPlot("Профиль площади F(s)", ImVec2(-1, chartH), ImPlotFlags_Crosshairs)) {
    ImPlot::SetupAxes("Длина дуги s, мм", "Площадь F, мм²", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
    ImPlot::SetupLegend(ImPlotLocation_NorthEast, ImPlotLegendFlags_Outside);
    ImPlot::SetupMouseText(ImPlotLocation_SouthEast);

    ImPlot::PushStyleColor(ImPlotCol_Fill, ImVec4(0.32F, 0.47F, 0.87F, 0.12F));
    ImPlot::PlotShaded("##fill", area.arcLengths.data(), area.flowAreas.data(), count, 0.0);
    ImPlot::PopStyleColor();

    ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.32F, 0.47F, 0.87F, 1.0F));
    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.2F);
    ImPlot::PlotLine("Площадь проходного сечения F(s)", area.arcLengths.data(), area.flowAreas.data(), count);
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

  ImGui::Spacing();

  // ---- Velocities |V| along streamlines ----
  if (ImPlot::BeginPlot("Скорость |V| вдоль линий тока", ImVec2(-1, chartH), ImPlotFlags_Crosshairs)) {
    ImPlot::SetupAxes("Длина дуги линии тока s, мм", "Модуль скорости |V|, м/с", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
    ImPlot::SetupLegend(ImPlotLocation_NorthEast, ImPlotLegendFlags_Outside);
    ImPlot::SetupMouseText(ImPlotLocation_SouthEast);

    ImPlot::PushColormap(ImPlotColormap_Spectral);

    const auto& vels = flow->velocities;
    for (std::size_t i = 0; i < vels.size(); ++i) {
      const auto& val = vels[i];
      int scount = static_cast<int>(val.samples.size());
      if (scount == 0) continue;
      
      static std::vector<double> sArr;
      static std::vector<double> vArr;
      sArr.resize(std::max(sArr.size(), static_cast<std::size_t>(scount)));
      vArr.resize(std::max(vArr.size(), static_cast<std::size_t>(scount)));
      
      for (int j = 0; j < scount; ++j) {
        sArr[static_cast<std::size_t>(j)] = val.samples[static_cast<std::size_t>(j)].arcLength;
        vArr[static_cast<std::size_t>(j)] = val.samples[static_cast<std::size_t>(j)].speed;
      }
      
      char label[64];
      std::snprintf(label, sizeof(label), "ψ = %.2f", val.psiLevel);
      ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.5F);
      ImPlot::PlotLine(label, sArr.data(), vArr.data(), scount);
      ImPlot::PopStyleVar();
    }
    ImPlot::PopColormap();
    ImPlot::EndPlot();
  }

  ImGui::Spacing();

  // ---- ψ along midline ----
  if (ImPlot::BeginPlot("Распределение ψ на средней линии", ImVec2(-1, chartH), ImPlotFlags_Crosshairs)) {
    ImPlot::SetupAxes("Длина дуги s, мм", "ψ (нормированная относительная)", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_None);
    ImPlot::SetupAxisLimits(ImAxis_Y1, -0.05, 1.05, ImPlotCond_Always);
    ImPlot::SetupLegend(ImPlotLocation_SouthEast, ImPlotLegendFlags_Outside);
    ImPlot::SetupMouseText(ImPlotLocation_NorthEast);

    auto psiMid = psiAlongMidline(*flow);
    int psiCount = std::min(static_cast<int>(psiMid.size()), count);
    if (psiCount > 1) {
      ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.20F, 0.62F, 0.40F, 1.0F));
      ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.5F);
      ImPlot::PlotLine("ψ(s)", area.arcLengths.data(), psiMid.data(), psiCount);
      ImPlot::PopStyleVar();
      ImPlot::PopStyleColor();
    }

    double refY = 0.5;
    ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.70F, 0.70F, 0.74F, 0.8F));
    ImPlot::PlotInfLines("Идеальная средняя линия (0.5)", &refY, 1, ImPlotInfLinesFlags_Horizontal);
    ImPlot::PopStyleColor();

    ImPlot::EndPlot();
  }

  ImGui::End();
}

} // namespace ggm::gui
