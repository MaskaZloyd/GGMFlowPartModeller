#include "gui/renderer/blade_plan_renderer.hpp"

#include "gl_headers.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <span>
#include <string_view>
#include <utility>

namespace ggm::gui {

namespace {

using VertexSpan = std::span<const float>;

constexpr std::string_view kVertexShaderSource = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec4 uViewport;
void main() {
    float ndcX = 2.0 * (aPos.x - uViewport.x) / uViewport.z - 1.0;
    float ndcY = 2.0 * (aPos.y - uViewport.y) / uViewport.w - 1.0;
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
}
)glsl";

constexpr std::string_view kFragmentShaderSource = R"glsl(
#version 330 core
uniform vec3 uColor;
uniform float uAlpha;
out vec4 FragColor;
void main() {
    FragColor = vec4(uColor, uAlpha);
}
)glsl";

constexpr float kDefaultLineWidth = 1.0F;
constexpr double kViewportPaddingFraction = 0.12;
constexpr int kCircleSegments = 192;
constexpr int kTargetGridLineCount = 10;
constexpr double kMinimumExtent = 1.0e-3;

struct Rgb
{
  float r;
  float g;
  float b;
};

constexpr auto kMinorGridColor = Rgb{0.910F, 0.915F, 0.925F};
constexpr auto kMajorGridColor = Rgb{0.790F, 0.805F, 0.830F};
constexpr auto kAxisColor = Rgb{0.630F, 0.660F, 0.710F};
constexpr auto kInletRingColor = Rgb{0.220F, 0.520F, 0.820F};
constexpr auto kOutletRingColor = Rgb{0.150F, 0.200F, 0.300F};
constexpr auto kBladeFillColor = Rgb{0.285F, 0.470F, 0.780F};
constexpr auto kBladeOutlineColor = Rgb{0.055F, 0.085F, 0.140F};
constexpr auto kCenterLineColor = Rgb{0.850F, 0.285F, 0.160F};
constexpr auto kSideLineColor = Rgb{0.105F, 0.220F, 0.390F};

[[nodiscard]] GLuint
compileShader(const GLenum type, const std::string_view source) noexcept
{
  const auto* sourcePtr = source.data();
  const auto sourceLength = static_cast<GLint>(source.size());
  const auto shader = glCreateShader(type);
  if (shader == 0U) {
    return 0;
  }

  glShaderSource(shader, 1, &sourcePtr, &sourceLength);
  glCompileShader(shader);

  GLint success{};
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (success == GL_FALSE) {
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

struct Bounds
{
  double minX = std::numeric_limits<double>::max();
  double maxX = std::numeric_limits<double>::lowest();
  double minY = std::numeric_limits<double>::max();
  double maxY = std::numeric_limits<double>::lowest();

  void include(double x, double y) noexcept
  {
    minX = std::min(minX, x);
    maxX = std::max(maxX, x);
    minY = std::min(minY, y);
    maxY = std::max(maxY, y);
  }

  void includeRadius(double radius) noexcept
  {
    if (radius <= 0.0 || !std::isfinite(radius)) {
      return;
    }
    include(-radius, -radius);
    include(radius, radius);
  }

  [[nodiscard]] bool hasData() const noexcept { return minX <= maxX && minY <= maxY; }

  void addPadding(double fraction) noexcept
  {
    const auto rangeX = maxX - minX;
    const auto rangeY = maxY - minY;
    minX -= rangeX * fraction;
    maxX += rangeX * fraction;
    minY -= rangeY * fraction;
    maxY += rangeY * fraction;
  }

  void ensureNonDegenerate() noexcept
  {
    ensureAxis(minX, maxX);
    ensureAxis(minY, maxY);
  }

private:
  static void ensureAxis(double& minValue, double& maxValue) noexcept
  {
    if (maxValue > minValue) {
      return;
    }
    const auto center = std::midpoint(minValue, maxValue);
    const auto scale = std::max({std::abs(center), std::abs(minValue), std::abs(maxValue), 1.0});
    const auto halfExtent = std::max(scale * 0.025, kMinimumExtent);
    minValue = center - halfExtent;
    maxValue = center + halfExtent;
  }
};

struct RenderViewport
{
  Bounds bounds;
  double rangeX = 0.0;
  double rangeY = 0.0;

  [[nodiscard]] BladePlanViewport toMap(int width, int height) const noexcept
  {
    return BladePlanViewport{
      .minX = bounds.minX,
      .maxX = bounds.maxX,
      .minY = bounds.minY,
      .maxY = bounds.maxY,
      .widthPx = width,
      .heightPx = height,
    };
  }
};

struct DrawUniforms
{
  GLint viewport = -1;
  GLint color = -1;
  GLint alpha = -1;
};

class DrawSession
{
public:
  DrawSession(GLuint shaderProgram,
              GLuint vao,
              GLuint vbo,
              DrawUniforms uniforms,
              float maxLineWidth,
              std::vector<float>& scratchVertices) noexcept
    : shaderProgram_(shaderProgram),
      vao_(vao),
      vbo_(vbo),
      uniforms_(uniforms),
      maxLineWidth_(maxLineWidth),
      scratchVertices_(scratchVertices)
  {
    glUseProgram(shaderProgram_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * static_cast<GLsizei>(sizeof(float)), nullptr);
    glEnableVertexAttribArray(0);
    setAlpha(1.0F);
  }

  ~DrawSession()
  {
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glLineWidth(kDefaultLineWidth);
    glUseProgram(0);
  }

  DrawSession(const DrawSession&) = delete;
  DrawSession& operator=(const DrawSession&) = delete;

  void setViewport(const RenderViewport& viewport) const noexcept
  {
    glUniform4f(uniforms_.viewport,
                static_cast<float>(viewport.bounds.minX),
                static_cast<float>(viewport.bounds.minY),
                static_cast<float>(viewport.rangeX),
                static_cast<float>(viewport.rangeY));
  }

  void setColor(Rgb color) const noexcept { glUniform3f(uniforms_.color, color.r, color.g, color.b); }
  void setAlpha(float alpha) const noexcept { glUniform1f(uniforms_.alpha, alpha); }
  void setLineWidth(float width) const noexcept
  {
    glLineWidth(std::clamp(width, kDefaultLineWidth, maxLineWidth_));
  }

  void drawVertices(VertexSpan vertices, GLenum mode) const noexcept
  {
    if (vertices.empty()) {
      return;
    }
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size_bytes()),
                 vertices.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(mode, 0, static_cast<GLsizei>(vertices.size() / 2U));
  }

  [[nodiscard]] std::vector<float>& scratch() const noexcept { return scratchVertices_; }

private:
  GLuint shaderProgram_ = 0;
  GLuint vao_ = 0;
  GLuint vbo_ = 0;
  DrawUniforms uniforms_{};
  float maxLineWidth_ = kDefaultLineWidth;
  std::vector<float>& scratchVertices_;
};

[[nodiscard]] double
niceGridStep(double range, int targetLines) noexcept
{
  if (range <= 0.0 || targetLines <= 0) {
    return 1.0;
  }
  const auto rough = range / static_cast<double>(targetLines);
  const auto magnitude = std::pow(10.0, std::floor(std::log10(rough)));
  const auto normalized = rough / magnitude;
  auto nice = 1.0;
  if (normalized >= 5.0) {
    nice = 5.0;
  } else if (normalized >= 2.0) {
    nice = 2.0;
  }
  return nice * magnitude;
}

void
appendLine(std::vector<float>& vertices, double x0, double y0, double x1, double y1)
{
  vertices.push_back(static_cast<float>(x0));
  vertices.push_back(static_cast<float>(y0));
  vertices.push_back(static_cast<float>(x1));
  vertices.push_back(static_cast<float>(y1));
}

void
configureFrame(int viewportWidth, int viewportHeight) noexcept
{
  glViewport(0, 0, viewportWidth, viewportHeight);
  glClearColor(0.988F, 0.990F, 0.994F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_MULTISAMPLE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

[[nodiscard]] RenderViewport
makeViewport(const core::BladeDesignResults& results, int viewportWidth, int viewportHeight) noexcept
{
  auto bounds = Bounds{};
  bounds.includeRadius(results.outletRadiusMm);
  bounds.includeRadius(results.inletRadiusMm);
  bounds.include(0.0, 0.0);

  for (const auto& blade : results.allBlades) {
    for (const auto& point : blade.closedContour) {
      if (std::isfinite(point.xMm) && std::isfinite(point.yMm)) {
        bounds.include(point.xMm, point.yMm);
      }
    }
  }

  bounds.addPadding(kViewportPaddingFraction);
  bounds.ensureNonDegenerate();

  auto rangeX = bounds.maxX - bounds.minX;
  auto rangeY = bounds.maxY - bounds.minY;
  const auto aspectView = static_cast<double>(viewportWidth) / static_cast<double>(viewportHeight);
  const auto aspectData = rangeX / rangeY;

  if (aspectData > aspectView) {
    const auto newRangeY = rangeX / aspectView;
    const auto centerY = std::midpoint(bounds.minY, bounds.maxY);
    bounds.minY = centerY - (newRangeY / 2.0);
    bounds.maxY = centerY + (newRangeY / 2.0);
    rangeY = newRangeY;
  } else {
    const auto newRangeX = rangeY * aspectView;
    const auto centerX = std::midpoint(bounds.minX, bounds.maxX);
    bounds.minX = centerX - (newRangeX / 2.0);
    bounds.maxX = centerX + (newRangeX / 2.0);
    rangeX = newRangeX;
  }

  return RenderViewport{.bounds = bounds, .rangeX = rangeX, .rangeY = rangeY};
}

void
drawGrid(const RenderViewport& viewport, const DrawSession& draw) noexcept
{
  const auto majorStep =
    niceGridStep(std::max(viewport.rangeX, viewport.rangeY), kTargetGridLineCount);
  const auto minorStep = majorStep / 5.0;
  auto& vertices = draw.scratch();

  const auto fillGrid = [&](double step) {
    vertices.clear();
    const auto minX = std::floor(viewport.bounds.minX / step) * step;
    const auto maxX = std::ceil(viewport.bounds.maxX / step) * step;
    const auto minY = std::floor(viewport.bounds.minY / step) * step;
    const auto maxY = std::ceil(viewport.bounds.maxY / step) * step;
    for (auto x = minX; x <= maxX + step * 0.5; x += step) {
      appendLine(vertices, x, minY, x, maxY);
    }
    for (auto y = minY; y <= maxY + step * 0.5; y += step) {
      appendLine(vertices, minX, y, maxX, y);
    }
  };

  draw.setAlpha(1.0F);
  draw.setLineWidth(1.0F);
  draw.setColor(kMinorGridColor);
  fillGrid(minorStep);
  draw.drawVertices(vertices, GL_LINES);

  draw.setLineWidth(1.2F);
  draw.setColor(kMajorGridColor);
  fillGrid(majorStep);
  draw.drawVertices(vertices, GL_LINES);

  vertices.clear();
  appendLine(vertices, viewport.bounds.minX, 0.0, viewport.bounds.maxX, 0.0);
  appendLine(vertices, 0.0, viewport.bounds.minY, 0.0, viewport.bounds.maxY);
  draw.setLineWidth(1.4F);
  draw.setColor(kAxisColor);
  draw.drawVertices(vertices, GL_LINES);
}

void
drawCircle(const DrawSession& draw, double radius, Rgb color, float lineWidth, float alpha) noexcept
{
  if (radius <= 0.0 || !std::isfinite(radius)) {
    return;
  }
  auto& vertices = draw.scratch();
  vertices.clear();
  vertices.reserve((kCircleSegments + 1U) * 2U);
  for (int i = 0; i <= kCircleSegments; ++i) {
    const double t = 2.0 * std::numbers::pi * static_cast<double>(i) /
                     static_cast<double>(kCircleSegments);
    vertices.push_back(static_cast<float>(radius * std::cos(t)));
    vertices.push_back(static_cast<float>(radius * std::sin(t)));
  }
  draw.setColor(color);
  draw.setAlpha(alpha);
  draw.setLineWidth(lineWidth);
  draw.drawVertices(vertices, GL_LINE_STRIP);
}

void
appendContour(std::vector<float>& vertices, const std::vector<core::BladePlanPoint>& points)
{
  vertices.clear();
  vertices.reserve(points.size() * 2U);
  for (const auto& point : points) {
    vertices.push_back(static_cast<float>(point.xMm));
    vertices.push_back(static_cast<float>(point.yMm));
  }
}

[[nodiscard]] double
polygonArea(const std::vector<core::BladePlanPoint>& points) noexcept
{
  double area = 0.0;
  for (std::size_t i = 0; i < points.size(); ++i) {
    const auto& a = points[i];
    const auto& b = points[(i + 1U) % points.size()];
    area += a.xMm * b.yMm - b.xMm * a.yMm;
  }
  return 0.5 * area;
}

[[nodiscard]] double
cross(const core::BladePlanPoint& a,
      const core::BladePlanPoint& b,
      const core::BladePlanPoint& c) noexcept
{
  const double abx = b.xMm - a.xMm;
  const double aby = b.yMm - a.yMm;
  const double acx = c.xMm - a.xMm;
  const double acy = c.yMm - a.yMm;
  return abx * acy - aby * acx;
}

[[nodiscard]] bool
pointInTriangle(const core::BladePlanPoint& p,
                const core::BladePlanPoint& a,
                const core::BladePlanPoint& b,
                const core::BladePlanPoint& c) noexcept
{
  const double c1 = cross(a, b, p);
  const double c2 = cross(b, c, p);
  const double c3 = cross(c, a, p);
  constexpr double eps = 1.0e-9;
  const bool hasNegative = c1 < -eps || c2 < -eps || c3 < -eps;
  const bool hasPositive = c1 > eps || c2 > eps || c3 > eps;
  return !(hasNegative && hasPositive);
}

[[nodiscard]] bool
triangulateClosedContour(const std::vector<core::BladePlanPoint>& closedContour,
                         std::vector<float>& vertices)
{
  vertices.clear();
  if (closedContour.size() < 4U) {
    return false;
  }

  std::vector<core::BladePlanPoint> polygon;
  polygon.reserve(closedContour.size());
  for (const auto& point : closedContour) {
    if (!std::isfinite(point.xMm) || !std::isfinite(point.yMm)) {
      return false;
    }
    if (!polygon.empty()) {
      const auto& prev = polygon.back();
      if (std::hypot(prev.xMm - point.xMm, prev.yMm - point.yMm) < 1.0e-10) {
        continue;
      }
    }
    polygon.push_back(point);
  }
  if (polygon.size() >= 2U) {
    const auto& first = polygon.front();
    const auto& last = polygon.back();
    if (std::hypot(first.xMm - last.xMm, first.yMm - last.yMm) < 1.0e-10) {
      polygon.pop_back();
    }
  }
  if (polygon.size() < 3U) {
    return false;
  }

  const bool ccw = polygonArea(polygon) > 0.0;
  std::vector<std::size_t> indices(polygon.size());
  std::iota(indices.begin(), indices.end(), 0U);

  const auto appendPoint = [&](const core::BladePlanPoint& point) {
    vertices.push_back(static_cast<float>(point.xMm));
    vertices.push_back(static_cast<float>(point.yMm));
  };

  std::size_t guard = 0;
  while (indices.size() > 3U && guard++ < polygon.size() * polygon.size()) {
    bool clipped = false;
    for (std::size_t pos = 0; pos < indices.size(); ++pos) {
      const std::size_t prevIndex = indices[(pos + indices.size() - 1U) % indices.size()];
      const std::size_t currIndex = indices[pos];
      const std::size_t nextIndex = indices[(pos + 1U) % indices.size()];
      const auto& a = polygon[prevIndex];
      const auto& b = polygon[currIndex];
      const auto& c = polygon[nextIndex];
      const double turn = cross(a, b, c);
      if ((ccw && turn <= 1.0e-9) || (!ccw && turn >= -1.0e-9)) {
        continue;
      }

      bool containsPoint = false;
      for (const auto candidateIndex : indices) {
        if (candidateIndex == prevIndex || candidateIndex == currIndex || candidateIndex == nextIndex) {
          continue;
        }
        if (pointInTriangle(polygon[candidateIndex], a, b, c)) {
          containsPoint = true;
          break;
        }
      }
      if (containsPoint) {
        continue;
      }

      appendPoint(a);
      appendPoint(b);
      appendPoint(c);
      indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(pos));
      clipped = true;
      break;
    }
    if (!clipped) {
      vertices.clear();
      return false;
    }
  }

  if (indices.size() == 3U) {
    appendPoint(polygon[indices[0]]);
    appendPoint(polygon[indices[1]]);
    appendPoint(polygon[indices[2]]);
  }

  return !vertices.empty();
}

void
drawBladeFill(const DrawSession& draw, const core::BladeContour& blade) noexcept
{
  if (blade.closedContour.size() < 4U) {
    return;
  }

  auto& vertices = draw.scratch();
  if (triangulateClosedContour(blade.closedContour, vertices)) {
    draw.setColor(kBladeFillColor);
    draw.setAlpha(0.34F);
    draw.drawVertices(vertices, GL_TRIANGLES);
    return;
  }

  if (blade.pressureSide.size() < 2U || blade.pressureSide.size() != blade.suctionSide.size()) {
    return;
  }

  vertices.clear();
  vertices.reserve((blade.pressureSide.size() - 1U) * 12U + 12U);

  const auto appendPoint = [&](const core::BladePlanPoint& point) {
    vertices.push_back(static_cast<float>(point.xMm));
    vertices.push_back(static_cast<float>(point.yMm));
  };
  const auto appendTri = [&](const core::BladePlanPoint& a,
                             const core::BladePlanPoint& b,
                             const core::BladePlanPoint& c) {
    appendPoint(a);
    appendPoint(b);
    appendPoint(c);
  };

  for (std::size_t i = 0; i + 1U < blade.pressureSide.size(); ++i) {
    const auto& p0 = blade.pressureSide[i];
    const auto& p1 = blade.pressureSide[i + 1U];
    const auto& s0 = blade.suctionSide[i];
    const auto& s1 = blade.suctionSide[i + 1U];
    appendTri(p0, s0, p1);
    appendTri(s0, s1, p1);
  }

  const auto inletCenter = core::BladePlanPoint{
    .xMm = 0.5 * (blade.pressureSide.front().xMm + blade.suctionSide.front().xMm),
    .yMm = 0.5 * (blade.pressureSide.front().yMm + blade.suctionSide.front().yMm),
  };
  appendTri(inletCenter, blade.suctionSide.front(), blade.pressureSide.front());

  const auto outletCenter = core::BladePlanPoint{
    .xMm = 0.5 * (blade.pressureSide.back().xMm + blade.suctionSide.back().xMm),
    .yMm = 0.5 * (blade.pressureSide.back().yMm + blade.suctionSide.back().yMm),
  };
  appendTri(outletCenter, blade.pressureSide.back(), blade.suctionSide.back());

  draw.setColor(kBladeFillColor);
  draw.setAlpha(0.34F);
  draw.drawVertices(vertices, GL_TRIANGLES);
}

void
drawBladeLine(const DrawSession& draw,
              const std::vector<core::BladePlanPoint>& points,
              GLenum mode,
              Rgb color,
              float lineWidth,
              float alpha) noexcept
{
  if (points.size() < 2U) {
    return;
  }
  auto& vertices = draw.scratch();
  appendContour(vertices, points);
  draw.setColor(color);
  draw.setAlpha(alpha);
  draw.setLineWidth(lineWidth);
  draw.drawVertices(vertices, mode);
}

void
drawBlades(const core::BladeDesignResults& results, const DrawSession& draw) noexcept
{
  for (const auto& blade : results.allBlades) {
    drawBladeFill(draw, blade);
  }

  for (const auto& blade : results.allBlades) {
    drawBladeLine(draw, blade.pressureSide, GL_LINE_STRIP, kSideLineColor, 1.8F, 0.95F);
    drawBladeLine(draw, blade.suctionSide, GL_LINE_STRIP, kSideLineColor, 1.8F, 0.95F);
    drawBladeLine(draw, blade.closedContour, GL_LINE_LOOP, kBladeOutlineColor, 2.0F, 1.0F);
    drawBladeLine(draw, blade.centerline, GL_LINE_STRIP, kCenterLineColor, 1.5F, 0.92F);
  }
}

}

BladePlanRenderer::~BladePlanRenderer()
{
  destroy();
}

BladePlanRenderer::BladePlanRenderer(BladePlanRenderer&& other) noexcept
  : vao_(std::exchange(other.vao_, 0U)),
    vbo_(std::exchange(other.vbo_, 0U)),
    shaderProgram_(std::exchange(other.shaderProgram_, 0U)),
    viewportUniformLocation_(std::exchange(other.viewportUniformLocation_, -1)),
    colorUniformLocation_(std::exchange(other.colorUniformLocation_, -1)),
    useVertexColorUniformLocation_(std::exchange(other.useVertexColorUniformLocation_, -1)),
    alphaUniformLocation_(std::exchange(other.alphaUniformLocation_, -1)),
    maxLineWidth_(std::exchange(other.maxLineWidth_, kDefaultLineWidth))
{
}

BladePlanRenderer&
BladePlanRenderer::operator=(BladePlanRenderer&& other) noexcept
{
  if (this != &other) {
    destroy();
    vao_ = std::exchange(other.vao_, 0U);
    vbo_ = std::exchange(other.vbo_, 0U);
    shaderProgram_ = std::exchange(other.shaderProgram_, 0U);
    viewportUniformLocation_ = std::exchange(other.viewportUniformLocation_, -1);
    colorUniformLocation_ = std::exchange(other.colorUniformLocation_, -1);
    useVertexColorUniformLocation_ = std::exchange(other.useVertexColorUniformLocation_, -1);
    alphaUniformLocation_ = std::exchange(other.alphaUniformLocation_, -1);
    maxLineWidth_ = std::exchange(other.maxLineWidth_, kDefaultLineWidth);
  }
  return *this;
}

bool
BladePlanRenderer::isReady() const noexcept
{
  return vao_ != 0U && vbo_ != 0U && shaderProgram_ != 0U && viewportUniformLocation_ >= 0 &&
         colorUniformLocation_ >= 0 && alphaUniformLocation_ >= 0;
}

void
BladePlanRenderer::initGl() noexcept
{
  destroy();

  const auto vertShader = compileShader(GL_VERTEX_SHADER, kVertexShaderSource);
  if (vertShader == 0U) {
    return;
  }
  const auto fragShader = compileShader(GL_FRAGMENT_SHADER, kFragmentShaderSource);
  if (fragShader == 0U) {
    glDeleteShader(vertShader);
    return;
  }

  const auto shaderProgram = glCreateProgram();
  if (shaderProgram == 0U) {
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    return;
  }

  glAttachShader(shaderProgram, vertShader);
  glAttachShader(shaderProgram, fragShader);
  glLinkProgram(shaderProgram);

  auto linkSuccess = GLint{};
  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkSuccess);
  glDeleteShader(vertShader);
  glDeleteShader(fragShader);
  if (linkSuccess == GL_FALSE) {
    glDeleteProgram(shaderProgram);
    return;
  }

  const auto viewportUniformLocation = glGetUniformLocation(shaderProgram, "uViewport");
  const auto colorUniformLocation = glGetUniformLocation(shaderProgram, "uColor");
  const auto alphaUniformLocation = glGetUniformLocation(shaderProgram, "uAlpha");
  if (viewportUniformLocation < 0 || colorUniformLocation < 0 || alphaUniformLocation < 0) {
    glDeleteProgram(shaderProgram);
    return;
  }

  auto vao = GLuint{};
  auto vbo = GLuint{};
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  if (vao == 0U || vbo == 0U) {
    if (vao != 0U) {
      glDeleteVertexArrays(1, &vao);
    }
    if (vbo != 0U) {
      glDeleteBuffers(1, &vbo);
    }
    glDeleteProgram(shaderProgram);
    return;
  }

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * static_cast<GLsizei>(sizeof(float)), nullptr);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  auto lineWidthRange = std::array<float, 2>{kDefaultLineWidth, kDefaultLineWidth};
  glGetFloatv(GL_SMOOTH_LINE_WIDTH_RANGE, lineWidthRange.data());

  vao_ = vao;
  vbo_ = vbo;
  shaderProgram_ = shaderProgram;
  viewportUniformLocation_ = viewportUniformLocation;
  colorUniformLocation_ = colorUniformLocation;
  alphaUniformLocation_ = alphaUniformLocation;
  maxLineWidth_ = std::max(lineWidthRange[1], kDefaultLineWidth);
}

BladePlanViewport
BladePlanRenderer::render(const core::BladeDesignResults& results,
                          int viewportWidth,
                          int viewportHeight) noexcept
{
  if (viewportWidth <= 0 || viewportHeight <= 0) {
    return {};
  }

  if (!isReady()) {
    initGl();
    if (!isReady()) {
      return {};
    }
  }

  configureFrame(viewportWidth, viewportHeight);
  const auto viewport = makeViewport(results, viewportWidth, viewportHeight);
  const DrawUniforms uniforms{
    .viewport = viewportUniformLocation_,
    .color = colorUniformLocation_,
    .alpha = alphaUniformLocation_,
  };
  auto draw = DrawSession{shaderProgram_, vao_, vbo_, uniforms, maxLineWidth_, scratchVertices_};
  draw.setViewport(viewport);

  drawGrid(viewport, draw);
  drawCircle(draw, results.outletRadiusMm, kOutletRingColor, 2.4F, 0.95F);
  drawCircle(draw, results.inletRadiusMm, kInletRingColor, 2.2F, 0.95F);
  drawCircle(draw, results.outletRadiusMm * 0.985, kOutletRingColor, 1.0F, 0.30F);
  drawBlades(results, draw);

  draw.setAlpha(1.0F);
  return viewport.toMap(viewportWidth, viewportHeight);
}

void
BladePlanRenderer::destroy() noexcept
{
  if (vao_ != 0U) {
    glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
  }
  if (vbo_ != 0U) {
    glDeleteBuffers(1, &vbo_);
    vbo_ = 0;
  }
  if (shaderProgram_ != 0U) {
    glDeleteProgram(shaderProgram_);
    shaderProgram_ = 0;
  }
  viewportUniformLocation_ = -1;
  colorUniformLocation_ = -1;
  useVertexColorUniformLocation_ = -1;
  alphaUniformLocation_ = -1;
  maxLineWidth_ = kDefaultLineWidth;
}

}
