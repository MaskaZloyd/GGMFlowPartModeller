#pragma once

#include "core/blade_params.hpp"
#include "core/blade_results.hpp"
#include "core/flow_solver_types.hpp"
#include "core/geometry.hpp"
#include "core/pump_params.hpp"
#include "gui/renderer/blade_plan_renderer.hpp"
#include "gui/renderer/fbo.hpp"

#include <optional>
#include <string>

namespace ggm::gui {

struct BladeDesignPanelState
{
  std::optional<core::PumpParams> lastPumpParams;
  std::optional<core::BladeDesignResults> results;
  std::string statusMessage;
  bool resultsStale = true;
};

struct BladeDesignPanelResult
{
  core::BladeDesignParams params;
  bool paramsChanged = false;
};

[[nodiscard]] BladeDesignPanelResult
drawBladeDesignPanel(BladeDesignPanelState& state,
                     Fbo& bladePlanFbo,
                     BladePlanRenderer& bladePlanRenderer,
                     const core::BladeDesignParams& params,
                     const core::PumpParams& pumpParams,
                     const core::MeridionalGeometry& geometry,
                     const core::FlowResults* flow,
                     bool geometryValid,
                     unsigned int dockspaceId) noexcept;

}
