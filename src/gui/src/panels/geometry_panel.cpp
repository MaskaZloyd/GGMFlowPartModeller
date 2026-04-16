#include "gui/panels/geometry_panel.hpp"

#include "core/geometry.hpp"
#include "renderer/gl_headers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include <imgui.h>

namespace ggm::gui {

namespace {

// Pick a round scale-bar length that occupies ~15% of the viewport width.
double
niceScaleLength(double pxPerMm, float widthPx) noexcept
{
  double targetMm = (widthPx * 0.15F) / pxPerMm;
  if (targetMm <= 0.0) {
    return 1.0;
  }
  double mag = std::pow(10.0, std::floor(std::log10(targetMm)));
  double fr = targetMm / mag;
  double step = 1.0;
  if (fr >= 5.0) {
    step = 5.0;
  } else if (fr >= 2.0) {
    step = 2.0;
  }
  return step * mag;
}

void
drawLegend(ImDrawList* dl, ImVec2 topLeft)
{
  constexpr ImU32 BG = IM_COL32(255, 255, 255, 215);
  constexpr ImU32 BORDER = IM_COL32(180, 180, 190, 255);
  constexpr ImU32 TEXT = IM_COL32(40, 40, 60, 255);

  struct Entry
  {
    ImU32 color;
    const char* label;
  };
  const Entry entries[] = {
    {IM_COL32(199, 51, 46, 255), "Втулка (hub)"},
    {IM_COL32(46, 92, 199, 255), "Покрывной диск (shroud)"},
    {IM_COL32(77, 158, 77, 255), "Средняя линия"},
    {IM_COL32(68, 1, 84, 255), "Линия тока ψ = 0"},
    {IM_COL32(253, 231, 37, 255), "Линия тока ψ = 1"},
  };

  float lineH = ImGui::GetTextLineHeightWithSpacing();
  float rowH = lineH;
  float width = 240.0F;
  float height = rowH * static_cast<float>(IM_ARRAYSIZE(entries)) + 10.0F;

  ImVec2 maxP(topLeft.x + width, topLeft.y + height);
  dl->AddRectFilled(topLeft, maxP, BG, 5.0F);
  dl->AddRect(topLeft, maxP, BORDER, 5.0F);

  for (int i = 0; i < IM_ARRAYSIZE(entries); ++i) {
    float y = topLeft.y + 5.0F + static_cast<float>(i) * rowH;
    ImVec2 a(topLeft.x + 10.0F, y + rowH * 0.5F - 2.0F);
    ImVec2 b(topLeft.x + 36.0F, y + rowH * 0.5F + 2.0F);
    dl->AddRectFilled(a, b, entries[i].color);
    dl->AddText(ImVec2(topLeft.x + 44.0F, y + 2.0F), TEXT, entries[i].label);
  }
}

void
drawScaleBar(ImDrawList* dl, ImVec2 imgMin, ImVec2 imgMax, const ViewportMap& vp)
{
  if (vp.widthPx <= 0 || vp.maxZ <= vp.minZ) {
    return;
  }
  float imgW = imgMax.x - imgMin.x;
  float imgH = imgMax.y - imgMin.y;
  double pxPerMm = imgW / (vp.maxZ - vp.minZ);
  double mm = niceScaleLength(pxPerMm, imgW);
  float lenPx = static_cast<float>(mm * pxPerMm);
  float y = imgMax.y - 22.0F;
  float xL = imgMin.x + 14.0F;
  float xR = xL + lenPx;

  constexpr ImU32 LINE = IM_COL32(40, 40, 60, 230);
  constexpr ImU32 TEXT = IM_COL32(40, 40, 60, 255);
  dl->AddLine(ImVec2(xL, y), ImVec2(xR, y), LINE, 2.0F);
  dl->AddLine(ImVec2(xL, y - 5.0F), ImVec2(xL, y + 5.0F), LINE, 2.0F);
  dl->AddLine(ImVec2(xR, y - 5.0F), ImVec2(xR, y + 5.0F), LINE, 2.0F);

  char label[32];
  std::snprintf(label, sizeof(label), "%g мм", mm);
  dl->AddText(ImVec2(xL, y - 20.0F), TEXT, label);
  (void)imgH;
}

} // namespace

void
drawGeometryPanel(Fbo& fbo,
                  GeometryRenderer& renderer,
                  const core::MeridionalGeometry& geom,
                  const core::FlowResults* flow,
                  const RenderSettings& renderSettings,
                  bool geometryValid) noexcept
{
  ImGui::Begin("Геометрия");

  ImVec2 avail = ImGui::GetContentRegionAvail();
  int panelWidth = static_cast<int>(avail.x);
  int panelHeight = static_cast<int>(avail.y);

  if (panelWidth <= 0 || panelHeight <= 0) {
    ImGui::End();
    return;
  }

  auto resizeResult = fbo.resize(panelWidth, panelHeight);
  if (!resizeResult) {
    ImGui::End();
    return;
  }

  ViewportMap vp{};
  fbo.bind();
  if (geometryValid) {
    vp = renderer.render(geom, flow, renderSettings, panelWidth, panelHeight);
  } else {
    glClearColor(0.988F, 0.990F, 0.994F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  fbo.unbind();

  ImVec2 imgMin = ImGui::GetCursorScreenPos();
  auto texId = static_cast<std::uintptr_t>(fbo.textureId());
  ImGui::Image(texId, avail, ImVec2(0, 1), ImVec2(1, 0));
  ImVec2 imgMax(imgMin.x + avail.x, imgMin.y + avail.y);

  if (geometryValid) {
    auto* dl = ImGui::GetWindowDrawList();
    drawLegend(dl, ImVec2(imgMin.x + 12.0F, imgMin.y + 12.0F));
    drawScaleBar(dl, imgMin, imgMax, vp);
  }

  ImGui::End();
}

} // namespace ggm::gui
