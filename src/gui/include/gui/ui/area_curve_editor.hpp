#pragma once

#include "core/target_area_curve.hpp"

namespace ggm::ui {

struct AreaCurveEditorState
{
  ggm::core::TargetAreaCurve targetCurve;

  bool curveChanged{false};
  bool requestPreview{false};
  bool requestOptimization{false};

  int selectedPoint{-1};
  bool dragInProgress{false};
  bool editedWhileDragging{false};
};

void
drawAreaCurveEditor(AreaCurveEditorState& state);

} // namespace ggm::ui
