#pragma once

namespace ggm::core {
struct FlowResults;
} // namespace ggm::core

namespace ggm::gui {

// Draw charts panel with area profile plot (ImPlot).
void
drawChartsPanel(const core::FlowResults* flow, bool flowValid) noexcept;

} // namespace ggm::gui
