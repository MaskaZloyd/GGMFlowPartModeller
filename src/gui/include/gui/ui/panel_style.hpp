#pragma once

// Common styling primitives for dockable panels — constants, colors, and
// input helpers. Use these instead of re-rolling per-panel widgets so the
// two modules (meridional / reverse design) stay visually consistent.

#include <imgui.h>

namespace ggm::gui::style {

// Fixed input width used across all panels so labels line up and the eye
// has a single rhythm to follow.
inline constexpr float PANEL_INPUT_WIDTH = 120.0F;

// Default numeric formats for drag widgets.
inline constexpr const char* PANEL_DRAG_FORMAT_DOUBLE = "%.3f";
inline constexpr const char* PANEL_DRAG_FORMAT_INT = "%d";

// Drag speeds.
inline constexpr float PANEL_DRAG_SPEED_DEFAULT = 0.1F;
inline constexpr float PANEL_DRAG_SPEED_SLOW = 0.01F;

// Accent/status palette. Kept in-sync with the ImGui dark theme so colored
// dots and buttons read well against the default window background.
inline constexpr ImVec4 PANEL_COLOR_INFO{ 0.70F, 0.76F, 0.83F, 1.0F };
inline constexpr ImVec4 PANEL_COLOR_SUCCESS{ 0.35F, 0.80F, 0.45F, 1.0F };
inline constexpr ImVec4 PANEL_COLOR_WARN{ 0.95F, 0.72F, 0.25F, 1.0F };
inline constexpr ImVec4 PANEL_COLOR_ERROR{ 0.92F, 0.40F, 0.40F, 1.0F };
inline constexpr ImVec4 PANEL_COLOR_ACCENT{ 0.20F, 0.48F, 0.82F, 1.0F };
inline constexpr ImVec4 PANEL_COLOR_ACCENT_HOV{ 0.26F, 0.56F, 0.92F, 1.0F };
inline constexpr ImVec4 PANEL_COLOR_DANGER{ 0.82F, 0.32F, 0.32F, 1.0F };
inline constexpr ImVec4 PANEL_COLOR_DANGER_HOV{ 0.92F, 0.40F, 0.40F, 1.0F };

// Return value for drag helpers: reports both whether the value changed this
// frame AND whether the widget was just activated (first mouse-down / key
// focus). Callers that need edit-batching keyed on activation use the second
// flag; others ignore it.
struct DragResult
{
  bool changed = false;
  bool activated = false;
};

// Fixed-width DragScalar<double> with the label rendered to the right.
// `minVal` / `maxVal` may be null for unconstrained drags. `tooltip` is
// optional and shown on hover.
DragResult
fixedDragDouble(const char* id,
                const char* label,
                double* value,
                const double* minVal = nullptr,
                const double* maxVal = nullptr,
                const char* format = PANEL_DRAG_FORMAT_DOUBLE,
                float speed = PANEL_DRAG_SPEED_DEFAULT,
                const char* tooltip = nullptr) noexcept;

DragResult
fixedDragInt(const char* id,
             const char* label,
             int* value,
             int minVal,
             int maxVal,
             const char* tooltip = nullptr) noexcept;

DragResult
fixedDragFloat(const char* id,
               const char* label,
               float* value,
               float minVal,
               float maxVal,
               const char* format = "%.2f",
               float speed = PANEL_DRAG_SPEED_DEFAULT,
               const char* tooltip = nullptr) noexcept;

// Full-width colored buttons. Returned true when clicked. `disabled` pushes
// `ImGui::BeginDisabled`, so callers don't need to wrap themselves.
bool
accentButton(const char* label, bool disabled = false) noexcept;

bool
dangerButton(const char* label, bool disabled = false) noexcept;

// Push/Pop the panel-wide style (frame padding + item spacing). Use at the
// top of a panel's Begin/End block to give the content breathing room.
void
pushPanelStyle() noexcept;

void
popPanelStyle() noexcept;

// Shows a colored dot + text on a single line, using the given color.
// Useful for status lines inside panels.
void
drawColoredStatusLine(const ImVec4& color, const char* text) noexcept;

// Shows the tooltip string if the previous item is hovered. No-op for null.
void
tooltipIfHovered(const char* text) noexcept;

} // namespace ggm::gui::ui
