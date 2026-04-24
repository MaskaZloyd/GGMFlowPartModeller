#include "gui/renderer/geometry_renderer.hpp"

#include "gl_headers.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace ggm::gui {

namespace {

using PointSpan = std::span<const math::Vec2>;
using VertexSpan = std::span<const float>;

constexpr std::string_view kVertexShaderSource = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec3 aColor;
uniform vec4 uViewport;      // (minZ, minR, rangeZ, rangeR)
uniform int  uUseVertexColor; // 0 = use uColor, 1 = use aColor
out vec3 vColor;
void main() {
    float ndcX = 2.0 * (aPos.x - uViewport.x) / uViewport.z - 1.0;
    float ndcY = 2.0 * (aPos.y - uViewport.y) / uViewport.w - 1.0;
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    vColor = aColor;
    // Silence warnings when not used.
    if (uUseVertexColor == 0) { vColor = vec3(0.0); }
}
)glsl";

constexpr std::string_view kFragmentShaderSource = R"glsl(
#version 330 core
in  vec3 vColor;
uniform vec3 uColor;
uniform int  uUseVertexColor;
uniform float uAlpha;
out vec4 FragColor;
void main() {
    vec3 c = (uUseVertexColor == 1) ? vColor : uColor;
    FragColor = vec4(c, uAlpha);
}
)glsl";

constexpr float kDefaultLineWidth = 1.0F;
constexpr double kViewportPaddingFraction = 0.1;
constexpr int kTargetGridLineCount = 8;
constexpr double kMinimumExtent = 1.0e-3;

struct Rgb
{
  float r;
  float g;
  float b;
};

constexpr auto kMinorGridColor = Rgb{0.900F, 0.905F, 0.915F};
constexpr auto kMajorGridColor = Rgb{0.780F, 0.790F, 0.810F};
constexpr auto kComputationalGridColor = Rgb{0.550F, 0.620F, 0.710F};
constexpr auto kMeanLineColor = Rgb{0.08F, 0.09F, 0.12F};
constexpr auto kHubColor = Rgb{0.780F, 0.200F, 0.180F};
constexpr auto kShroudColor = Rgb{0.180F, 0.360F, 0.780F};

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

[[nodiscard]] Rgb
viridisColor(float t) noexcept
{
  t = std::clamp(t, 0.0F, 1.0F);
  static constexpr auto kStops = std::to_array<Rgb>({
    {0.267F, 0.005F, 0.329F}, // 0.000
    {0.275F, 0.125F, 0.474F}, // 0.125
    {0.230F, 0.318F, 0.545F}, // 0.250
    {0.173F, 0.449F, 0.558F}, // 0.375
    {0.128F, 0.567F, 0.551F}, // 0.500
    {0.135F, 0.659F, 0.518F}, // 0.625
    {0.267F, 0.749F, 0.440F}, // 0.750
    {0.526F, 0.830F, 0.288F}, // 0.875
    {0.993F, 0.906F, 0.144F}, // 1.000
  });

  const auto scaled = t * static_cast<float>(kStops.size() - 1U);
  const auto lowerIndex = std::min(static_cast<std::size_t>(scaled), kStops.size() - 2U);
  const auto fraction = scaled - static_cast<float>(lowerIndex);
  const auto& a = kStops[lowerIndex];
  const auto& b = kStops[lowerIndex + 1U];

  return {a.r + fraction * (b.r - a.r), a.g + fraction * (b.g - a.g), a.b + fraction * (b.b - a.b)};
}

struct BoundingBox
{
  double minZ = std::numeric_limits<double>::max();
  double maxZ = std::numeric_limits<double>::lowest();
  double minR = std::numeric_limits<double>::max();
  double maxR = std::numeric_limits<double>::lowest();

  void expand(const PointSpan points) noexcept
  {
    for (const auto& point : points) {
      minZ = std::min(minZ, point.x());
      maxZ = std::max(maxZ, point.x());
      minR = std::min(minR, point.y());
      maxR = std::max(maxR, point.y());
    }
  }

  [[nodiscard]] bool hasData() const noexcept { return minZ <= maxZ && minR <= maxR; }

  void addPadding(const double fraction) noexcept
  {
    const auto rangeZ = maxZ - minZ;
    const auto rangeR = maxR - minR;
    const auto padZ = rangeZ * fraction;
    const auto padR = rangeR * fraction;
    minZ -= padZ;
    maxZ += padZ;
    minR -= padR;
    maxR += padR;
  }

  void ensureNonDegenerate() noexcept
  {
    ensureAxis(minZ, maxZ);
    ensureAxis(minR, maxR);
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
  BoundingBox bounds;
  double rangeZ = 0.0;
  double rangeR = 0.0;

  [[nodiscard]] ViewportMap toMap(const int viewportWidth, const int viewportHeight) const noexcept
  {
    return ViewportMap{
      .minZ = bounds.minZ,
      .maxZ = bounds.maxZ,
      .minR = bounds.minR,
      .maxR = bounds.maxR,
      .widthPx = viewportWidth,
      .heightPx = viewportHeight,
    };
  }
};

struct DrawUniforms
{
  GLint viewport = -1;
  GLint color = -1;
  GLint useVertexColor = -1;
  GLint alpha = -1;
};

class DrawSession
{
public:
  DrawSession(const GLuint shaderProgram,
              const GLuint vao,
              const GLuint vbo,
              const DrawUniforms uniforms,
              const float maxLineWidth,
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
    setUseVertexColor(false);
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
  DrawSession(DrawSession&&) = delete;
  DrawSession& operator=(DrawSession&&) = delete;

  void setViewport(const RenderViewport& viewport) const noexcept
  {
    glUniform4f(uniforms_.viewport,
                static_cast<float>(viewport.bounds.minZ),
                static_cast<float>(viewport.bounds.minR),
                static_cast<float>(viewport.rangeZ),
                static_cast<float>(viewport.rangeR));
  }

  void setColor(const Rgb color) const noexcept
  {
    glUniform3f(uniforms_.color, color.r, color.g, color.b);
  }

  void setAlpha(const float alpha) const noexcept { glUniform1f(uniforms_.alpha, alpha); }

  void setUseVertexColor(const bool enabled) const noexcept
  {
    glUniform1i(uniforms_.useVertexColor, enabled ? 1 : 0);
    if (enabled) {
      // Interleaved layout: 2 floats pos + 3 floats color.
      glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, 5 * static_cast<GLsizei>(sizeof(float)), nullptr);
      glVertexAttribPointer(1,
                            3,
                            GL_FLOAT,
                            GL_FALSE,
                            5 * static_cast<GLsizei>(sizeof(float)),
                            reinterpret_cast<const void*>(2 * sizeof(float)));
      glEnableVertexAttribArray(1);
    } else {
      // Position-only layout: 2 floats per vertex.
      glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, 2 * static_cast<GLsizei>(sizeof(float)), nullptr);
      glDisableVertexAttribArray(1);
    }
  }

  void setLineWidth(const float width) const noexcept
  {
    glLineWidth(std::clamp(width, kDefaultLineWidth, maxLineWidth_));
  }

  void drawPolyline(const PointSpan points, const GLenum mode = GL_LINE_STRIP) const noexcept
  {
    if (points.empty()) {
      return;
    }

    scratchVertices_.clear();
    scratchVertices_.reserve(points.size() * 2U);
    for (const auto& point : points) {
      scratchVertices_.push_back(static_cast<float>(point.x()));
      scratchVertices_.push_back(static_cast<float>(point.y()));
    }

    drawVertices(scratchVertices_, mode, 2);
  }

  // Draws from interleaved [pos_x, pos_y, r, g, b] vertices. `mode` =
  // GL_LINE_STRIP / GL_TRIANGLES / etc. Caller must have enabled vertex
  // color via setUseVertexColor(true).
  void drawColoredVertices(const VertexSpan vertices, const GLenum mode) const noexcept
  {
    drawVertices(vertices, mode, 5);
  }

  void drawVertices(const VertexSpan vertices,
                    const GLenum mode,
                    const std::size_t floatsPerVertex = 2U) const noexcept
  {
    if (vertices.empty() || floatsPerVertex == 0U) {
      return;
    }

    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size_bytes()),
                 vertices.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(mode, 0, static_cast<GLsizei>(vertices.size() / floatsPerVertex));
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
niceGridStep(const double range, const int targetLines) noexcept
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
configureFrame(const int viewportWidth, const int viewportHeight) noexcept
{
  glViewport(0, 0, viewportWidth, viewportHeight);
  glClearColor(0.988F, 0.990F, 0.994F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);

  glEnable(GL_MULTISAMPLE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

[[nodiscard]] RenderViewport
makeViewport(const core::MeridionalGeometry& geom,
             const int viewportWidth,
             const int viewportHeight) noexcept
{
  auto bounds = BoundingBox{};
  bounds.expand(geom.hubCurve);
  bounds.expand(geom.shroudCurve);
  bounds.addPadding(kViewportPaddingFraction);
  bounds.ensureNonDegenerate();

  auto rangeZ = bounds.maxZ - bounds.minZ;
  auto rangeR = bounds.maxR - bounds.minR;

  const auto aspectView = static_cast<double>(viewportWidth) / static_cast<double>(viewportHeight);
  const auto aspectData = rangeZ / rangeR;

  if (aspectData > aspectView) {
    const auto newRangeR = rangeZ / aspectView;
    const auto centerR = std::midpoint(bounds.minR, bounds.maxR);
    bounds.minR = centerR - (newRangeR / 2.0);
    bounds.maxR = centerR + (newRangeR / 2.0);
    rangeR = newRangeR;
  } else {
    const auto newRangeZ = rangeR * aspectView;
    const auto centerZ = std::midpoint(bounds.minZ, bounds.maxZ);
    bounds.minZ = centerZ - (newRangeZ / 2.0);
    bounds.maxZ = centerZ + (newRangeZ / 2.0);
    rangeZ = newRangeZ;
  }

  return RenderViewport{
    .bounds = bounds,
    .rangeZ = rangeZ,
    .rangeR = rangeR,
  };
}

void
appendLine(std::vector<float>& vertices,
           const double x0,
           const double y0,
           const double x1,
           const double y1)
{
  vertices.push_back(static_cast<float>(x0));
  vertices.push_back(static_cast<float>(y0));
  vertices.push_back(static_cast<float>(x1));
  vertices.push_back(static_cast<float>(y1));
}

void
fillGridVertices(std::vector<float>& vertices, const BoundingBox& bounds, double gridStep)
{
  vertices.clear();
  if (!bounds.hasData() || gridStep <= 0.0) {
    return;
  }

  const auto minZ = std::floor(bounds.minZ / gridStep) * gridStep;
  const auto maxZ = std::ceil(bounds.maxZ / gridStep) * gridStep;
  const auto minR = std::floor(bounds.minR / gridStep) * gridStep;
  const auto maxR = std::ceil(bounds.maxR / gridStep) * gridStep;

  for (auto zValue = minZ; zValue <= (maxZ + (gridStep * 0.5)); zValue += gridStep) {
    appendLine(vertices, zValue, minR, zValue, maxR);
  }
  for (auto rValue = minR; rValue <= (maxR + (gridStep * 0.5)); rValue += gridStep) {
    appendLine(vertices, minZ, rValue, maxZ, rValue);
  }
}

void
drawCoordinateGrid(const RenderViewport& viewport, const DrawSession& drawSession) noexcept
{
  const auto majorStep =
    niceGridStep(std::max(viewport.rangeZ, viewport.rangeR), kTargetGridLineCount);
  const auto minorStep = majorStep / 5.0;

  auto& vertices = drawSession.scratch();

  drawSession.setLineWidth(kDefaultLineWidth);
  drawSession.setColor(kMinorGridColor);
  fillGridVertices(vertices, viewport.bounds, minorStep);
  drawSession.drawVertices(vertices, GL_LINES);

  drawSession.setLineWidth(1.2F);
  drawSession.setColor(kMajorGridColor);
  fillGridVertices(vertices, viewport.bounds, majorStep);
  drawSession.drawVertices(vertices, GL_LINES);
}

void
drawComputationalGrid(const core::FlowResults& flow, const DrawSession& drawSession) noexcept
{
  const auto& grid = flow.solution.grid;
  if (grid.nh <= 0 || grid.m <= 0) {
    return;
  }

  const auto rowCount = static_cast<std::size_t>(grid.nh);
  const auto columnCount = static_cast<std::size_t>(grid.m);
  const auto requiredNodeCount = rowCount * columnCount;
  if (grid.nodes.size() < requiredNodeCount) {
    return;
  }

  auto vertices = std::vector<float>{};
  vertices.reserve(rowCount * 4U);

  for (auto row = std::size_t{0}; row < rowCount; ++row) {
    const auto hubIndex = row * columnCount;
    const auto shroudIndex = hubIndex + columnCount - 1U;
    appendLine(vertices,
               grid.nodes[hubIndex].x(),
               grid.nodes[hubIndex].y(),
               grid.nodes[shroudIndex].x(),
               grid.nodes[shroudIndex].y());
  }

  drawSession.setLineWidth(kDefaultLineWidth);
  drawSession.setColor(kComputationalGridColor);
  drawSession.drawVertices(vertices, GL_LINES);
}

[[nodiscard]] double
maxStreamlineSpeed(const core::FlowResults& flow) noexcept
{
  double peak = 0.0;
  for (const auto& vel : flow.velocities) {
    for (const auto& sample : vel.samples) {
      peak = std::max(peak, sample.speed);
    }
  }
  return peak > 0.0 ? peak : 1.0;
}

// Append [x, y, r, g, b] to `out`.
inline void
appendColoredVertex(std::vector<float>& out, double x, double y, Rgb color)
{
  out.push_back(static_cast<float>(x));
  out.push_back(static_cast<float>(y));
  out.push_back(color.r);
  out.push_back(color.g);
  out.push_back(color.b);
}

// Renders the full FEM channel as a filled triangle mesh (grid.triangles),
// each node tinted by the |V| of its nearest streamline sample. This fills
// the entire region between hub and shroud — including the narrow bands
// the per-streamline strips used to miss.
void
drawVelocityHeatmap(const core::FlowResults& flow,
                    double peakSpeed,
                    const DrawSession& drawSession) noexcept
{
  const auto& grid = flow.solution.grid;
  if (grid.nodes.empty() || grid.triangles.empty() || flow.velocities.empty() ||
      peakSpeed <= 0.0) {
    return;
  }

  // Per-node speed via nearest streamline sample. O(N·M) per frame; for
  // the current grid sizes (nh·m ≈ 7k nodes × ≈500 samples ≈ 3.5M distance
  // checks) this is a few ms on a modern CPU — acceptable on the UI thread.
  std::vector<float> nodeSpeed(grid.nodes.size(), 0.0F);
  for (std::size_t n = 0; n < grid.nodes.size(); ++n) {
    const auto& node = grid.nodes[n];
    double bestSq = std::numeric_limits<double>::max();
    double bestSpeed = 0.0;
    for (const auto& vel : flow.velocities) {
      for (const auto& sample : vel.samples) {
        const double dz = sample.point.x() - node.x();
        const double dr = sample.point.y() - node.y();
        const double d2 = (dz * dz) + (dr * dr);
        if (d2 < bestSq) {
          bestSq = d2;
          bestSpeed = sample.speed;
        }
      }
    }
    nodeSpeed[n] = static_cast<float>(bestSpeed);
  }

  drawSession.setUseVertexColor(true);
  drawSession.setAlpha(0.55F);

  auto& vertices = drawSession.scratch();
  vertices.clear();
  vertices.reserve(grid.triangles.size() * 3U * 5U);

  for (const auto& tri : grid.triangles) {
    for (int corner = 0; corner < 3; ++corner) {
      const int idx = tri[static_cast<std::size_t>(corner)];
      if (idx < 0 || static_cast<std::size_t>(idx) >= grid.nodes.size()) {
        continue;
      }
      const auto& p = grid.nodes[static_cast<std::size_t>(idx)];
      const float t = std::clamp(
        nodeSpeed[static_cast<std::size_t>(idx)] / static_cast<float>(peakSpeed), 0.0F, 1.0F);
      appendColoredVertex(vertices, p.x(), p.y(), viridisColor(t));
    }
  }

  drawSession.drawColoredVertices(vertices, GL_TRIANGLES);

  drawSession.setAlpha(1.0F);
  drawSession.setUseVertexColor(false);
}

void
drawStreamlines(const core::FlowResults& flow,
                const RenderSettings& settings,
                const DrawSession& drawSession,
                double peakSpeed) noexcept
{
  drawSession.setLineWidth(settings.streamlineWidth + 0.4F);

  if (settings.colorStreamlinesBySpeed && !flow.velocities.empty()) {
    drawSession.setUseVertexColor(true);

    auto& vertices = drawSession.scratch();
    for (const auto& vel : flow.velocities) {
      if (vel.samples.size() < 2U) {
        continue;
      }
      vertices.clear();
      vertices.reserve(vel.samples.size() * 5U);
      for (const auto& sample : vel.samples) {
        const auto t = static_cast<float>(std::clamp(sample.speed / peakSpeed, 0.0, 1.0));
        appendColoredVertex(vertices, sample.point.x(), sample.point.y(), viridisColor(t));
      }
      drawSession.drawColoredVertices(vertices, GL_LINE_STRIP);
    }

    drawSession.setUseVertexColor(false);
    return;
  }

  for (const auto& streamline : flow.streamlines) {
    if (streamline.points.size() < 2U) {
      continue;
    }
    drawSession.setColor(viridisColor(static_cast<float>(streamline.psiLevel)));
    drawSession.drawPolyline(streamline.points);
  }
}

void
drawMeanLine(const core::FlowResults& flow,
             const RenderSettings& settings,
             const DrawSession& drawSession) noexcept
{
  const auto& midPoints = flow.areaProfile.midPoints;
  if (midPoints.size() < 2U) {
    return;
  }

  constexpr int kDashOn = 5;
  constexpr int kDashPeriod = 8;

  drawSession.setLineWidth(settings.meanLineWidth);
  drawSession.setColor(kMeanLineColor);

  auto dashSegment = std::vector<math::Vec2>{};
  dashSegment.reserve(static_cast<std::size_t>(kDashOn));

  for (auto index = std::size_t{0}; index < midPoints.size(); ++index) {
    const auto phase = static_cast<int>(index % static_cast<std::size_t>(kDashPeriod));
    if (phase < kDashOn) {
      dashSegment.push_back(midPoints[index]);
      continue;
    }

    if (dashSegment.size() >= 2U) {
      drawSession.drawPolyline(dashSegment);
    }
    dashSegment.clear();
  }

  if (dashSegment.size() >= 2U) {
    drawSession.drawPolyline(dashSegment);
  }
}

void
drawGeometryCurves(const core::MeridionalGeometry& geom,
                   const RenderSettings& settings,
                   const DrawSession& drawSession) noexcept
{
  drawSession.setLineWidth(settings.hubLineWidth);
  drawSession.setColor(kHubColor);
  drawSession.drawPolyline(geom.hubCurve);

  drawSession.setLineWidth(settings.shroudLineWidth);
  drawSession.setColor(kShroudColor);
  drawSession.drawPolyline(geom.shroudCurve);
}

} // namespace

GeometryRenderer::~GeometryRenderer()
{
  destroy();
}

GeometryRenderer::GeometryRenderer(GeometryRenderer&& other) noexcept
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

GeometryRenderer&
GeometryRenderer::operator=(GeometryRenderer&& other) noexcept
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
GeometryRenderer::isReady() const noexcept
{
  return vao_ != 0U && vbo_ != 0U && shaderProgram_ != 0U && viewportUniformLocation_ >= 0 &&
         colorUniformLocation_ >= 0 && useVertexColorUniformLocation_ >= 0 &&
         alphaUniformLocation_ >= 0;
}

void
GeometryRenderer::initGl() noexcept
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
  const auto useVertexColorUniformLocation =
    glGetUniformLocation(shaderProgram, "uUseVertexColor");
  const auto alphaUniformLocation = glGetUniformLocation(shaderProgram, "uAlpha");
  if (viewportUniformLocation < 0 || colorUniformLocation < 0 ||
      useVertexColorUniformLocation < 0 || alphaUniformLocation < 0) {
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
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(2 * sizeof(float)), nullptr);
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
  useVertexColorUniformLocation_ = useVertexColorUniformLocation;
  alphaUniformLocation_ = alphaUniformLocation;
  maxLineWidth_ = std::max(lineWidthRange[1], kDefaultLineWidth);
}

ViewportMap
GeometryRenderer::render(const core::MeridionalGeometry& geom,
                         const core::FlowResults* flow,
                         const RenderSettings& settings,
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

  if (geom.hubCurve.empty() && geom.shroudCurve.empty()) {
    return {};
  }

  const auto viewport = makeViewport(geom, viewportWidth, viewportHeight);
  const DrawUniforms uniforms{
    .viewport = viewportUniformLocation_,
    .color = colorUniformLocation_,
    .useVertexColor = useVertexColorUniformLocation_,
    .alpha = alphaUniformLocation_,
  };
  auto drawSession = DrawSession{shaderProgram_, vao_, vbo_, uniforms, maxLineWidth_, scratchVertices_};
  drawSession.setViewport(viewport);

  if (settings.showCoordGrid) {
    drawCoordinateGrid(viewport, drawSession);
  }

  if (settings.showComputationalGrid && flow != nullptr) {
    drawComputationalGrid(*flow, drawSession);
  }

  if (flow != nullptr) {
    const double peakSpeed = maxStreamlineSpeed(*flow);
    if (settings.showVelocityHeatmap) {
      drawVelocityHeatmap(*flow, peakSpeed, drawSession);
    }
    drawStreamlines(*flow, settings, drawSession, peakSpeed);
    drawMeanLine(*flow, settings, drawSession);
  }

  drawGeometryCurves(geom, settings, drawSession);

  return viewport.toMap(viewportWidth, viewportHeight);
}

void
GeometryRenderer::destroy() noexcept
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

} // namespace ggm::gui
