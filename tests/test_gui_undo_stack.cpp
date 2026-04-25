#include "core/pump_params.hpp"
#include "gui/commands/edit_command.hpp"
#include "gui/commands/undo_stack.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace ggm;

namespace {

gui::EditCommand
makeCmd(double before, double after)
{
  core::PumpParams p0;
  p0.xa = before;
  core::PumpParams p1;
  p1.xa = after;
  return gui::EditCommand{.before = p0, .after = p1, .label = "test"};
}

}

TEST_CASE("UndoStack: initially cannot undo or redo", "[undo_stack]")
{
  gui::UndoStack stack;
  REQUIRE(!stack.canUndo());
  REQUIRE(!stack.canRedo());
}

TEST_CASE("UndoStack: can undo after a push", "[undo_stack]")
{
  gui::UndoStack stack;
  stack.push(makeCmd(1.0, 2.0));
  REQUIRE(stack.canUndo());
}

TEST_CASE("UndoStack: cannot redo after push only", "[undo_stack]")
{
  gui::UndoStack stack;
  stack.push(makeCmd(1.0, 2.0));
  REQUIRE(!stack.canRedo());
}

TEST_CASE("UndoStack: undo restores previous params", "[undo_stack]")
{
  gui::UndoStack stack;
  core::PumpParams before;
  before.xa = 10.0;
  core::PumpParams after;
  after.xa = 20.0;
  stack.push(gui::EditCommand{.before = before, .after = after, .label = "set xa"});
  REQUIRE(stack.undoParams().xa == before.xa);
}

TEST_CASE("UndoStack: redo available after undo", "[undo_stack]")
{
  gui::UndoStack stack;
  stack.push(makeCmd(1.0, 2.0));
  stack.undo();
  REQUIRE(stack.canRedo());
}

TEST_CASE("UndoStack: push truncates redo history", "[undo_stack]")
{
  gui::UndoStack stack;
  stack.push(makeCmd(1.0, 2.0));
  stack.push(makeCmd(2.0, 3.0));
  stack.undo();
  REQUIRE(stack.canRedo());
  stack.push(makeCmd(2.0, 4.0));
  REQUIRE(!stack.canRedo());
}

TEST_CASE("UndoStack: undo then redo restores forward state", "[undo_stack]")
{
  gui::UndoStack stack;
  core::PumpParams p0;
  p0.xa = 5.0;
  core::PumpParams p1;
  p1.xa = 10.0;
  stack.push(gui::EditCommand{.before = p0, .after = p1, .label = "xa"});

  stack.undo();
  REQUIRE(stack.canRedo());
  REQUIRE(stack.redoParams().xa == p1.xa);

  stack.redo();
  REQUIRE(!stack.canRedo());
  REQUIRE(!stack.canUndo() == false);
}
