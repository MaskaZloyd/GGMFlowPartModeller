#pragma once

#include "gui/renderer/geometry/geometry_render_types.hpp"
#include "gui/renderer/geometry/plot_viewport.hpp"
#include "gui/renderer/opengl/gl_buffer.hpp"
#include "gui/renderer/opengl/gl_shader_program.hpp"
#include "gui/renderer/opengl/gl_vertex_array.hpp"

#include <span>
#include <vector>

namespace ggm::gui {

struct PixelRect
{
  double minX = 0.0;
  double minY = 0.0;
  double maxX = 0.0;
  double maxY = 0.0;
};

[[nodiscard]] bool
clipSegmentToRect(PixelPoint& a, PixelPoint& b, const PixelRect& rect) noexcept;

class SdfLineRenderer2D
{
public:
  SdfLineRenderer2D() = default;
  ~SdfLineRenderer2D() = default;
  SdfLineRenderer2D(const SdfLineRenderer2D&) = delete;
  SdfLineRenderer2D& operator=(const SdfLineRenderer2D&) = delete;
  SdfLineRenderer2D(SdfLineRenderer2D&&) noexcept = default;
  SdfLineRenderer2D& operator=(SdfLineRenderer2D&&) noexcept = default;

  void drawPolyline(std::span<const math::Vec2> points,
                    const LineStyle& style,
                    const PlotViewport& viewport) noexcept;
  void drawSegments(std::span<const LineSegment2D> segments, const PlotViewport& viewport) noexcept;

private:
  struct Vertex
  {
    float x = 0.0F;
    float y = 0.0F;
    float startX = 0.0F;
    float startY = 0.0F;
    float endX = 0.0F;
    float endY = 0.0F;
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;
    float thicknessPx = 1.0F;
    float dashPeriodPx = 0.0F;
    float dashDuty = 1.0F;
    float dashOffsetPx = 0.0F;
  };

  [[nodiscard]] bool ensureReady() noexcept;
  void drawPreparedVertices(const PlotViewport& viewport) noexcept;
  void appendSegment(const LineSegment2D& segment,
                     const PlotViewport& viewport,
                     bool outlinePass) noexcept;

  GlShaderProgram shader_;
  GlVertexArray vao_;
  GlBuffer vbo_;
  int framebufferUniform_ = -1;
  std::vector<Vertex> vertices_;
  std::vector<LineSegment2D> segmentScratch_;
};

} // namespace ggm::gui
