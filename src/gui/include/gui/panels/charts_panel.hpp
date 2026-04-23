#pragma once

namespace ggm::core {
struct FlowResults;
} // namespace ggm::core

namespace ggm::gui {

// Draw dockable chart windows with ImPlot.
void
drawChartsPanel(const core::FlowResults* flow, bool flowValid, unsigned int dockspaceId) noexcept;

} // namespace ggm::gui
