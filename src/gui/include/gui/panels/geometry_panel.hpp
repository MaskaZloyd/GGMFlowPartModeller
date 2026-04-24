#pragma once

#include "core/geometry.hpp"
#include "gui/renderer/fbo.hpp"
#include "gui/renderer/geometry_renderer.hpp"
#include "gui/renderer/render_settings.hpp"

#include <optional>
#include <string>

namespace ggm::core {
struct FlowResults;
} // namespace ggm::core

namespace ggm::gui {

// Per-panel state: snapshot slot for before/after comparisons and a
// transient message shown after e.g. a PNG/PPM export.
struct GeometryPanelState
{
  std::optional<core::MeridionalGeometry> snapshot;
  std::string exportMessage;
  double exportMessageExpiresAt = 0.0; // ImGui::GetTime() value; 0 = hidden
};

void
drawGeometryPanelWithTitle(const char* windowTitle,
                           Fbo& fbo,
                           GeometryRenderer& renderer,
                           GeometryPanelState& state,
                           const core::MeridionalGeometry& geom,
                           const core::FlowResults* flow,
                           const RenderSettings& renderSettings,
                           bool geometryValid,
                           unsigned int dockspaceId) noexcept;

// Draw the 2D geometry viewport panel using FBO rendering.
void
drawGeometryPanel(Fbo& fbo,
                  GeometryRenderer& renderer,
                  GeometryPanelState& state,
                  const core::MeridionalGeometry& geom,
                  const core::FlowResults* flow,
                  const RenderSettings& renderSettings,
                  bool geometryValid,
                  unsigned int dockspaceId) noexcept;

} // namespace ggm::gui
