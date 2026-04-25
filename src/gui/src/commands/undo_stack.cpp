#include "gui/commands/undo_stack.hpp"

#include <utility>

namespace ggm::gui {

void
UndoStack::push(EditCommand cmd)
{

  while (static_cast<int>(stack_.size()) > cursor_ + 1) {
    stack_.pop_back();
  }

  stack_.push_back(std::move(cmd));
  ++cursor_;

  while (static_cast<int>(stack_.size()) > MAX_DEPTH) {
    stack_.pop_front();
    --cursor_;
  }
}

void
UndoStack::clear() noexcept
{
  stack_.clear();
  cursor_ = -1;
}

bool
UndoStack::canUndo() const noexcept
{
  return cursor_ >= 0;
}

bool
UndoStack::canRedo() const noexcept
{
  return cursor_ < static_cast<int>(stack_.size()) - 1;
}

const core::PumpParams&
UndoStack::undoParams() const noexcept
{
  return stack_[static_cast<std::size_t>(cursor_)].before;
}

const core::PumpParams&
UndoStack::redoParams() const noexcept
{
  return stack_[static_cast<std::size_t>(cursor_ + 1)].after;
}

std::string_view
UndoStack::undoLabel() const noexcept
{
  return stack_[static_cast<std::size_t>(cursor_)].label;
}

std::string_view
UndoStack::redoLabel() const noexcept
{
  return stack_[static_cast<std::size_t>(cursor_ + 1)].label;
}

void
UndoStack::undo() noexcept
{
  if (canUndo()) {
    --cursor_;
  }
}

void
UndoStack::redo() noexcept
{
  if (canRedo()) {
    ++cursor_;
  }
}

}
