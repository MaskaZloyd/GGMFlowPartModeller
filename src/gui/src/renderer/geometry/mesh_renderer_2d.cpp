#include "gui/renderer/geometry/mesh_renderer_2d.hpp"

#include "renderer/gl_headers.hpp"

#include <cmath>
#include <cstddef>
#include <span>
#include <string_view>

namespace ggm::gui {
namespace {

constexpr std::string_view kVertexShaderSource = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPosPx;
layout(location = 1) in vec4 aColor;

uniform vec2 uFramebufferSize;

out vec4 vColor;

void main() {
    vec2 ndc = (2.0 * aPosPx / uFramebufferSize) - vec2(1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vColor = aColor;
}
)glsl";

constexpr std::string_view kFragmentShaderSource = R"glsl(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)glsl";

[[nodiscard]] bool
isFinite(const math::Vec2& point) noexcept
{
  return std::isfinite(point.x()) && std::isfinite(point.y());
}

} // namespace

void
MeshRenderer2D::drawTriangles(std::span<const MeshVertex2D> vertices,
                              const PlotViewport& viewport) noexcept
{
  if (vertices.size() < 3U || !viewport.hasData() || !ensureReady()) {
    return;
  }

  vertices_.clear();
  vertices_.reserve(vertices.size());
  const auto appendVertex = [&](const MeshVertex2D& vertex) {
    const auto pixel = viewport.dataToPixel(vertex.point);
    vertices_.push_back(Vertex{
      .x = static_cast<float>(pixel.x),
      .y = static_cast<float>(pixel.y),
      .r = vertex.color.r,
      .g = vertex.color.g,
      .b = vertex.color.b,
      .a = vertex.color.a,
    });
  };

  for (std::size_t i = 0; i + 2U < vertices.size(); i += 3U) {
    const auto& a = vertices[i];
    const auto& b = vertices[i + 1U];
    const auto& c = vertices[i + 2U];
    if (!isFinite(a.point) || !isFinite(b.point) || !isFinite(c.point)) {
      continue;
    }
    appendVertex(a);
    appendVertex(b);
    appendVertex(c);
  }

  if (vertices_.empty()) {
    return;
  }

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  shader_.use();
  shader_.setUniform2f(framebufferUniform_,
                       static_cast<float>(viewport.widthPx()),
                       static_cast<float>(viewport.heightPx()));
  vao_.bind();
  vbo_.upload(std::span<const Vertex>{vertices_.data(), vertices_.size()}, GL_DYNAMIC_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
  GlBuffer::unbind(GL_ARRAY_BUFFER);
  GlVertexArray::unbind();
  GlShaderProgram::unuse();
}

bool
MeshRenderer2D::ensureReady() noexcept
{
  if (shader_.isValid() && vao_.isValid() && vbo_.isValid() && framebufferUniform_ >= 0) {
    return true;
  }

  shader_.destroy();
  vao_.destroy();
  vbo_.destroy();
  framebufferUniform_ = -1;

  if (!shader_.create(kVertexShaderSource, kFragmentShaderSource, "mesh_2d")) {
    return false;
  }
  framebufferUniform_ = shader_.uniformLocation("uFramebufferSize");
  if (framebufferUniform_ < 0 || !vao_.create() || !vbo_.create(GL_ARRAY_BUFFER)) {
    shader_.destroy();
    vao_.destroy();
    vbo_.destroy();
    framebufferUniform_ = -1;
    return false;
  }

  vao_.bind();
  vbo_.bind();
  const auto stride = static_cast<int>(sizeof(Vertex));
  vao_.enableAttribute(0, 2, GL_FLOAT, false, stride, offsetof(Vertex, x));
  vao_.enableAttribute(1, 4, GL_FLOAT, false, stride, offsetof(Vertex, r));
  GlBuffer::unbind(GL_ARRAY_BUFFER);
  GlVertexArray::unbind();
  return true;
}

} // namespace ggm::gui
