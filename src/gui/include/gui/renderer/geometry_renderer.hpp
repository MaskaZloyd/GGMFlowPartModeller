#pragma once

#include "core/flow_solver_types.hpp"
#include "core/geometry.hpp"
#include "gui/renderer/render_settings.hpp"

#include <vector>

namespace ggm::gui {

// Visible world bounds after the aspect correction. Panels use this to draw
// overlays (legend, scale bar) whose position depends on the pixel-to-world
// mapping.
struct ViewportMap
{
  double minZ = 0.0;
  double maxZ = 0.0;
  double minR = 0.0;
  double maxR = 0.0;
  int widthPx = 0;
  int heightPx = 0;
};

// Renders hub/shroud polylines, grid, streamlines, and mean line
// into the currently bound FBO using OpenGL.
class GeometryRenderer
{
public:
  GeometryRenderer() = default;
  ~GeometryRenderer();
  GeometryRenderer(const GeometryRenderer&) = delete;
  GeometryRenderer& operator=(const GeometryRenderer&) = delete;
  GeometryRenderer(GeometryRenderer&& other) noexcept;
  GeometryRenderer& operator=(GeometryRenderer&& other) noexcept;

  ViewportMap render(const core::MeridionalGeometry& geom,
                     const core::FlowResults* flow,
                     const RenderSettings& settings,
                     int viewportWidth,
                     int viewportHeight) noexcept;

private:
  [[nodiscard]] bool isReady() const noexcept;
  void initGl() noexcept;
  void destroy() noexcept;

  unsigned int vao_ = 0;
  unsigned int vbo_ = 0;
  unsigned int shaderProgram_ = 0;
  int viewportUniformLocation_ = -1;
  int colorUniformLocation_ = -1;
  int useVertexColorUniformLocation_ = -1;
  int alphaUniformLocation_ = -1;
  float maxLineWidth_ = 1.0F;
  std::vector<float> scratchVertices_;
};

} // namespace ggm::gui
