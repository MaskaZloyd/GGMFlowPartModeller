#pragma once

#include "core/async_geometry_optimizer.hpp"
#include "core/geometry_optimizer.hpp"
#include "core/pump_params.hpp"
#include "gui/panels/geometry_panel.hpp"
#include "gui/renderer/fbo.hpp"
#include "gui/renderer/geometry_renderer.hpp"
#include "gui/renderer/render_settings.hpp"
#include "gui/ui/area_curve_editor.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ggm::gui {

struct ReverseDesignPanelState
{
  ui::AreaCurveEditorState editorState;
  core::GeometryOptimizationSettings optimizationSettings;
  core::GeometryDesignBounds bounds;
  core::PumpParams currentParams;
  core::GeometryObjectiveBreakdown currentObjective;
  core::MeridionalGeometry previewGeometry;
  core::AreaProfile previewAreaProfile;
  RenderSettings previewRenderSettings;
  double comparisonReferenceOutletArea = 0.0;

  std::vector<double> xiSamples;
  std::vector<double> targetSamples;
  std::vector<double> currentSamples;
  std::vector<double> residualSamples;

  std::string statusMessage;

  bool hasCurrentParams = false;
  bool hasPreview = false;
  bool hasOptimizationResult = false;
  bool lastOptimizationConverged = false;
  int lastOptimizationGenerations = 0;

  std::unique_ptr<core::AsyncGeometryOptimizer> asyncOptimizer =
    std::make_unique<core::AsyncGeometryOptimizer>();

  Fbo previewFbo;
  GeometryRenderer previewRenderer;
  GeometryPanelState previewPanelState;
};

struct ReverseDesignPanelResult
{
  std::optional<core::PumpParams> paramsForMeridional;
};

[[nodiscard]] ReverseDesignPanelResult
drawReverseDesignPanel(ReverseDesignPanelState& state,
                       const core::PumpParams& meridionalParams,
                       unsigned int dockspaceId);

} // namespace ggm::gui
