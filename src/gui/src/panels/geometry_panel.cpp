#include "gui/panels/geometry_panel.hpp"

#include "core/flow_solver_types.hpp"
#include "core/geometry.hpp"
#include "layout/dock_utils.hpp"
#include "renderer/gl_headers.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

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

// Map a world-space (z, r) point to pixel coordinates inside the image.
ImVec2
worldToPixel(double z, double r, ImVec2 imgMin, ImVec2 imgMax, const ViewportMap& vp) noexcept
{
  const double rangeZ = std::max(vp.maxZ - vp.minZ, 1e-9);
  const double rangeR = std::max(vp.maxR - vp.minR, 1e-9);
  const float imgW = imgMax.x - imgMin.x;
  const float imgH = imgMax.y - imgMin.y;
  const float px = imgMin.x + static_cast<float>((z - vp.minZ) / rangeZ) * imgW;
  // World r is "up"; image y grows down — flip.
  const float py = imgMax.y - static_cast<float>((r - vp.minR) / rangeR) * imgH;
  return ImVec2(px, py);
}

math::Vec2
pixelToWorld(ImVec2 pixel, ImVec2 imgMin, ImVec2 imgMax, const ViewportMap& vp) noexcept
{
  const double rangeZ = std::max(vp.maxZ - vp.minZ, 1e-9);
  const double rangeR = std::max(vp.maxR - vp.minR, 1e-9);
  const float imgW = imgMax.x - imgMin.x;
  const float imgH = imgMax.y - imgMin.y;
  const double z = vp.minZ + ((pixel.x - imgMin.x) / imgW) * rangeZ;
  const double r = vp.minR + ((imgMax.y - pixel.y) / imgH) * rangeR;
  return { z, r };
}

// Stats overlay placed at a caller-supplied anchor (so it can sit under
// the legend on the left). Width is sized for the longest line.
void
drawStatsOverlay(ImDrawList* dl, ImVec2 topLeft, const core::FlowResults* flow)
{
  if (flow == nullptr) {
    return;
  }

  constexpr ImU32 BG = IM_COL32(255, 255, 255, 215);
  constexpr ImU32 BORDER = IM_COL32(180, 180, 190, 255);
  constexpr ImU32 TEXT = IM_COL32(40, 40, 60, 255);
  constexpr ImU32 DIM = IM_COL32(110, 110, 130, 255);

  const auto& ap = flow->areaProfile;
  double minFlow = std::numeric_limits<double>::max();
  for (double f : ap.flowAreas) {
    if (std::isfinite(f) && f > 0.0) {
      minFlow = std::min(minFlow, f);
    }
  }
  if (!std::isfinite(minFlow)) {
    minFlow = 0.0;
  }

  double vMin = std::numeric_limits<double>::max();
  double vMax = 0.0;
  for (const auto& vel : flow->velocities) {
    for (const auto& s : vel.samples) {
      if (std::isfinite(s.speed)) {
        vMin = std::min(vMin, s.speed);
        vMax = std::max(vMax, s.speed);
      }
    }
  }
  if (!std::isfinite(vMin)) {
    vMin = 0.0;
  }

  char lines[5][64];
  std::snprintf(lines[0], sizeof(lines[0]), "F_in  = %.1f мм²", ap.f1);
  std::snprintf(lines[1], sizeof(lines[1]), "F_out = %.1f мм²", ap.f2);
  std::snprintf(lines[2], sizeof(lines[2]), "F_min = %.1f мм²", minFlow);
  std::snprintf(lines[3], sizeof(lines[3]), "|V| ∈ [%.2f, %.2f]", vMin, vMax);
  std::snprintf(
    lines[4], sizeof(lines[4]), "F_out / F_in = %.3f", ap.f1 > 0.0 ? ap.f2 / ap.f1 : 0.0);

  // Size box to fit the widest rendered line, with a small side padding.
  float maxTextW = ImGui::CalcTextSize("Метрики потока").x;
  for (const auto& line : lines) {
    maxTextW = std::max(maxTextW, ImGui::CalcTextSize(line).x);
  }
  const float padX = 10.0F;
  const float padY = 6.0F;
  const float rowH = ImGui::GetTextLineHeightWithSpacing();
  const float width = maxTextW + (padX * 2.0F);
  const float height = (rowH * 6.0F) + padY; // title row + 5 data rows

  const ImVec2 bottomRight{ topLeft.x + width, topLeft.y + height };
  dl->AddRectFilled(topLeft, bottomRight, BG, 5.0F);
  dl->AddRect(topLeft, bottomRight, BORDER, 5.0F);

  dl->AddText(ImVec2(topLeft.x + padX, topLeft.y + (padY * 0.5F)), DIM, "Метрики потока");
  for (int i = 0; i < 5; ++i) {
    dl->AddText(ImVec2(topLeft.x + padX,
                       topLeft.y + (padY * 0.5F) + static_cast<float>(i + 1) * rowH),
                TEXT,
                lines[i]);
  }
}

// Draws a dashed line between two pixel points. ImDrawList has no native
// dash support, so we step along the segment and emit short strokes.
void
drawDashedLine(ImDrawList* dl,
               ImVec2 start,
               ImVec2 end,
               ImU32 color,
               float thickness = 1.5F,
               float dashLen = 8.0F,
               float gapLen = 5.0F)
{
  const float dx = end.x - start.x;
  const float dy = end.y - start.y;
  const float length = std::sqrt((dx * dx) + (dy * dy));
  if (length <= 0.0F) {
    return;
  }
  const float ux = dx / length;
  const float uy = dy / length;
  const float period = dashLen + gapLen;

  for (float t = 0.0F; t < length; t += period) {
    const float t1 = std::min(t + dashLen, length);
    const ImVec2 a(start.x + (ux * t), start.y + (uy * t));
    const ImVec2 b(start.x + (ux * t1), start.y + (uy * t1));
    dl->AddLine(a, b, color, thickness);
  }
}

// Dashed black lines connecting hub↔shroud at inlet and at outlet. Marks
// the entry and exit cross-sections on top of the flow visualization.
void
drawPortLines(ImDrawList* dl,
              ImVec2 imgMin,
              ImVec2 imgMax,
              const core::MeridionalGeometry& geom,
              const ViewportMap& vp)
{
  constexpr ImU32 DASH_COLOR = IM_COL32(30, 30, 45, 220);

  if (!geom.hubCurve.empty() && !geom.shroudCurve.empty()) {
    const ImVec2 hubIn = worldToPixel(
      geom.hubCurve.front().x(), geom.hubCurve.front().y(), imgMin, imgMax, vp);
    const ImVec2 shrIn = worldToPixel(
      geom.shroudCurve.front().x(), geom.shroudCurve.front().y(), imgMin, imgMax, vp);
    drawDashedLine(dl, hubIn, shrIn, DASH_COLOR);

    const ImVec2 hubOut = worldToPixel(
      geom.hubCurve.back().x(), geom.hubCurve.back().y(), imgMin, imgMax, vp);
    const ImVec2 shrOut = worldToPixel(
      geom.shroudCurve.back().x(), geom.shroudCurve.back().y(), imgMin, imgMax, vp);
    drawDashedLine(dl, hubOut, shrOut, DASH_COLOR);
  }
}

// Colored circles at inlet (hub[0], shroud[0]), outlet (hub.back, shroud.back)
// and throat (the midline station with minimum flow area).
void
drawCriticalMarkers(ImDrawList* dl,
                    ImVec2 imgMin,
                    ImVec2 imgMax,
                    const core::MeridionalGeometry& geom,
                    const core::FlowResults* flow,
                    const ViewportMap& vp)
{
  // Place label relative to the circle: "above" drops it above the point,
  // "below" puts it below, "side" places to the right.
  enum class LabelSide
  {
    Above,
    Below,
    Side,
  };

  const auto marker =
    [&](const math::Vec2& world, ImU32 fill, const char* label, LabelSide side) {
      const ImVec2 px = worldToPixel(world.x(), world.y(), imgMin, imgMax, vp);
      dl->AddCircleFilled(px, 5.0F, fill);
      dl->AddCircle(px, 5.0F, IM_COL32(20, 20, 20, 255), 0, 1.2F);

      const ImVec2 textSize = ImGui::CalcTextSize(label);
      constexpr float GAP = 8.0F;
      ImVec2 textPos;
      switch (side) {
        case LabelSide::Above:
          textPos = ImVec2(px.x - (textSize.x * 0.5F), px.y - GAP - textSize.y);
          break;
        case LabelSide::Below:
          textPos = ImVec2(px.x - (textSize.x * 0.5F), px.y + GAP);
          break;
        case LabelSide::Side:
        default:
          textPos = ImVec2(px.x + GAP, px.y - (textSize.y * 0.5F));
          break;
      }
      dl->AddText(textPos, IM_COL32(20, 20, 30, 255), label);
    };

  // Hub — inlet sits low-left (label below); outlet meets D₂ in the upper-
  // right corner where "below" would go off-screen, so label above there.
  if (!geom.hubCurve.empty()) {
    marker(geom.hubCurve.front(), IM_COL32(70, 200, 70, 230), "вход hub", LabelSide::Below);
    marker(geom.hubCurve.back(), IM_COL32(220, 60, 60, 230), "выход hub", LabelSide::Above);
  }
  // Shroud — labels above (shroud is the upper curve).
  if (!geom.shroudCurve.empty()) {
    marker(
      geom.shroudCurve.front(), IM_COL32(70, 200, 70, 230), "вход shroud", LabelSide::Above);
    marker(geom.shroudCurve.back(), IM_COL32(220, 60, 60, 230), "выход shroud", LabelSide::Above);
  }

  (void)flow;
}

// Shows a tooltip with local (z, r, nearest ψ, |V|) when the user hovers
// over the geometry image.
void
drawHoverInspector(ImVec2 imgMin,
                   ImVec2 imgMax,
                   const core::FlowResults* flow,
                   const ViewportMap& vp)
{
  if (!ImGui::IsItemHovered()) {
    return;
  }

  const ImVec2 mousePx = ImGui::GetIO().MousePos;
  if (mousePx.x < imgMin.x || mousePx.x > imgMax.x || mousePx.y < imgMin.y ||
      mousePx.y > imgMax.y) {
    return;
  }

  const math::Vec2 world = pixelToWorld(mousePx, imgMin, imgMax, vp);

  // Find nearest streamline sample (z, r space).
  const core::VelocitySample* best = nullptr;
  double bestPsi = 0.0;
  double bestSq = std::numeric_limits<double>::max();
  if (flow != nullptr) {
    for (const auto& vel : flow->velocities) {
      for (const auto& s : vel.samples) {
        const double dz = s.point.x() - world.x();
        const double dr = s.point.y() - world.y();
        const double d2 = (dz * dz) + (dr * dr);
        if (d2 < bestSq) {
          bestSq = d2;
          best = &s;
          bestPsi = vel.psiLevel;
        }
      }
    }
  }

  ImGui::BeginTooltip();
  ImGui::Text("z = %.2f мм   r = %.2f мм", world.x(), world.y());
  if (best != nullptr) {
    ImGui::Separator();
    ImGui::Text("ближ. линия тока: ψ = %.3f", bestPsi);
    ImGui::Text("|V| = %.3f   (v_z, v_r) = (%.3f, %.3f)",
                best->speed,
                best->velocity.x(),
                best->velocity.y());
    ImGui::Text("s (вдоль л.т.) = %.2f мм", best->arcLength);
  }
  ImGui::EndTooltip();
}

// Ghost overlay of a snapshot geometry via ImDrawList — same viewport as
// the main render so curves align. Drawn with muted violet, semi-transp.
void
drawSnapshotOverlay(ImDrawList* dl,
                    ImVec2 imgMin,
                    ImVec2 imgMax,
                    const core::MeridionalGeometry& snapshot,
                    const ViewportMap& vp)
{
  constexpr ImU32 HUB_COLOR = IM_COL32(120, 60, 170, 170);
  constexpr ImU32 SHROUD_COLOR = IM_COL32(60, 130, 170, 170);

  const auto drawCurve = [&](const std::vector<math::Vec2>& curve, ImU32 color) {
    if (curve.size() < 2U) {
      return;
    }
    std::vector<ImVec2> pts;
    pts.reserve(curve.size());
    for (const auto& p : curve) {
      pts.push_back(worldToPixel(p.x(), p.y(), imgMin, imgMax, vp));
    }
    dl->AddPolyline(pts.data(), static_cast<int>(pts.size()), color, ImDrawFlags_None, 2.0F);
  };

  drawCurve(snapshot.hubCurve, HUB_COLOR);
  drawCurve(snapshot.shroudCurve, SHROUD_COLOR);
}

// Exports the currently bound FBO contents to a .ppm file. PPM is a trivial
// header + raw RGB bytes — zero dependencies, quick to implement, and every
// modern viewer/converter reads it.
bool
exportFboToPpm(const Fbo& fbo, const std::string& path)
{
  const auto [width, height] = fbo.size();
  if (width <= 0 || height <= 0) {
    return false;
  }

  std::vector<unsigned char> pixels(static_cast<std::size_t>(width) *
                                    static_cast<std::size_t>(height) * 3U);

  // Read pixels back from the resolve color texture. After unbind() it
  // already contains the resolved frame; glGetTexImage avoids us needing
  // to know the internal FBO id.
  glBindTexture(GL_TEXTURE_2D, fbo.textureId());
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
  glBindTexture(GL_TEXTURE_2D, 0);

  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }
  out << "P6\n" << width << ' ' << height << "\n255\n";
  // FBO pixels come out bottom-up; flip rows for standard top-down layout.
  const std::size_t stride = static_cast<std::size_t>(width) * 3U;
  for (int y = height - 1; y >= 0; --y) {
    const auto* row = pixels.data() + (static_cast<std::size_t>(y) * stride);
    out.write(reinterpret_cast<const char*>(row), static_cast<std::streamsize>(stride));
  }
  return static_cast<bool>(out);
}

// Renders the legend and returns its on-screen height so callers can stack
// further overlays (e.g. stats) directly beneath it.
float
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
  return height;
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

namespace {

std::string
makeExportPath()
{
  using Clock = std::chrono::system_clock;
  const auto now = Clock::to_time_t(Clock::now());
  std::tm tm{};
  localtime_r(&now, &tm);
  char buf[64];
  std::strftime(buf, sizeof(buf), "ggm_geometry_%Y%m%d_%H%M%S.ppm", &tm);
  return (std::filesystem::current_path() / buf).string();
}

void
drawActionBar(GeometryPanelState& state,
              Fbo& fbo,
              const core::MeridionalGeometry& geom,
              bool geometryValid)
{
  ImGui::BeginDisabled(!geometryValid);
  if (ImGui::SmallButton("Сохранить снимок")) {
    state.snapshot = geom;
    state.exportMessage = "Снимок сохранён для сравнения";
    state.exportMessageExpiresAt = ImGui::GetTime() + 3.0;
  }
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::BeginDisabled(!state.snapshot.has_value());
  if (ImGui::SmallButton("Очистить снимок")) {
    state.snapshot.reset();
  }
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::BeginDisabled(!geometryValid);
  if (ImGui::SmallButton("Экспорт PPM")) {
    const std::string path = makeExportPath();
    const bool ok = exportFboToPpm(fbo, path);
    state.exportMessage = ok ? ("Экспорт: " + path) : ("Ошибка экспорта: " + path);
    state.exportMessageExpiresAt = ImGui::GetTime() + 5.0;
  }
  ImGui::EndDisabled();

  if (state.exportMessageExpiresAt > 0.0 && ImGui::GetTime() < state.exportMessageExpiresAt) {
    ImGui::SameLine();
    ImGui::TextDisabled("%s", state.exportMessage.c_str());
  }
}

} // namespace

void
drawGeometryPanelWithTitle(const char* windowTitle,
                           Fbo& fbo,
                           GeometryRenderer& renderer,
                           GeometryPanelState& state,
                           const core::MeridionalGeometry& geom,
                           const core::FlowResults* flow,
                           const RenderSettings& renderSettings,
                           bool geometryValid,
                           ImGuiID dockspaceId) noexcept
{
  prepareDockedWindow(windowTitle, dockspaceId);
  ImGui::Begin(windowTitle);

  drawActionBar(state, fbo, geom, geometryValid);

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
    const ImVec2 legendPos{ imgMin.x + 12.0F, imgMin.y + 12.0F };
    const float legendH = drawLegend(dl, legendPos);
    drawScaleBar(dl, imgMin, imgMax, vp);
    // Stats box stacked directly under the legend, 8 px gap.
    drawStatsOverlay(dl, ImVec2(legendPos.x, legendPos.y + legendH + 8.0F), flow);
    // Inlet/outlet cross-section lines (dashed) — under the circle markers
    // so the markers remain visually on top.
    drawPortLines(dl, imgMin, imgMax, geom, vp);
    if (renderSettings.showCriticalMarkers) {
      drawCriticalMarkers(dl, imgMin, imgMax, geom, flow, vp);
    }
    if (renderSettings.showSnapshotOverlay && state.snapshot.has_value()) {
      drawSnapshotOverlay(dl, imgMin, imgMax, *state.snapshot, vp);
    }
    if (renderSettings.showHoverInspector) {
      drawHoverInspector(imgMin, imgMax, flow, vp);
    }
  }

  ImGui::End();
}

void
drawGeometryPanel(Fbo& fbo,
                  GeometryRenderer& renderer,
                  GeometryPanelState& state,
                  const core::MeridionalGeometry& geom,
                  const core::FlowResults* flow,
                  const RenderSettings& renderSettings,
                  bool geometryValid,
                  ImGuiID dockspaceId) noexcept
{
  drawGeometryPanelWithTitle(
    "Геометрия", fbo, renderer, state, geom, flow, renderSettings, geometryValid, dockspaceId);
}

} // namespace ggm::gui
