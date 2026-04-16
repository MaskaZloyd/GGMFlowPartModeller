#pragma once

#include "gui/renderer/fbo.hpp"
#include "gui/renderer/geometry_renderer.hpp"
#include "gui/renderer/render_settings.hpp"

namespace ggm::core {
struct MeridionalGeometry;
struct FlowResults;
} // namespace ggm::core

namespace ggm::gui {

// Draw the 2D geometry viewport panel using FBO rendering.
void
drawGeometryPanel(Fbo& fbo,
                  GeometryRenderer& renderer,
                  const core::MeridionalGeometry& geom,
                  const core::FlowResults* flow,
                  const RenderSettings& renderSettings,
                  bool geometryValid) noexcept;

} // namespace ggm::gui
