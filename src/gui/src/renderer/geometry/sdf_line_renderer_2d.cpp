#include "gui/renderer/geometry/sdf_line_renderer_2d.hpp"

#include "renderer/gl_headers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace ggm::gui {
namespace {

constexpr std::string_view kVertexShaderSource = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPosPx;
layout(location = 1) in vec2 aStartPx;
layout(location = 2) in vec2 aEndPx;
layout(location = 3) in vec4 aColor;
layout(location = 4) in float aThicknessPx;
layout(location = 5) in float aDashPeriodPx;
layout(location = 6) in float aDashDuty;
layout(location = 7) in float aDashOffsetPx;

uniform vec2 uFramebufferSize;

out vec2 vFragPx;
out vec2 vStartPx;
out vec2 vEndPx;
out vec4 vColor;
out float vThicknessPx;
out float vDashPeriodPx;
out float vDashDuty;
out float vDashOffsetPx;

void main() {
    vec2 ndc = (2.0 * aPosPx / uFramebufferSize) - vec2(1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vFragPx = aPosPx;
    vStartPx = aStartPx;
    vEndPx = aEndPx;
    vColor = aColor;
    vThicknessPx = aThicknessPx;
    vDashPeriodPx = aDashPeriodPx;
    vDashDuty = aDashDuty;
    vDashOffsetPx = aDashOffsetPx;
}
)glsl";

constexpr std::string_view kFragmentShaderSource = R"glsl(
#version 330 core
in vec2 vFragPx;
in vec2 vStartPx;
in vec2 vEndPx;
in vec4 vColor;
in float vThicknessPx;
in float vDashPeriodPx;
in float vDashDuty;
in float vDashOffsetPx;

out vec4 FragColor;

float sdSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a;
    vec2 ba = b - a;
    float denom = dot(ba, ba);
    if (denom < 1e-8) {
        return length(pa);
    }
    float h = clamp(dot(pa, ba) / denom, 0.0, 1.0);
    return length(pa - ba * h);
}

float segmentAlong(vec2 p, vec2 a, vec2 b) {
    vec2 ba = b - a;
    float denom = dot(ba, ba);
    if (denom < 1e-8) {
        return 0.0;
    }
    float h = clamp(dot(p - a, ba) / denom, 0.0, 1.0);
    return h * sqrt(denom);
}

void main() {
    float dashDuty = clamp(vDashDuty, 0.0, 1.0);
    if (vDashPeriodPx > 0.0 && dashDuty < 1.0) {
        float phase = mod(vDashOffsetPx + segmentAlong(vFragPx, vStartPx, vEndPx), vDashPeriodPx);
        if ((phase / vDashPeriodPx) > dashDuty) {
            discard;
        }
    }

    float dist = sdSegment(vFragPx, vStartPx, vEndPx);
    float halfWidth = max(0.0, 0.5 * vThicknessPx);
    float aa = 1.0;
    float alpha = 1.0 - smoothstep(halfWidth - aa, halfWidth + aa, dist);
    FragColor = vec4(vColor.rgb, vColor.a * alpha);
}
)glsl";

[[nodiscard]] bool
isFinite(const math::Vec2& point) noexcept
{
  return std::isfinite(point.x()) && std::isfinite(point.y());
}

[[nodiscard]] double
distance(PixelPoint a, PixelPoint b) noexcept
{
  return std::hypot(a.x - b.x, a.y - b.y);
}

[[nodiscard]] Rgba
outlineColor(const LineStyle& style) noexcept
{
  auto color = style.outlineColor;
  if (color.a <= 0.0F) {
    color.a = style.color.a;
  }
  return color;
}

[[nodiscard]] LineStyle
styleForPass(const LineStyle& style, bool outlinePass) noexcept
{
  if (!outlinePass) {
    return style;
  }

  auto outline = style;
  outline.color = outlineColor(style);
  outline.thicknessPx = style.outlineThicknessPx;
  return outline;
}

[[nodiscard]] PixelRect
expandedViewportRect(const PlotViewport& viewport, float thicknessPx) noexcept
{
  const auto margin = std::max(4.0, 0.5 * static_cast<double>(thicknessPx) + 2.0);
  return PixelRect{
    .minX = -margin,
    .minY = -margin,
    .maxX = static_cast<double>(viewport.widthPx()) + margin,
    .maxY = static_cast<double>(viewport.heightPx()) + margin,
  };
}

} // namespace

bool
clipSegmentToRect(PixelPoint& a, PixelPoint& b, const PixelRect& rect) noexcept
{
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  double t0 = 0.0;
  double t1 = 1.0;

  const auto clip = [&](double p, double q) noexcept {
    if (std::abs(p) < 1.0e-12) {
      return q >= 0.0;
    }
    const double r = q / p;
    if (p < 0.0) {
      if (r > t1) {
        return false;
      }
      if (r > t0) {
        t0 = r;
      }
      return true;
    }
    if (r < t0) {
      return false;
    }
    if (r < t1) {
      t1 = r;
    }
    return true;
  };

  if (!clip(-dx, a.x - rect.minX) || !clip(dx, rect.maxX - a.x) || !clip(-dy, a.y - rect.minY) ||
      !clip(dy, rect.maxY - a.y)) {
    return false;
  }

  const auto originalA = a;
  if (t1 < 1.0) {
    b = PixelPoint{.x = originalA.x + (t1 * dx), .y = originalA.y + (t1 * dy)};
  }
  if (t0 > 0.0) {
    a = PixelPoint{.x = originalA.x + (t0 * dx), .y = originalA.y + (t0 * dy)};
  }
  return true;
}

void
SdfLineRenderer2D::drawPolyline(std::span<const math::Vec2> points,
                                const LineStyle& style,
                                const PlotViewport& viewport) noexcept
{
  if (points.size() < 2U) {
    return;
  }

  segmentScratch_.clear();
  segmentScratch_.reserve(points.size() - 1U);

  double dashOffset = 0.0;
  for (std::size_t i = 1U; i < points.size(); ++i) {
    const auto& a = points[i - 1U];
    const auto& b = points[i];
    if (!isFinite(a) || !isFinite(b)) {
      dashOffset = 0.0;
      continue;
    }

    segmentScratch_.push_back(
      LineSegment2D{.a = a, .b = b, .style = style, .dashOffsetPx = dashOffset});

    const auto pa = viewport.dataToPixel(a);
    const auto pb = viewport.dataToPixel(b);
    dashOffset += distance(pa, pb);
  }

  drawSegments(segmentScratch_, viewport);
}

void
SdfLineRenderer2D::drawSegments(std::span<const LineSegment2D> segments,
                                const PlotViewport& viewport) noexcept
{
  if (segments.empty() || !viewport.hasData() || !ensureReady()) {
    return;
  }

  vertices_.clear();
  for (const auto& segment : segments) {
    if (segment.style.outlineThicknessPx > segment.style.thicknessPx &&
        segment.style.outlineThicknessPx > 0.0F) {
      appendSegment(segment, viewport, true);
    }
  }
  drawPreparedVertices(viewport);

  vertices_.clear();
  for (const auto& segment : segments) {
    appendSegment(segment, viewport, false);
  }
  drawPreparedVertices(viewport);
}

bool
SdfLineRenderer2D::ensureReady() noexcept
{
  if (shader_.isValid() && vao_.isValid() && vbo_.isValid() && framebufferUniform_ >= 0) {
    return true;
  }

  shader_.destroy();
  vao_.destroy();
  vbo_.destroy();
  framebufferUniform_ = -1;

  if (!shader_.create(kVertexShaderSource, kFragmentShaderSource, "sdf_line_2d")) {
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
  vao_.enableAttribute(1, 2, GL_FLOAT, false, stride, offsetof(Vertex, startX));
  vao_.enableAttribute(2, 2, GL_FLOAT, false, stride, offsetof(Vertex, endX));
  vao_.enableAttribute(3, 4, GL_FLOAT, false, stride, offsetof(Vertex, r));
  vao_.enableAttribute(4, 1, GL_FLOAT, false, stride, offsetof(Vertex, thicknessPx));
  vao_.enableAttribute(5, 1, GL_FLOAT, false, stride, offsetof(Vertex, dashPeriodPx));
  vao_.enableAttribute(6, 1, GL_FLOAT, false, stride, offsetof(Vertex, dashDuty));
  vao_.enableAttribute(7, 1, GL_FLOAT, false, stride, offsetof(Vertex, dashOffsetPx));
  GlBuffer::unbind(GL_ARRAY_BUFFER);
  GlVertexArray::unbind();
  return true;
}

void
SdfLineRenderer2D::drawPreparedVertices(const PlotViewport& viewport) noexcept
{
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

void
SdfLineRenderer2D::appendSegment(const LineSegment2D& segment,
                                 const PlotViewport& viewport,
                                 bool outlinePass) noexcept
{
  auto style = styleForPass(segment.style, outlinePass);
  style.dashDuty = std::clamp(style.dashDuty, 0.0F, 1.0F);
  if (style.thicknessPx <= 0.0F || style.color.a <= 0.0F || !isFinite(segment.a) ||
      !isFinite(segment.b)) {
    return;
  }

  const auto originalA = viewport.dataToPixel(segment.a);
  const auto originalB = viewport.dataToPixel(segment.b);
  auto clippedA = originalA;
  auto clippedB = originalB;
  if (!std::isfinite(clippedA.x) || !std::isfinite(clippedA.y) || !std::isfinite(clippedB.x) ||
      !std::isfinite(clippedB.y)) {
    return;
  }

  const auto clipRect = expandedViewportRect(viewport, style.thicknessPx);
  if (!clipSegmentToRect(clippedA, clippedB, clipRect)) {
    return;
  }

  const auto length = distance(clippedA, clippedB);
  double tx = 1.0;
  double ty = 0.0;
  if (length > 1.0e-9) {
    tx = (clippedB.x - clippedA.x) / length;
    ty = (clippedB.y - clippedA.y) / length;
  }
  const double nx = -ty;
  const double ny = tx;
  const double extent = (0.5 * static_cast<double>(style.thicknessPx)) + 2.0;

  const auto start = PixelPoint{.x = clippedA.x - (tx * extent), .y = clippedA.y - (ty * extent)};
  const auto end = PixelPoint{.x = clippedB.x + (tx * extent), .y = clippedB.y + (ty * extent)};
  const auto v0 = PixelPoint{.x = start.x + (nx * extent), .y = start.y + (ny * extent)};
  const auto v1 = PixelPoint{.x = start.x - (nx * extent), .y = start.y - (ny * extent)};
  const auto v2 = PixelPoint{.x = end.x - (nx * extent), .y = end.y - (ny * extent)};
  const auto v3 = PixelPoint{.x = end.x + (nx * extent), .y = end.y + (ny * extent)};

  const auto dashOffset = static_cast<float>(segment.dashOffsetPx + distance(originalA, clippedA));
  const auto dashPeriod = std::max(style.dashPeriodPx, 0.0F);

  const auto appendVertex = [&](PixelPoint point) {
    vertices_.push_back(Vertex{
      .x = static_cast<float>(point.x),
      .y = static_cast<float>(point.y),
      .startX = static_cast<float>(clippedA.x),
      .startY = static_cast<float>(clippedA.y),
      .endX = static_cast<float>(clippedB.x),
      .endY = static_cast<float>(clippedB.y),
      .r = style.color.r,
      .g = style.color.g,
      .b = style.color.b,
      .a = style.color.a,
      .thicknessPx = style.thicknessPx,
      .dashPeriodPx = dashPeriod,
      .dashDuty = style.dashDuty,
      .dashOffsetPx = dashOffset,
    });
  };

  appendVertex(v0);
  appendVertex(v1);
  appendVertex(v2);
  appendVertex(v0);
  appendVertex(v2);
  appendVertex(v3);
}

} // namespace ggm::gui
