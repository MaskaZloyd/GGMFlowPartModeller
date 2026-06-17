#pragma once

#include "core/flow_solver_types.hpp"
#include "core/geometry.hpp"
#include "gui/renderer/geometry/mesh_renderer_2d.hpp"
#include "gui/renderer/geometry/sdf_line_renderer_2d.hpp"
#include "gui/renderer/render_settings.hpp"
#include "gui/renderer/viewport_map.hpp"

#include <vector>

namespace ggm::gui {

/// Orchestrates meridional-section render passes into the currently bound FBO.
class GeometryRenderer
{
public:
  GeometryRenderer() = default;
  ~GeometryRenderer() = default;
  GeometryRenderer(const GeometryRenderer&) = delete;
  GeometryRenderer& operator=(const GeometryRenderer&) = delete;
  GeometryRenderer(GeometryRenderer&& other) noexcept = default;
  GeometryRenderer& operator=(GeometryRenderer&& other) noexcept = default;

  ViewportMap render(const core::MeridionalGeometry& geom,
                     const core::FlowResults* flow,
                     const RenderSettings& settings,
                     int viewportWidth,
                     int viewportHeight) noexcept;
  void clear(int viewportWidth, int viewportHeight) noexcept;

private:
  SdfLineRenderer2D lineRenderer_;
  MeshRenderer2D meshRenderer_;
  std::vector<LineSegment2D> segmentScratch_;
  std::vector<MeshVertex2D> meshScratch_;
};

} // namespace ggm::gui
