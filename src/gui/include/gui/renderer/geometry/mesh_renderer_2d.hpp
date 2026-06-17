#pragma once

#include "gui/renderer/geometry/geometry_render_types.hpp"
#include "gui/renderer/geometry/plot_viewport.hpp"
#include "gui/renderer/opengl/gl_buffer.hpp"
#include "gui/renderer/opengl/gl_shader_program.hpp"
#include "gui/renderer/opengl/gl_vertex_array.hpp"

#include <span>
#include <vector>

namespace ggm::gui {

class MeshRenderer2D
{
public:
  MeshRenderer2D() = default;
  ~MeshRenderer2D() = default;
  MeshRenderer2D(const MeshRenderer2D&) = delete;
  MeshRenderer2D& operator=(const MeshRenderer2D&) = delete;
  MeshRenderer2D(MeshRenderer2D&&) noexcept = default;
  MeshRenderer2D& operator=(MeshRenderer2D&&) noexcept = default;

  void drawTriangles(std::span<const MeshVertex2D> vertices, const PlotViewport& viewport) noexcept;

private:
  struct Vertex
  {
    float x = 0.0F;
    float y = 0.0F;
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;
  };

  [[nodiscard]] bool ensureReady() noexcept;

  GlShaderProgram shader_;
  GlVertexArray vao_;
  GlBuffer vbo_;
  int framebufferUniform_ = -1;
  std::vector<Vertex> vertices_;
};

} // namespace ggm::gui
