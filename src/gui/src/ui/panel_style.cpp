#include "gui/ui/panel_style.hpp"

#include <algorithm>

namespace ggm::gui::style {

namespace {

// Shared body for double / float / int drag widgets. Returns DragResult.
// `drawWidget` is a callable that issues ImGui::DragScalar (or DragInt) and
// returns whether the value changed this frame.
template<typename DrawFn>
DragResult
fixedDragCommon(const char* id, const char* label, const char* tooltip, DrawFn&& drawWidget) noexcept
{
  ImGui::PushID(id);
  ImGui::SetNextItemWidth(PANEL_INPUT_WIDTH);

  const bool changed = drawWidget();
  const bool activated = ImGui::IsItemActivated();

  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);

  tooltipIfHovered(tooltip);

  ImGui::PopID();
  return { changed, activated };
}

} // namespace

DragResult
fixedDragDouble(const char* id,
                const char* label,
                double* value,
                const double* minVal,
                const double* maxVal,
                const char* format,
                float speed,
                const char* tooltip) noexcept
{
  return fixedDragCommon(id, label, tooltip, [&]() {
    const bool changed =
      ImGui::DragScalar("##v", ImGuiDataType_Double, value, speed, minVal, maxVal, format);
    if (minVal != nullptr && *value < *minVal) {
      *value = *minVal;
    }
    if (maxVal != nullptr && *value > *maxVal) {
      *value = *maxVal;
    }
    return changed;
  });
}

DragResult
fixedDragInt(const char* id,
             const char* label,
             int* value,
             int minVal,
             int maxVal,
             const char* tooltip) noexcept
{
  return fixedDragCommon(id, label, tooltip, [&]() {
    const bool changed =
      ImGui::DragInt("##v", value, 1.0F, minVal, maxVal, PANEL_DRAG_FORMAT_INT, ImGuiSliderFlags_AlwaysClamp);
    *value = std::clamp(*value, minVal, maxVal);
    return changed;
  });
}

DragResult
fixedDragFloat(const char* id,
               const char* label,
               float* value,
               float minVal,
               float maxVal,
               const char* format,
               float speed,
               const char* tooltip) noexcept
{
  return fixedDragCommon(id, label, tooltip, [&]() {
    const bool changed = ImGui::DragScalar("##v",
                                           ImGuiDataType_Float,
                                           value,
                                           speed,
                                           &minVal,
                                           &maxVal,
                                           format,
                                           ImGuiSliderFlags_AlwaysClamp);
    *value = std::clamp(*value, minVal, maxVal);
    return changed;
  });
}

bool
accentButton(const char* label, bool disabled) noexcept
{
  ImGui::BeginDisabled(disabled);
  ImGui::PushStyleColor(ImGuiCol_Button, PANEL_COLOR_ACCENT);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, PANEL_COLOR_ACCENT_HOV);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, PANEL_COLOR_ACCENT);
  const bool clicked = ImGui::Button(label, ImVec2(-1, 0.0F));
  ImGui::PopStyleColor(3);
  ImGui::EndDisabled();
  return clicked;
}

bool
dangerButton(const char* label, bool disabled) noexcept
{
  ImGui::BeginDisabled(disabled);
  ImGui::PushStyleColor(ImGuiCol_Button, PANEL_COLOR_DANGER);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, PANEL_COLOR_DANGER_HOV);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, PANEL_COLOR_DANGER);
  const bool clicked = ImGui::Button(label, ImVec2(-1, 0.0F));
  ImGui::PopStyleColor(3);
  ImGui::EndDisabled();
  return clicked;
}

void
pushPanelStyle() noexcept
{
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0F, 4.0F));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0F, 6.0F));
}

void
popPanelStyle() noexcept
{
  ImGui::PopStyleVar(2);
}

void
drawColoredStatusLine(const ImVec4& color, const char* text) noexcept
{
  ImGui::TextColored(color, "●");
  ImGui::SameLine();
  ImGui::PushTextWrapPos();
  ImGui::TextColored(color, "%s", text);
  ImGui::PopTextWrapPos();
}

void
tooltipIfHovered(const char* text) noexcept
{
  if (text != nullptr && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", text);
  }
}

} // namespace ggm::gui::ui
