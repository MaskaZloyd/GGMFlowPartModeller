#pragma once

namespace ggm::gui {

// Render the "Log" panel. Reads the last formatted messages from the shared
// ring-buffer sink exposed by ggm::logging::guiSink().
void
drawLogPanel() noexcept;

} // namespace ggm::gui
