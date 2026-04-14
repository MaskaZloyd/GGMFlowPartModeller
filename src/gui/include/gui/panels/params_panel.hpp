#pragma once

#include "gui/commands/edit_command.hpp"

#include <optional>

namespace ggm::gui {

struct ParamsPanelResult {
  // Live-edited params for preview (always set when panel is visible).
  core::PumpParams liveParams;
  // If set, drag just ended — push to undo stack.
  std::optional<EditCommand> finishedEdit;
};

// Draw parameter input panel.
[[nodiscard]] ParamsPanelResult drawParamsPanel(const core::PumpParams& current) noexcept;

} // namespace ggm::gui
