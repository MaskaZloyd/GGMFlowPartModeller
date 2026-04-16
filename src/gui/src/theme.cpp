#include "gui/theme.hpp"

#include <imgui.h>
#include <implot.h>

namespace ggm::gui {

namespace {

constexpr ImVec4
rgba(float r, float g, float b, float a = 1.0F)
{
  return {r, g, b, a};
}

// Palette — light canvas with a soft indigo accent.
constexpr ImVec4 BG_BASE = rgba(0.975F, 0.977F, 0.980F);   // main background
constexpr ImVec4 BG_PANEL = rgba(0.945F, 0.950F, 0.958F);  // child/group bg
constexpr ImVec4 BG_HEADER = rgba(0.895F, 0.910F, 0.935F); // section headers
constexpr ImVec4 BG_HOVER = rgba(0.880F, 0.905F, 0.940F);
constexpr ImVec4 BG_ACTIVE = rgba(0.780F, 0.850F, 0.955F);
constexpr ImVec4 ACCENT = rgba(0.320F, 0.470F, 0.870F);
constexpr ImVec4 ACCENT_HOVER = rgba(0.430F, 0.570F, 0.930F);
constexpr ImVec4 BORDER = rgba(0.820F, 0.830F, 0.845F);
constexpr ImVec4 TEXT = rgba(0.145F, 0.150F, 0.180F);
constexpr ImVec4 TEXT_MUTED = rgba(0.480F, 0.490F, 0.520F);

} // namespace

void
applyLightTheme()
{
  ImGui::StyleColorsLight();
  auto& style = ImGui::GetStyle();

  // Spacing / shape: airy, rounded, predictable.
  style.WindowRounding = 6.0F;
  style.ChildRounding = 6.0F;
  style.FrameRounding = 4.0F;
  style.PopupRounding = 6.0F;
  style.ScrollbarRounding = 9.0F;
  style.GrabRounding = 4.0F;
  style.TabRounding = 4.0F;
  style.WindowPadding = ImVec2(12.0F, 10.0F);
  style.FramePadding = ImVec2(8.0F, 5.0F);
  style.ItemSpacing = ImVec2(8.0F, 7.0F);
  style.ItemInnerSpacing = ImVec2(6.0F, 5.0F);
  style.IndentSpacing = 18.0F;
  style.ScrollbarSize = 14.0F;
  style.GrabMinSize = 10.0F;
  style.WindowBorderSize = 1.0F;
  style.FrameBorderSize = 1.0F;
  style.PopupBorderSize = 1.0F;
  style.WindowTitleAlign = ImVec2(0.02F, 0.5F);

  auto& c = style.Colors;
  c[ImGuiCol_Text] = TEXT;
  c[ImGuiCol_TextDisabled] = TEXT_MUTED;
  c[ImGuiCol_WindowBg] = BG_BASE;
  c[ImGuiCol_ChildBg] = BG_PANEL;
  c[ImGuiCol_PopupBg] = rgba(0.990F, 0.992F, 0.996F);
  c[ImGuiCol_Border] = BORDER;
  c[ImGuiCol_FrameBg] = rgba(1.0F, 1.0F, 1.0F);
  c[ImGuiCol_FrameBgHovered] = BG_HOVER;
  c[ImGuiCol_FrameBgActive] = BG_ACTIVE;
  c[ImGuiCol_TitleBg] = BG_PANEL;
  c[ImGuiCol_TitleBgActive] = BG_HEADER;
  c[ImGuiCol_TitleBgCollapsed] = BG_PANEL;
  c[ImGuiCol_MenuBarBg] = BG_HEADER;
  c[ImGuiCol_ScrollbarBg] = rgba(0.960F, 0.962F, 0.968F);
  c[ImGuiCol_ScrollbarGrab] = rgba(0.800F, 0.820F, 0.840F);
  c[ImGuiCol_ScrollbarGrabHovered] = rgba(0.700F, 0.740F, 0.780F);
  c[ImGuiCol_ScrollbarGrabActive] = ACCENT;
  c[ImGuiCol_CheckMark] = ACCENT;
  c[ImGuiCol_SliderGrab] = ACCENT;
  c[ImGuiCol_SliderGrabActive] = ACCENT_HOVER;
  c[ImGuiCol_Button] = rgba(0.920F, 0.930F, 0.950F);
  c[ImGuiCol_ButtonHovered] = BG_HOVER;
  c[ImGuiCol_ButtonActive] = BG_ACTIVE;
  c[ImGuiCol_Header] = BG_HEADER;
  c[ImGuiCol_HeaderHovered] = BG_HOVER;
  c[ImGuiCol_HeaderActive] = BG_ACTIVE;
  c[ImGuiCol_Separator] = BORDER;
  c[ImGuiCol_SeparatorHovered] = ACCENT_HOVER;
  c[ImGuiCol_SeparatorActive] = ACCENT;
  c[ImGuiCol_ResizeGrip] = rgba(0.850F, 0.860F, 0.875F);
  c[ImGuiCol_ResizeGripHovered] = ACCENT_HOVER;
  c[ImGuiCol_ResizeGripActive] = ACCENT;
  c[ImGuiCol_Tab] = rgba(0.895F, 0.910F, 0.935F);
  c[ImGuiCol_TabHovered] = BG_HOVER;
  c[ImGuiCol_TabActive] = rgba(0.975F, 0.977F, 0.980F);
  c[ImGuiCol_TabUnfocused] = BG_PANEL;
  c[ImGuiCol_TabUnfocusedActive] = BG_HEADER;
  c[ImGuiCol_DockingPreview] = ACCENT;
  c[ImGuiCol_DockingEmptyBg] = BG_BASE;
  c[ImGuiCol_PlotLines] = ACCENT;
  c[ImGuiCol_PlotLinesHovered] = ACCENT_HOVER;
  c[ImGuiCol_PlotHistogram] = ACCENT;
  c[ImGuiCol_NavHighlight] = ACCENT;

  // ImPlot light variant.
  ImPlot::StyleColorsLight();
  auto& ps = ImPlot::GetStyle();
  ps.LineWeight = 1.5F;
  ps.MarkerSize = 4.0F;
  ps.PlotPadding = ImVec2(10.0F, 10.0F);
  ps.LabelPadding = ImVec2(6.0F, 4.0F);
  ps.LegendPadding = ImVec2(8.0F, 6.0F);
  ps.LegendInnerPadding = ImVec2(4.0F, 3.0F);
}

} // namespace ggm::gui
