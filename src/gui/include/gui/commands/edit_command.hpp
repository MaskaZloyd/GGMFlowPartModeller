#pragma once

#include "core/pump_params.hpp"

#include <string>

namespace ggm::gui {

/// A snapshot-based undoable edit.
/// Stores complete before/after PumpParams (112 bytes each — cheap to copy).
struct EditCommand
{
  core::PumpParams before;
  core::PumpParams after;
  std::string label;
};

}
