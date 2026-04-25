#pragma once

#include "gui/commands/edit_command.hpp"

#include <optional>

namespace ggm::gui {

struct ParamsPanelState
{
  core::PumpParams beforeEdit;
  bool active = false;
};

struct ParamsPanelResult
{
  /// Live-edited params for preview (always set when panel is visible).
  core::PumpParams liveParams;
  /// If set, drag just ended — push to undo stack.
  std::optional<EditCommand> finishedEdit;
};

/// Draw parameter input panel. The state is caller-owned and preserved
/// across frames to track drag lifetime (start/finish).
[[nodiscard]] ParamsPanelResult
drawParamsPanel(const core::PumpParams& current,
                ParamsPanelState& state,
                unsigned int dockspaceId) noexcept;

}
