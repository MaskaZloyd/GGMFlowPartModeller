#include "gui/panels/blade_design_panel.hpp"

#include "core/blade_solver.hpp"
#include "gui/ui/panel_style.hpp"
#include "layout/dock_utils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <numbers>
#include <vector>

#include <imgui.h>
#include <implot.h>

namespace ggm::gui {

namespace {

constexpr const char* kParamsTitle = "Параметры лопаточной системы";
constexpr const char* kPlanTitle = "План РК";
constexpr const char* kConformalTitle = "Конформная диаграмма";
constexpr const char* kCharacteristicsTitle = "Характеристики лопатки";
constexpr const char* kTablesTitle = "Таблицы лопаточной системы";
constexpr const char* kHeadCurveTitle = "Напорная характеристика";

bool
comboAngleLaw(core::BladeAngleLaw& law) noexcept
{
  int value = static_cast<int>(law);
  ImGui::PushID("angle_law");
  ImGui::SetNextItemWidth(style::PANEL_INPUT_WIDTH);
  const bool changed = ImGui::Combo("##v", &value, "constant\0linear\0quadratic\0bezier\0");
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Закон beta(r)");
  ImGui::PopID();
  if (changed) {
    law = static_cast<core::BladeAngleLaw>(value);
  }
  return changed;
}

bool
comboThicknessLaw(core::BladeThicknessLaw& law) noexcept
{
  int value = static_cast<int>(law);
  ImGui::PushID("thickness_law");
  ImGui::SetNextItemWidth(style::PANEL_INPUT_WIDTH);
  const bool changed = ImGui::Combo("##v", &value, "constant\0linear\0parabolic\0bezier\0");
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Закон s(r)");
  ImGui::PopID();
  if (changed) {
    law = static_cast<core::BladeThicknessLaw>(value);
  }
  return changed;
}

bool
comboLatticeType(core::BladeLatticeType& latticeType) noexcept
{
  int value = static_cast<int>(latticeType);
  ImGui::PushID("lattice_type");
  ImGui::SetNextItemWidth(style::PANEL_INPUT_WIDTH);
  const bool changed = ImGui::Combo("##v", &value, "Cylindrical\0Spatial\0");
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Тип решётки");
  ImGui::PopID();
  if (changed) {
    latticeType = static_cast<core::BladeLatticeType>(value);
  }
  return changed;
}

bool
fixedDrag(const char* id,
          const char* label,
          double* value,
          const double* minVal = nullptr,
          const double* maxVal = nullptr,
          const char* format = style::PANEL_DRAG_FORMAT_DOUBLE,
          float speed = style::PANEL_DRAG_SPEED_DEFAULT,
          const char* tooltip = nullptr) noexcept
{
  return style::fixedDragDouble(id, label, value, minVal, maxVal, format, speed, tooltip).changed;
}

bool
fixedDragInt(const char* id,
             const char* label,
             int* value,
             int minVal,
             int maxVal,
             const char* tooltip = nullptr) noexcept
{
  return style::fixedDragInt(id, label, value, minVal, maxVal, tooltip).changed;
}

[[nodiscard]] double
fallbackInletAreaMm2(const core::PumpParams& params) noexcept
{
  return 0.25 * std::numbers::pi * std::max(0.0, params.din * params.din - params.dvt * params.dvt);
}

[[nodiscard]] double
fallbackOutletAreaMm2(const core::PumpParams& params) noexcept
{
  return std::numbers::pi * params.d2 * params.b2;
}

constexpr ImVec4 kPlotBlue{0.18F, 0.39F, 0.78F, 1.0F};
constexpr ImVec4 kPlotOrange{0.88F, 0.34F, 0.16F, 1.0F};
constexpr ImVec4 kPlotGreen{0.18F, 0.58F, 0.34F, 1.0F};
constexpr ImVec4 kPlotViolet{0.44F, 0.30F, 0.72F, 1.0F};
constexpr ImVec4 kPlotGray{0.42F, 0.46F, 0.54F, 1.0F};

void
setupReadablePlot(ImPlotLocation legendLocation = ImPlotLocation_NorthEast) noexcept
{
  ImPlot::SetupLegend(legendLocation);
  ImPlot::SetupMouseText(ImPlotLocation_SouthEast);
}

void
plotLineStyled(const char* label,
               const std::vector<double>& x,
               const std::vector<double>& y,
               ImVec4 color,
               float width = 2.2F) noexcept
{
  if (x.empty() || y.empty()) {
    return;
  }
  ImPlot::PushStyleColor(ImPlotCol_Line, color);
  ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, width);
  ImPlot::PlotLine(label, x.data(), y.data(), static_cast<int>(std::min(x.size(), y.size())));
  ImPlot::PopStyleVar();
  ImPlot::PopStyleColor();
}

void
plotScatterStyled(const char* label,
                  const double* x,
                  const double* y,
                  int count,
                  ImVec4 color,
                  ImPlotMarker marker = ImPlotMarker_Circle) noexcept
{
  ImPlot::SetNextMarkerStyle(marker, 5.5F, color, IMPLOT_AUTO, ImVec4(0.08F, 0.10F, 0.14F, 1.0F));
  ImPlot::PlotScatter(label, x, y, count);
}

ImVec2
planWorldToPixel(double x, double y, ImVec2 imgMin, ImVec2 imgMax, const BladePlanViewport& vp) noexcept
{
  const double rangeX = std::max(vp.maxX - vp.minX, 1e-9);
  const double rangeY = std::max(vp.maxY - vp.minY, 1e-9);
  const float imgW = imgMax.x - imgMin.x;
  const float imgH = imgMax.y - imgMin.y;
  const float px = imgMin.x + static_cast<float>((x - vp.minX) / rangeX) * imgW;
  const float py = imgMax.y - static_cast<float>((y - vp.minY) / rangeY) * imgH;
  return {px, py};
}

core::BladePlanPoint
planPixelToWorld(ImVec2 pixel, ImVec2 imgMin, ImVec2 imgMax, const BladePlanViewport& vp) noexcept
{
  const double rangeX = std::max(vp.maxX - vp.minX, 1e-9);
  const double rangeY = std::max(vp.maxY - vp.minY, 1e-9);
  const float imgW = imgMax.x - imgMin.x;
  const float imgH = imgMax.y - imgMin.y;
  return {
    .xMm = vp.minX + ((pixel.x - imgMin.x) / imgW) * rangeX,
    .yMm = vp.minY + ((imgMax.y - pixel.y) / imgH) * rangeY,
  };
}

double
nicePlanScaleLength(double pxPerMm, float widthPx) noexcept
{
  const double targetMm = (widthPx * 0.15F) / std::max(pxPerMm, 1e-9);
  const double magnitude = std::pow(10.0, std::floor(std::log10(std::max(targetMm, 1e-9))));
  const double normalized = targetMm / magnitude;
  double step = 1.0;
  if (normalized >= 5.0) {
    step = 5.0;
  } else if (normalized >= 2.0) {
    step = 2.0;
  }
  return step * magnitude;
}

void
drawPlanScaleBar(ImDrawList* dl, ImVec2 imgMin, ImVec2 imgMax, const BladePlanViewport& vp)
{
  if (vp.maxX <= vp.minX) {
    return;
  }
  const float imgW = imgMax.x - imgMin.x;
  const double pxPerMm = imgW / (vp.maxX - vp.minX);
  const double mm = nicePlanScaleLength(pxPerMm, imgW);
  const float lenPx = static_cast<float>(mm * pxPerMm);
  const float y = imgMax.y - 24.0F;
  const float xL = imgMin.x + 16.0F;
  const float xR = xL + lenPx;
  constexpr ImU32 LINE = IM_COL32(34, 39, 52, 235);
  constexpr ImU32 TEXT = IM_COL32(34, 39, 52, 255);

  dl->AddLine({xL, y}, {xR, y}, LINE, 2.0F);
  dl->AddLine({xL, y - 5.0F}, {xL, y + 5.0F}, LINE, 2.0F);
  dl->AddLine({xR, y - 5.0F}, {xR, y + 5.0F}, LINE, 2.0F);
  char label[32];
  std::snprintf(label, sizeof(label), "%g мм", mm);
  dl->AddText({xL, y - 21.0F}, TEXT, label);
}

void
drawPlanLegend(ImDrawList* dl, ImVec2 topLeft, const core::BladeDesignResults& res)
{
  constexpr ImU32 BG = IM_COL32(255, 255, 255, 220);
  constexpr ImU32 BORDER = IM_COL32(174, 184, 202, 255);
  constexpr ImU32 TEXT = IM_COL32(34, 39, 52, 255);

  struct Entry
  {
    ImU32 color;
    const char* label;
  };
  const std::array<Entry, 4> entries{{
    {IM_COL32(73, 120, 199, 255), "Контур лопатки"},
    {IM_COL32(217, 73, 41, 255), "Средняя линия"},
    {IM_COL32(56, 133, 209, 255), "D1"},
    {IM_COL32(38, 51, 77, 255), "D2"},
  }};

  const float rowH = ImGui::GetTextLineHeightWithSpacing();
  const float width = 218.0F;
  const float height = rowH * static_cast<float>(entries.size() + 2U) + 12.0F;
  const ImVec2 bottomRight{topLeft.x + width, topLeft.y + height};
  dl->AddRectFilled(topLeft, bottomRight, BG, 5.0F);
  dl->AddRect(topLeft, bottomRight, BORDER, 5.0F);

  char title[96];
  std::snprintf(title, sizeof(title), "План РК: z = %d", static_cast<int>(res.allBlades.size()));
  dl->AddText({topLeft.x + 10.0F, topLeft.y + 7.0F}, TEXT, title);
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const float y = topLeft.y + 8.0F + static_cast<float>(i + 1U) * rowH;
    dl->AddRectFilled({topLeft.x + 10.0F, y + rowH * 0.5F - 2.0F},
                      {topLeft.x + 36.0F, y + rowH * 0.5F + 2.0F},
                      entries[i].color);
    dl->AddText({topLeft.x + 44.0F, y + 2.0F}, TEXT, entries[i].label);
  }
  char radii[96];
  std::snprintf(radii, sizeof(radii), "r1 %.1f мм   r2 %.1f мм", res.inletRadiusMm, res.outletRadiusMm);
  dl->AddText({topLeft.x + 10.0F, bottomRight.y - rowH - 2.0F}, TEXT, radii);
}

void
drawRotationArrow(ImDrawList* dl, ImVec2 imgMin, ImVec2 imgMax, const BladePlanViewport& vp)
{
  const double radius = std::min(vp.maxX - vp.minX, vp.maxY - vp.minY) * 0.33;
  const double a0 = std::numbers::pi * 0.12;
  const double a1 = std::numbers::pi * 0.58;
  std::array<ImVec2, 32> pts{};
  for (std::size_t i = 0; i < pts.size(); ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(pts.size() - 1U);
    const double a = a0 + (a1 - a0) * t;
    pts[i] = planWorldToPixel(radius * std::cos(a), radius * std::sin(a), imgMin, imgMax, vp);
  }
  constexpr ImU32 COLOR = IM_COL32(38, 51, 77, 210);
  dl->AddPolyline(pts.data(), static_cast<int>(pts.size()), COLOR, ImDrawFlags_None, 2.0F);
  const ImVec2 tip = pts.back();
  const ImVec2 prev = pts[pts.size() - 3U];
  const float dx = tip.x - prev.x;
  const float dy = tip.y - prev.y;
  const float len = std::sqrt((dx * dx) + (dy * dy));
  if (len > 1e-3F) {
    const float ux = dx / len;
    const float uy = dy / len;
    const ImVec2 left{tip.x - ux * 12.0F - uy * 5.5F, tip.y - uy * 12.0F + ux * 5.5F};
    const ImVec2 right{tip.x - ux * 12.0F + uy * 5.5F, tip.y - uy * 12.0F - ux * 5.5F};
    dl->AddTriangleFilled(tip, left, right, COLOR);
  }
}

void
drawPlanHoverInspector(ImVec2 imgMin, ImVec2 imgMax, const BladePlanViewport& vp)
{
  if (!ImGui::IsItemHovered()) {
    return;
  }
  const ImVec2 mousePx = ImGui::GetIO().MousePos;
  if (mousePx.x < imgMin.x || mousePx.x > imgMax.x || mousePx.y < imgMin.y || mousePx.y > imgMax.y) {
    return;
  }
  const auto world = planPixelToWorld(mousePx, imgMin, imgMax, vp);
  const double r = std::sqrt((world.xMm * world.xMm) + (world.yMm * world.yMm));
  const double phi = std::atan2(world.yMm, world.xMm);
  ImGui::BeginTooltip();
  ImGui::Text("x = %.2f мм   y = %.2f мм", world.xMm, world.yMm);
  ImGui::Text("r = %.2f мм   phi = %.3f рад", r, phi);
  ImGui::EndTooltip();
}

void
drawVelocityTrianglePlot(const char* id, const core::BladeVelocityTriangle& triangle) noexcept
{
  const double u = triangle.peripheralSpeedMs;
  const double cu = triangle.circumferentialVelocityMs;
  const double cm = triangle.meridionalVelocityMs;
  const std::vector<double> uX{0.0, u};
  const std::vector<double> uY{0.0, 0.0};
  const std::vector<double> cX{0.0, cu};
  const std::vector<double> cY{0.0, cm};
  const std::vector<double> wX{u, cu};
  const std::vector<double> wY{0.0, cm};
  const std::vector<double> cmX{cu, cu};
  const std::vector<double> cmY{0.0, cm};

  if (ImPlot::BeginPlot(id, ImVec2(-1, 170.0F), ImPlotFlags_Crosshairs)) {
    setupReadablePlot(ImPlotLocation_NorthEast);
    ImPlot::SetupAxes("c_u / u, м/с", "c_m, м/с", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
    plotLineStyled("u", uX, uY, kPlotGray, 2.4F);
    plotLineStyled("c", cX, cY, kPlotBlue, 2.6F);
    plotLineStyled("w", wX, wY, kPlotOrange, 2.6F);
    plotLineStyled("c_m", cmX, cmY, kPlotGreen, 1.8F);
    const double tipX[1]{cu};
    const double tipY[1]{cm};
    plotScatterStyled("tip", tipX, tipY, 1, kPlotBlue, ImPlotMarker_Circle);
    ImPlot::EndPlot();
  }
}

void
drawParamsWindow(BladeDesignPanelState& state,
                 BladeDesignPanelResult& result,
                 const core::PumpParams& pumpParams,
                 const core::MeridionalGeometry& geometry,
                 const core::FlowResults* flow,
                 bool geometryValid,
                 unsigned int dockspaceId) noexcept
{
  prepareDockedWindow(kParamsTitle, dockspaceId);
  style::pushPanelStyle();
  ImGui::Begin(kParamsTitle);

  auto& params = result.params;
  static constexpr double MIN_POSITIVE = 0.001;
  static constexpr double MIN_ZERO = 0.0;
  static constexpr double MIN_BETA = 5.0;
  static constexpr double MAX_BETA = 85.0;
  static constexpr double MIN_SIGMA = 0.1;
  static constexpr double MAX_SIGMA = 1.0;

  ImGui::TextDisabled("Источник: текущее меридианное сечение");
  ImGui::Text("D2 %.2f мм   b2 %.2f мм", pumpParams.d2, pumpParams.b2);
  ImGui::Text("din %.2f мм   dvt %.2f мм", pumpParams.din, pumpParams.dvt);
  ImGui::Separator();

  if (comboLatticeType(params.latticeType)) {
    result.paramsChanged = true;
    state.resultsStale = true;
  }
  if (params.latticeType == core::BladeLatticeType::Spatial) {
    ImGui::TextColored(style::PANEL_COLOR_WARN, "Spatial-решётка подготовлена как структура, расчёт пока отключён.");
  }

  result.paramsChanged |= fixedDragInt("z", "z — число лопастей", &params.bladeCount, 2, 32);
  result.paramsChanged |= fixedDrag("q",
                                    "Q — расход, м3/с",
                                    &params.flowRateM3s,
                                    &MIN_POSITIVE,
                                    nullptr,
                                    "%.4f",
                                    0.001F,
                                    "Расход задаётся в лопаточной системе и синхронизируется с расчётом скоростей.");
  result.paramsChanged |=
    fixedDrag("rpm", "n — частота, об/мин", &params.rpm, &MIN_POSITIVE, nullptr, "%.1f", 10.0F);
  result.paramsChanged |=
    fixedDrag("head", "H — расчётный напор, м", &params.designHeadM, &MIN_ZERO, nullptr, "%.2f", 0.5F);

  result.paramsChanged |= ImGui::Checkbox("auto beta1", &params.autoInletAngle);
  ImGui::SameLine();
  result.paramsChanged |= ImGui::Checkbox("auto beta2", &params.autoOutletAngle);
  ImGui::BeginDisabled(params.autoInletAngle);
  result.paramsChanged |=
    fixedDrag("beta1", "beta1 — вход, град", &params.beta1Deg, &MIN_BETA, &MAX_BETA, "%.2f", 0.2F);
  ImGui::EndDisabled();
  ImGui::BeginDisabled(params.autoOutletAngle);
  result.paramsChanged |=
    fixedDrag("beta2", "beta2 — выход, град", &params.beta2Deg, &MIN_BETA, &MAX_BETA, "%.2f", 0.2F);
  ImGui::EndDisabled();
  result.paramsChanged |= comboAngleLaw(params.angleLaw);

  ImGui::Separator();
  result.paramsChanged |= fixedDrag("s1", "s1 — вход, мм", &params.s1Mm, &MIN_POSITIVE, nullptr);
  result.paramsChanged |= fixedDrag("s2", "s2 — выход, мм", &params.s2Mm, &MIN_POSITIVE, nullptr);
  result.paramsChanged |= fixedDrag("smax", "smax — максимум, мм", &params.sMaxMm, &MIN_POSITIVE, nullptr);
  result.paramsChanged |= comboThicknessLaw(params.thicknessLaw);
  result.paramsChanged |= fixedDrag(
    "le_bulge", "Входная кромка, мм", &params.leadingEdgeBulgeMm, &MIN_ZERO, nullptr);
  result.paramsChanged |= fixedDrag(
    "te_bulge", "Выходная кромка, мм", &params.trailingEdgeBulgeMm, &MIN_ZERO, nullptr);

  ImGui::Separator();
  result.paramsChanged |= ImGui::Checkbox("auto sigma", &params.autoSlipFactor);
  ImGui::BeginDisabled(params.autoSlipFactor);
  result.paramsChanged |=
    fixedDrag("sigma", "sigma", &params.slipFactor, &MIN_SIGMA, &MAX_SIGMA, "%.3f", 0.01F);
  ImGui::EndDisabled();
  result.paramsChanged |=
    fixedDrag("loss", "k_h потерь", &params.hydraulicLossK, &MIN_ZERO, nullptr, "%.2f", 1.0F);

  if (result.paramsChanged) {
    state.resultsStale = true;
  }

  ImGui::Separator();
  ImGui::BeginDisabled(!geometryValid);
  if (style::accentButton("Пересчитать", false)) {
    core::BladeInputFromMeridional input{
      .pumpParams = pumpParams,
      .geometry = geometry,
      .inletAreaMm2 = fallbackInletAreaMm2(pumpParams),
      .outletAreaMm2 = fallbackOutletAreaMm2(pumpParams),
    };
    if (flow != nullptr) {
      input.flowResults = *flow;
    }
    core::BladeSolver solver;
    auto solved = solver.solve(params, input);
    if (solved) {
      state.results = std::move(*solved);
      state.resultsStale = false;
      state.statusMessage = "Расчёт лопаточной системы выполнен.";
    } else {
      state.results.reset();
      state.resultsStale = true;
      state.statusMessage = std::string("Ошибка расчёта: ") + core::toString(solved.error());
    }
  }
  ImGui::EndDisabled();

  if (state.resultsStale) {
    ImGui::TextColored(style::PANEL_COLOR_WARN, "Результаты требуют пересчёта.");
  }
  if (!state.statusMessage.empty()) {
    ImGui::TextWrapped("%s", state.statusMessage.c_str());
  }

  ImGui::End();
  style::popPanelStyle();
}

void
drawPlanWindow(const BladeDesignPanelState& state,
               Fbo& fbo,
               BladePlanRenderer& renderer,
               unsigned int dockspaceId) noexcept
{
  prepareDockedWindow(kPlanTitle, dockspaceId);
  ImGui::Begin(kPlanTitle);
  if (!state.results) {
    ImGui::TextDisabled("Нет расчёта.");
    ImGui::End();
    return;
  }

  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const int panelWidth = static_cast<int>(avail.x);
  const int panelHeight = static_cast<int>(avail.y);
  if (panelWidth <= 0 || panelHeight <= 0) {
    ImGui::End();
    return;
  }

  auto resizeResult = fbo.resize(panelWidth, panelHeight);
  if (!resizeResult) {
    ImGui::TextDisabled("Ошибка OpenGL FBO.");
    ImGui::End();
    return;
  }

  BladePlanViewport vp{};
  fbo.bind();
  vp = renderer.render(*state.results, panelWidth, panelHeight);
  fbo.unbind();

  const ImVec2 imgMin = ImGui::GetCursorScreenPos();
  const auto texId = static_cast<std::uintptr_t>(fbo.textureId());
  ImGui::Image(texId, avail, ImVec2(0, 1), ImVec2(1, 0));
  const ImVec2 imgMax{imgMin.x + avail.x, imgMin.y + avail.y};

  auto* dl = ImGui::GetWindowDrawList();
  drawPlanLegend(dl, {imgMin.x + 12.0F, imgMin.y + 12.0F}, *state.results);
  drawPlanScaleBar(dl, imgMin, imgMax, vp);
  drawRotationArrow(dl, imgMin, imgMax, vp);

  const ImVec2 d1 = planWorldToPixel(state.results->inletRadiusMm, 0.0, imgMin, imgMax, vp);
  const ImVec2 d2 = planWorldToPixel(state.results->outletRadiusMm, 0.0, imgMin, imgMax, vp);
  dl->AddText({d1.x + 6.0F, d1.y + 4.0F}, IM_COL32(32, 94, 150, 255), "D1");
  dl->AddText({d2.x + 6.0F, d2.y + 4.0F}, IM_COL32(28, 36, 56, 255), "D2");
  drawPlanHoverInspector(imgMin, imgMax, vp);

  ImGui::End();
}

void
drawConformalWindow(const BladeDesignPanelState& state, unsigned int dockspaceId) noexcept
{
  prepareDockedWindow(kConformalTitle, dockspaceId);
  ImGui::Begin(kConformalTitle);
  if (!state.results) {
    ImGui::TextDisabled("Нет расчёта.");
    ImGui::End();
    return;
  }
  const auto& sections = state.results->sections;
  if (ImPlot::BeginPlot("##conformal", ImVec2(-1, -1), ImPlotFlags_Crosshairs)) {
    setupReadablePlot(ImPlotLocation_NorthWest);
    ImPlot::SetupAxes("u = r phi, мм", "r, мм", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
    std::vector<double> u;
    std::vector<double> r;
    u.reserve(sections.size());
    r.reserve(sections.size());
    for (const auto& section : sections) {
      u.push_back(section.uMm);
      r.push_back(section.rMm);
    }
    const int bladeCount = static_cast<int>(state.results->allBlades.size());
    for (int i = 0; i < bladeCount; ++i) {
      std::vector<double> shifted = u;
      for (std::size_t j = 0; j < shifted.size(); ++j) {
        shifted[j] += 2.0 * std::numbers::pi * r[j] * static_cast<double>(i) /
                      static_cast<double>(std::max(bladeCount, 1));
      }
      char label[32];
      std::snprintf(label, sizeof(label), i == 0 ? "blade" : "##blade_%d", i);
      ImPlot::PushStyleColor(ImPlotCol_Line, i == 0 ? kPlotBlue : ImVec4(0.18F, 0.39F, 0.78F, 0.28F));
      ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, i == 0 ? 2.6F : 1.3F);
      ImPlot::PlotLine(label, shifted.data(), r.data(), static_cast<int>(r.size()));
      ImPlot::PopStyleVar();
      ImPlot::PopStyleColor();
    }
    if (!u.empty() && !r.empty()) {
      const double xEnds[2]{u.front(), u.back()};
      const double yEnds[2]{r.front(), r.back()};
      plotScatterStyled("in/out", xEnds, yEnds, 2, kPlotOrange, ImPlotMarker_Diamond);
    }
    const double r1 = state.results->inletRadiusMm;
    const double r2 = state.results->outletRadiusMm;
    ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.42F, 0.46F, 0.54F, 0.65F));
    ImPlot::PlotInfLines("r1", &r1, 1, ImPlotInfLinesFlags_Horizontal);
    ImPlot::PlotInfLines("r2", &r2, 1, ImPlotInfLinesFlags_Horizontal);
    ImPlot::PopStyleColor();
    ImPlot::EndPlot();
  }
  ImGui::End();
}

void
drawCharacteristicsWindow(const BladeDesignPanelState& state, unsigned int dockspaceId) noexcept
{
  prepareDockedWindow(kCharacteristicsTitle, dockspaceId);
  ImGui::Begin(kCharacteristicsTitle);
  if (!state.results) {
    ImGui::TextDisabled("Нет расчёта.");
    ImGui::End();
    return;
  }

  const auto& sections = state.results->sections;
  std::vector<double> r;
  std::vector<double> beta;
  std::vector<double> thick;
  std::vector<double> phi;
  std::vector<double> pitch;
  std::vector<double> blockage;
  r.reserve(sections.size());
  beta.reserve(sections.size());
  thick.reserve(sections.size());
  phi.reserve(sections.size());
  pitch.reserve(sections.size());
  blockage.reserve(sections.size());
  for (const auto& section : sections) {
    r.push_back(section.rMm);
    beta.push_back(section.betaDeg);
    thick.push_back(section.thicknessMm);
    phi.push_back(section.phiRad);
    pitch.push_back(section.pitchMm);
    blockage.push_back(section.blockage);
  }

  if (ImPlot::BeginPlot("##blade_laws", ImVec2(-1, ImGui::GetContentRegionAvail().y * 0.45F))) {
    setupReadablePlot(ImPlotLocation_NorthEast);
    ImPlot::SetupAxes("r, мм", "beta/s/phi", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
    plotLineStyled("beta, deg", r, beta, kPlotBlue, 2.5F);
    plotLineStyled("s, mm", r, thick, kPlotOrange, 2.2F);
    plotLineStyled("phi, rad", r, phi, kPlotViolet, 2.0F);
    ImPlot::EndPlot();
  }
  if (ImPlot::BeginPlot("##pitch_blockage", ImVec2(-1, -1))) {
    setupReadablePlot(ImPlotLocation_NorthEast);
    ImPlot::SetupAxes("r, мм", "pitch / blockage", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
    plotLineStyled("t(r), mm", r, pitch, kPlotGreen, 2.4F);
    plotLineStyled("s/t", r, blockage, kPlotGray, 2.2F);
    ImPlot::EndPlot();
  }
  ImGui::End();
}

void
drawTablesWindow(const BladeDesignPanelState& state, unsigned int dockspaceId) noexcept
{
  prepareDockedWindow(kTablesTitle, dockspaceId);
  ImGui::Begin(kTablesTitle);
  if (!state.results) {
    ImGui::TextDisabled("Нет расчёта.");
    ImGui::End();
    return;
  }

  const auto& res = *state.results;
  if (ImGui::BeginTable("##blade_summary", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
    const auto row = [](const char* name, double value, const char* unit) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(name);
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%.4g %s", value, unit);
    };
    row("r1", res.inletRadiusMm, "мм");
    row("r2", res.outletRadiusMm, "мм");
    row("F_in", res.inletAreaMm2, "мм2");
    row("F_out", res.outletAreaMm2, "мм2");
    row("sigma", res.slipFactor, "");
    row("beta1", res.inletTriangle.betaDeg, "град");
    row("beta2", res.outletTriangle.betaDeg, "град");
    row("c_m1", res.inletTriangle.meridionalVelocityMs, "м/с");
    row("c_m2", res.outletTriangle.meridionalVelocityMs, "м/с");
    row("u1", res.inletTriangle.peripheralSpeedMs, "м/с");
    row("u2", res.outletTriangle.peripheralSpeedMs, "м/с");
    ImGui::EndTable();
  }

  ImGui::Separator();
  if (ImGui::BeginTable("##velocity_triangles", 2, ImGuiTableFlags_SizingStretchSame)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("Входной треугольник");
    drawVelocityTrianglePlot("##triangle_in", res.inletTriangle);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted("Выходной треугольник");
    drawVelocityTrianglePlot("##triangle_out", res.outletTriangle);
    ImGui::EndTable();
  }

  for (const auto& msg : res.diagnostics) {
    ImGui::BulletText("%s", msg.c_str());
  }
  ImGui::End();
}

void
drawHeadCurveWindow(const BladeDesignPanelState& state, unsigned int dockspaceId) noexcept
{
  prepareDockedWindow(kHeadCurveTitle, dockspaceId);
  ImGui::Begin(kHeadCurveTitle);
  if (!state.results) {
    ImGui::TextDisabled("Нет расчёта.");
    ImGui::End();
    return;
  }

  const auto& res = *state.results;
  std::vector<double> q;
  std::vector<double> hEuler;
  std::vector<double> hSlip;
  std::vector<double> hReal;
  q.reserve(res.performanceCurve.size());
  hEuler.reserve(res.performanceCurve.size());
  hSlip.reserve(res.performanceCurve.size());
  hReal.reserve(res.performanceCurve.size());
  for (const auto& p : res.performanceCurve) {
    q.push_back(p.qM3h);
    hEuler.push_back(p.headEulerM);
    hSlip.push_back(p.headSlipM);
    hReal.push_back(p.headRealM);
  }

  if (ImPlot::BeginPlot("##head_curve", ImVec2(-1, -1), ImPlotFlags_Crosshairs)) {
    setupReadablePlot(ImPlotLocation_NorthEast);
    ImPlot::SetupAxes("Q, м3/ч", "H, м", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
    ImPlot::PushStyleColor(ImPlotCol_Fill, ImVec4(0.18F, 0.58F, 0.34F, 0.12F));
    ImPlot::PlotShaded("##real_head_fill", q.data(), hReal.data(), static_cast<int>(q.size()), 0.0);
    ImPlot::PopStyleColor();
    plotLineStyled("Euler", q, hEuler, kPlotGray, 2.0F);
    plotLineStyled("slip", q, hSlip, kPlotBlue, 2.3F);
    plotLineStyled("H real", q, hReal, kPlotGreen, 2.7F);
    ImPlot::EndPlot();
  }

  ImGui::End();
}

}

BladeDesignPanelResult
drawBladeDesignPanel(BladeDesignPanelState& state,
                     Fbo& bladePlanFbo,
                     BladePlanRenderer& bladePlanRenderer,
                     const core::BladeDesignParams& params,
                     const core::PumpParams& pumpParams,
                     const core::MeridionalGeometry& geometry,
                     const core::FlowResults* flow,
                     bool geometryValid,
                     unsigned int dockspaceId) noexcept
{
  BladeDesignPanelResult result;
  result.params = params;

  if (!state.lastPumpParams || *state.lastPumpParams != pumpParams) {
    state.lastPumpParams = pumpParams;
    state.resultsStale = true;
  }

  drawParamsWindow(state, result, pumpParams, geometry, flow, geometryValid, dockspaceId);
  drawPlanWindow(state, bladePlanFbo, bladePlanRenderer, dockspaceId);
  drawConformalWindow(state, dockspaceId);
  drawCharacteristicsWindow(state, dockspaceId);
  drawTablesWindow(state, dockspaceId);
  drawHeadCurveWindow(state, dockspaceId);

  return result;
}

}
