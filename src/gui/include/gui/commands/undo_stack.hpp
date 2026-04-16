#pragma once

#include "gui/commands/edit_command.hpp"

#include <deque>
#include <string_view>

namespace ggm::gui {

class UndoStack
{
public:
  static constexpr int MAX_DEPTH = 100;

  void push(EditCommand cmd);
  void clear() noexcept;

  [[nodiscard]] bool canUndo() const noexcept;
  [[nodiscard]] bool canRedo() const noexcept;

  // Returns the params to restore. Caller must check canUndo()/canRedo() first.
  [[nodiscard]] const core::PumpParams& undoParams() const noexcept;
  [[nodiscard]] const core::PumpParams& redoParams() const noexcept;

  [[nodiscard]] std::string_view undoLabel() const noexcept;
  [[nodiscard]] std::string_view redoLabel() const noexcept;

  void undo() noexcept;
  void redo() noexcept;

private:
  std::deque<EditCommand> stack_;
  int cursor_ = -1;
};

} // namespace ggm::gui
