#include "gui/renderer/geometry_renderer.hpp"

#include "gl_headers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace ggm::gui {

namespace {

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
constexpr const char* VERTEX_SHADER_SRC = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec4 uViewport;  // (minZ, minR, rangeZ, rangeR)
void main() {
    float ndcX = 2.0 * (aPos.x - uViewport.x) / uViewport.z - 1.0;
    float ndcY = 2.0 * (aPos.y - uViewport.y) / uViewport.w - 1.0;
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
}
)glsl";

constexpr const char* FRAGMENT_SHADER_SRC = R"glsl(
#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)glsl";
// NOLINTEND(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)

unsigned int compileShader(unsigned int type, const char* source) noexcept {
  unsigned int shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);

  int success = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (success == 0) {
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

// Sample the viridis colormap at t in [0, 1]. Piecewise linear approximation
// through the canonical 9 stops — good enough for ~5-20 streamlines.
struct Rgb {
  float r;
  float g;
  float b;
};

Rgb viridisColor(float t) noexcept {
  t = std::clamp(t, 0.0F, 1.0F);
  static constexpr Rgb STOPS[] = {
      {0.267F, 0.005F, 0.329F}, // 0.000
      {0.275F, 0.125F, 0.474F}, // 0.125
      {0.230F, 0.318F, 0.545F}, // 0.250
      {0.173F, 0.449F, 0.558F}, // 0.375
      {0.128F, 0.567F, 0.551F}, // 0.500
      {0.135F, 0.659F, 0.518F}, // 0.625
      {0.267F, 0.749F, 0.440F}, // 0.750
      {0.526F, 0.830F, 0.288F}, // 0.875
      {0.993F, 0.906F, 0.144F}, // 1.000
  };
  static constexpr int N = static_cast<int>(sizeof(STOPS) / sizeof(STOPS[0]));
  float scaled = t * static_cast<float>(N - 1);
  int i0 = std::min(static_cast<int>(scaled), N - 2);
  float f = scaled - static_cast<float>(i0);
  const auto& a = STOPS[i0];
  const auto& b = STOPS[i0 + 1];
  return {a.r + f * (b.r - a.r), a.g + f * (b.g - a.g), a.b + f * (b.b - a.b)};
}

struct BoundingBox {
  double minZ = std::numeric_limits<double>::max();
  double maxZ = std::numeric_limits<double>::lowest();
  double minR = std::numeric_limits<double>::max();
  double maxR = std::numeric_limits<double>::lowest();

  void expand(const std::vector<math::Vec2>& points) {
    for (const auto& point : points) {
      minZ = std::min(minZ, point.x());
      maxZ = std::max(maxZ, point.x());
      minR = std::min(minR, point.y());
      maxR = std::max(maxR, point.y());
    }
  }

  void addPadding(double fraction) {
    double rangeZ = maxZ - minZ;
    double rangeR = maxR - minR;
    double padZ = rangeZ * fraction;
    double padR = rangeR * fraction;
    minZ -= padZ;
    maxZ += padZ;
    minR -= padR;
    maxR += padR;
  }
};

// Compute a "nice" grid step: 1, 2, 5, 10, 20, 50, 100, ...
double niceGridStep(double range, int targetLines) noexcept {
  double rough = range / static_cast<double>(targetLines);
  double mag = std::pow(10.0, std::floor(std::log10(rough)));
  double normalized = rough / mag;
  double nice = 1.0;
  if (normalized >= 5.0) {
    nice = 5.0;
  } else if (normalized >= 2.0) {
    nice = 2.0;
  }
  return nice * mag;
}

} // namespace

GeometryRenderer::~GeometryRenderer() {
  destroy();
}

GeometryRenderer::GeometryRenderer(GeometryRenderer&& other) noexcept
    : vao_(other.vao_)
    , vbo_(other.vbo_)
    , shaderProgram_(other.shaderProgram_)
    , initialized_(other.initialized_)
    , maxLineWidth_(other.maxLineWidth_) {
  other.vao_ = 0;
  other.vbo_ = 0;
  other.shaderProgram_ = 0;
  other.initialized_ = false;
}

GeometryRenderer& GeometryRenderer::operator=(GeometryRenderer&& other) noexcept {
  if (this != &other) {
    destroy();
    vao_ = other.vao_;
    vbo_ = other.vbo_;
    shaderProgram_ = other.shaderProgram_;
    initialized_ = other.initialized_;
    maxLineWidth_ = other.maxLineWidth_;
    other.vao_ = 0;
    other.vbo_ = 0;
    other.shaderProgram_ = 0;
    other.initialized_ = false;
  }
  return *this;
}

void GeometryRenderer::initGl() noexcept {
  unsigned int vertShader = compileShader(GL_VERTEX_SHADER, VERTEX_SHADER_SRC);
  unsigned int fragShader = compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SRC);

  shaderProgram_ = glCreateProgram();
  if (vertShader != 0) {
    glAttachShader(shaderProgram_, vertShader);
  }
  if (fragShader != 0) {
    glAttachShader(shaderProgram_, fragShader);
  }
  glLinkProgram(shaderProgram_);

  int linkSuccess = 0;
  glGetProgramiv(shaderProgram_, GL_LINK_STATUS, &linkSuccess);

  if (vertShader != 0) {
    glDeleteShader(vertShader);
  }
  if (fragShader != 0) {
    glDeleteShader(fragShader);
  }

  if (linkSuccess == 0) {
    glDeleteProgram(shaderProgram_);
    shaderProgram_ = 0;
    return;
  }

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);

  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
  glEnableVertexAttribArray(0);
  glBindVertexArray(0);

  // Query max supported line width
  float lineWidthRange[2] = {1.0F, 1.0F}; // NOLINT
  glGetFloatv(GL_SMOOTH_LINE_WIDTH_RANGE, lineWidthRange);
  maxLineWidth_ = lineWidthRange[1];

  initialized_ = true;
}

ViewportMap GeometryRenderer::render(const core::MeridionalGeometry& geom,
                                     const core::FlowResults* flow,
                                     const RenderSettings& settings,
                                     int viewportWidth,
                                     int viewportHeight) noexcept {
  if (!initialized_) {
    initGl();
  }

  glViewport(0, 0, viewportWidth, viewportHeight);
  glClearColor(0.988F, 0.990F, 0.994F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);

  // Smooth lines/edges (the window is created with MSAA samples=4).
  glEnable(GL_MULTISAMPLE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  if (geom.hubCurve.empty() && geom.shroudCurve.empty()) {
    return {};
  }

  // Compute bounding box with 10% padding
  BoundingBox bbox;
  bbox.expand(geom.hubCurve);
  bbox.expand(geom.shroudCurve);
  bbox.addPadding(0.1);

  // Maintain aspect ratio
  double rangeZ = bbox.maxZ - bbox.minZ;
  double rangeR = bbox.maxR - bbox.minR;
  double aspectView = static_cast<double>(viewportWidth) / static_cast<double>(viewportHeight);
  double aspectData = rangeZ / rangeR;

  if (aspectData > aspectView) {
    double newRangeR = rangeZ / aspectView;
    double centerR = (bbox.minR + bbox.maxR) / 2.0;
    bbox.minR = centerR - (newRangeR / 2.0);
    bbox.maxR = centerR + (newRangeR / 2.0);
    rangeR = newRangeR;
  } else {
    double newRangeZ = rangeR * aspectView;
    double centerZ = (bbox.minZ + bbox.maxZ) / 2.0;
    bbox.minZ = centerZ - (newRangeZ / 2.0);
    bbox.maxZ = centerZ + (newRangeZ / 2.0);
    rangeZ = newRangeZ;
  }

  glUseProgram(shaderProgram_);
  int viewportLoc = glGetUniformLocation(shaderProgram_, "uViewport");
  int colorLoc = glGetUniformLocation(shaderProgram_, "uColor");
  glUniform4f(viewportLoc, static_cast<float>(bbox.minZ), static_cast<float>(bbox.minR),
              static_cast<float>(rangeZ), static_cast<float>(rangeR));

  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);

  auto uploadAndDraw = [&](const std::vector<math::Vec2>& curve, unsigned int mode) {
    std::vector<float> vertices;
    vertices.reserve(curve.size() * 2);
    for (const auto& point : curve) {
      vertices.push_back(static_cast<float>(point.x()));
      vertices.push_back(static_cast<float>(point.y()));
    }
    glBufferData(GL_ARRAY_BUFFER, static_cast<long>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(mode, 0, static_cast<int>(curve.size()));
  };

  auto setLineWidth = [&](float width) {
    glLineWidth(std::clamp(width, 1.0F, maxLineWidth_));
  };

  // --- Coordinate grid ---
  if (settings.showCoordGrid) {
    double majorStep = niceGridStep(std::max(rangeZ, rangeR), 8);
    double minorStep = majorStep / 5.0;

    std::vector<float> gridVerts;
    auto addLine = [&](double x0, double y0, double x1, double y1) {
      gridVerts.push_back(static_cast<float>(x0));
      gridVerts.push_back(static_cast<float>(y0));
      gridVerts.push_back(static_cast<float>(x1));
      gridVerts.push_back(static_cast<float>(y1));
    };

    // Minor grid — subtle light-gray on white.
    setLineWidth(1.0F);
    glUniform3f(colorLoc, 0.900F, 0.905F, 0.915F);
    gridVerts.clear();
    double startZ = std::floor(bbox.minZ / minorStep) * minorStep;
    double startR = std::floor(bbox.minR / minorStep) * minorStep;
    for (double zVal = startZ; zVal <= bbox.maxZ; zVal += minorStep) {
      addLine(zVal, bbox.minR, zVal, bbox.maxR);
    }
    for (double rVal = startR; rVal <= bbox.maxR; rVal += minorStep) {
      addLine(bbox.minZ, rVal, bbox.maxZ, rVal);
    }
    if (!gridVerts.empty()) {
      glBufferData(GL_ARRAY_BUFFER, static_cast<long>(gridVerts.size() * sizeof(float)),
                   gridVerts.data(), GL_DYNAMIC_DRAW);
      glDrawArrays(GL_LINES, 0, static_cast<int>(gridVerts.size() / 2));
    }

    // Major grid — slightly darker divisions.
    setLineWidth(1.2F);
    glUniform3f(colorLoc, 0.780F, 0.790F, 0.810F);
    gridVerts.clear();
    startZ = std::floor(bbox.minZ / majorStep) * majorStep;
    startR = std::floor(bbox.minR / majorStep) * majorStep;
    for (double zVal = startZ; zVal <= bbox.maxZ; zVal += majorStep) {
      addLine(zVal, bbox.minR, zVal, bbox.maxR);
    }
    for (double rVal = startR; rVal <= bbox.maxR; rVal += majorStep) {
      addLine(bbox.minZ, rVal, bbox.maxZ, rVal);
    }
    if (!gridVerts.empty()) {
      glBufferData(GL_ARRAY_BUFFER, static_cast<long>(gridVerts.size() * sizeof(float)),
                   gridVerts.data(), GL_DYNAMIC_DRAW);
      glDrawArrays(GL_LINES, 0, static_cast<int>(gridVerts.size() / 2));
    }
  }

  // --- Computational grid ---
  if (settings.showComputationalGrid && flow != nullptr) {
    setLineWidth(1.0F);
    glUniform3f(colorLoc, 0.550F, 0.620F, 0.710F);
    const auto& grid = flow->solution.grid;

    // Draw transverse chords (hub[i] to shroud[i])
    std::vector<float> compGridVerts;
    for (int row = 0; row < grid.nh; ++row) {
      auto hubIdx = static_cast<std::size_t>(row * grid.m);
      auto shrIdx = static_cast<std::size_t>(row * grid.m + grid.m - 1);
      compGridVerts.push_back(static_cast<float>(grid.nodes[hubIdx].x()));
      compGridVerts.push_back(static_cast<float>(grid.nodes[hubIdx].y()));
      compGridVerts.push_back(static_cast<float>(grid.nodes[shrIdx].x()));
      compGridVerts.push_back(static_cast<float>(grid.nodes[shrIdx].y()));
    }
    if (!compGridVerts.empty()) {
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<long>(compGridVerts.size() * sizeof(float)),
                   compGridVerts.data(), GL_DYNAMIC_DRAW);
      glDrawArrays(GL_LINES, 0, static_cast<int>(compGridVerts.size() / 2));
    }
  }

  // --- Streamlines — coloured by ψ via viridis ---
  if (flow != nullptr) {
    setLineWidth(settings.streamlineWidth + 0.4F);
    for (const auto& streamline : flow->streamlines) {
      if (streamline.points.empty()) {
        continue;
      }
      auto colour = viridisColor(static_cast<float>(streamline.psiLevel));
      glUniform3f(colorLoc, colour.r, colour.g, colour.b);
      uploadAndDraw(streamline.points, GL_LINE_STRIP);
    }
  }

  // --- Mean line (equidistant) ---
  if (flow != nullptr && !flow->areaProfile.midPoints.empty()) {
    setLineWidth(settings.meanLineWidth);
    glUniform3f(colorLoc, 0.30F, 0.62F, 0.30F);

    // Dashed: draw every other segment group
    const auto& mid = flow->areaProfile.midPoints;
    constexpr int DASH_ON = 5;
    constexpr int DASH_PERIOD = 8;
    std::vector<math::Vec2> dashSegment;
    for (std::size_t idx = 0; idx < mid.size(); ++idx) {
      int phase = static_cast<int>(idx % DASH_PERIOD);
      if (phase < DASH_ON) {
        dashSegment.push_back(mid[idx]);
      } else {
        if (dashSegment.size() >= 2) {
          uploadAndDraw(dashSegment, GL_LINE_STRIP);
        }
        dashSegment.clear();
      }
    }
    if (dashSegment.size() >= 2) {
      uploadAndDraw(dashSegment, GL_LINE_STRIP);
    }
  }

  // --- Hub — vivid crimson, thick ---
  setLineWidth(settings.hubLineWidth);
  glUniform3f(colorLoc, 0.780F, 0.200F, 0.180F);
  uploadAndDraw(geom.hubCurve, GL_LINE_STRIP);

  // --- Shroud — deep indigo, thick ---
  setLineWidth(settings.shroudLineWidth);
  glUniform3f(colorLoc, 0.180F, 0.360F, 0.780F);
  uploadAndDraw(geom.shroudCurve, GL_LINE_STRIP);

  setLineWidth(1.0F);
  glBindVertexArray(0);
  glUseProgram(0);

  return ViewportMap{.minZ = bbox.minZ,
                     .maxZ = bbox.maxZ,
                     .minR = bbox.minR,
                     .maxR = bbox.maxR,
                     .widthPx = viewportWidth,
                     .heightPx = viewportHeight};
}

void GeometryRenderer::destroy() noexcept {
  if (vao_ != 0) {
    glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
  }
  if (vbo_ != 0) {
    glDeleteBuffers(1, &vbo_);
    vbo_ = 0;
  }
  if (shaderProgram_ != 0) {
    glDeleteProgram(shaderProgram_);
    shaderProgram_ = 0;
  }
  initialized_ = false;
}

} // namespace ggm::gui
