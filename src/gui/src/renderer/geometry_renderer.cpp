#include "gui/renderer/geometry_renderer.hpp"

#include "gui/renderer/geometry/plot_viewport.hpp"
#include "gui/renderer/opengl/gl_state_guard.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>

namespace ggm::gui {
namespace {

constexpr int kTargetGridLineCount = 8;
constexpr auto kBackground = Rgba{0.988F, 0.990F, 0.994F, 1.0F};
constexpr auto kMinorGridColor = Rgba{0.900F, 0.905F, 0.915F, 0.70F};
constexpr auto kMajorGridColor = Rgba{0.780F, 0.790F, 0.810F, 0.85F};
constexpr auto kComputationalGridColor = Rgba{0.550F, 0.620F, 0.710F, 0.72F};
constexpr auto kMeanLineColor = Rgba{0.930F, 0.560F, 0.120F, 0.95F};
constexpr auto kHubColor = Rgba{0.780F, 0.200F, 0.180F, 1.0F};
constexpr auto kShroudColor = Rgba{0.180F, 0.360F, 0.780F, 1.0F};
constexpr auto kContourOutlineColor = Rgba{1.0F, 1.0F, 1.0F, 0.72F};

[[nodiscard]] Rgba
viridisColor(float t, float alpha = 1.0F) noexcept
{
  t = std::clamp(t, 0.0F, 1.0F);
  static constexpr auto kStops = std::to_array<Rgba>({
    {0.267F, 0.005F, 0.329F, 1.0F},
    {0.275F, 0.125F, 0.474F, 1.0F},
    {0.230F, 0.318F, 0.545F, 1.0F},
    {0.173F, 0.449F, 0.558F, 1.0F},
    {0.128F, 0.567F, 0.551F, 1.0F},
    {0.135F, 0.659F, 0.518F, 1.0F},
    {0.267F, 0.749F, 0.440F, 1.0F},
    {0.526F, 0.830F, 0.288F, 1.0F},
    {0.993F, 0.906F, 0.144F, 1.0F},
  });

  const auto scaled = t * static_cast<float>(kStops.size() - 1U);
  const auto lowerIndex = std::min(static_cast<std::size_t>(scaled), kStops.size() - 2U);
  const auto fraction = scaled - static_cast<float>(lowerIndex);
  const auto& a = kStops[lowerIndex];
  const auto& b = kStops[lowerIndex + 1U];
  return Rgba{
    .r = a.r + fraction * (b.r - a.r),
    .g = a.g + fraction * (b.g - a.g),
    .b = a.b + fraction * (b.b - a.b),
    .a = alpha,
  };
}

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
appendSegment(std::vector<LineSegment2D>& out,
              double z0,
              double r0,
              double z1,
              double r1,
              const LineStyle& style)
{
  out.push_back(LineSegment2D{
    .a = {z0, r0},
    .b = {z1, r1},
    .style = style,
    .dashOffsetPx = 0.0,
  });
}

void
drawCoordinateGrid(const PlotViewport& viewport,
                   SdfLineRenderer2D& lineRenderer,
                   std::vector<LineSegment2D>& scratch) noexcept
{
  const auto map = viewport.toMap();
  const auto majorStep =
    niceGridStep(std::max(viewport.rangeZ(), viewport.rangeR()), kTargetGridLineCount);
  const auto minorStep = majorStep / 5.0;

  const auto appendGrid = [&](double step, const LineStyle& style) {
    scratch.clear();
    const auto minZ = std::floor(map.minZ / step) * step;
    const auto maxZ = std::ceil(map.maxZ / step) * step;
    const auto minR = std::floor(map.minR / step) * step;
    const auto maxR = std::ceil(map.maxR / step) * step;
    for (auto z = minZ; z <= maxZ + (step * 0.5); z += step) {
      appendSegment(scratch, z, minR, z, maxR, style);
    }
    for (auto r = minR; r <= maxR + (step * 0.5); r += step) {
      appendSegment(scratch, minZ, r, maxZ, r, style);
    }
    lineRenderer.drawSegments(std::span<const LineSegment2D>{scratch.data(), scratch.size()},
                              viewport);
  };

  appendGrid(minorStep, LineStyle{.color = kMinorGridColor, .thicknessPx = 1.0F});
  appendGrid(majorStep, LineStyle{.color = kMajorGridColor, .thicknessPx = 1.25F});
}

void
drawComputationalGrid(const core::FlowResults& flow,
                      const PlotViewport& viewport,
                      SdfLineRenderer2D& lineRenderer,
                      std::vector<LineSegment2D>& scratch) noexcept
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

  const auto style = LineStyle{.color = kComputationalGridColor, .thicknessPx = 1.0F};
  scratch.clear();
  scratch.reserve(rowCount);
  for (std::size_t row = 0; row < rowCount; ++row) {
    const auto hubIndex = row * columnCount;
    const auto shroudIndex = hubIndex + columnCount - 1U;
    scratch.push_back(LineSegment2D{
      .a = grid.nodes[hubIndex],
      .b = grid.nodes[shroudIndex],
      .style = style,
      .dashOffsetPx = 0.0,
    });
  }
  lineRenderer.drawSegments(std::span<const LineSegment2D>{scratch.data(), scratch.size()},
                            viewport);
}

[[nodiscard]] double
maxStreamlineSpeed(const core::FlowResults& flow) noexcept
{
  double peak = 0.0;
  for (const auto& velocity : flow.velocities) {
    for (const auto& sample : velocity.samples) {
      peak = std::max(peak, sample.speed);
    }
  }
  return peak > 0.0 ? peak : 1.0;
}

void
drawVelocityHeatmap(const core::FlowResults& flow,
                    double peakSpeed,
                    const PlotViewport& viewport,
                    MeshRenderer2D& meshRenderer,
                    std::vector<MeshVertex2D>& scratch) noexcept
{
  const auto& grid = flow.solution.grid;
  if (grid.nodes.empty() || grid.triangles.empty() || flow.velocities.empty() || peakSpeed <= 0.0) {
    return;
  }

  std::vector<float> nodeSpeed(grid.nodes.size(), 0.0F);
  for (std::size_t nodeIndex = 0; nodeIndex < grid.nodes.size(); ++nodeIndex) {
    const auto& node = grid.nodes[nodeIndex];
    double bestSq = std::numeric_limits<double>::max();
    double bestSpeed = 0.0;
    for (const auto& velocity : flow.velocities) {
      for (const auto& sample : velocity.samples) {
        const double dz = sample.point.x() - node.x();
        const double dr = sample.point.y() - node.y();
        const double distanceSq = (dz * dz) + (dr * dr);
        if (distanceSq < bestSq) {
          bestSq = distanceSq;
          bestSpeed = sample.speed;
        }
      }
    }
    nodeSpeed[nodeIndex] = static_cast<float>(bestSpeed);
  }

  scratch.clear();
  scratch.reserve(grid.triangles.size() * 3U);
  for (const auto& triangle : grid.triangles) {
    auto valid = true;
    for (int index : triangle) {
      if (index < 0 || static_cast<std::size_t>(index) >= grid.nodes.size()) {
        valid = false;
      }
    }
    if (!valid) {
      continue;
    }

    for (int index : triangle) {
      const auto nodeIndex = static_cast<std::size_t>(index);
      const auto t = std::clamp(nodeSpeed[nodeIndex] / static_cast<float>(peakSpeed), 0.0F, 1.0F);
      scratch.push_back(MeshVertex2D{
        .point = grid.nodes[nodeIndex],
        .color = viridisColor(t, 0.55F),
      });
    }
  }

  meshRenderer.drawTriangles(std::span<const MeshVertex2D>{scratch.data(), scratch.size()},
                             viewport);
}

void
drawStreamlines(const core::FlowResults& flow,
                const RenderSettings& settings,
                double peakSpeed,
                const PlotViewport& viewport,
                SdfLineRenderer2D& lineRenderer,
                std::vector<LineSegment2D>& scratch) noexcept
{
  const auto thickness = std::max(settings.streamlineWidth + 0.4F, 1.0F);

  if (settings.colorStreamlinesBySpeed && !flow.velocities.empty()) {
    scratch.clear();
    for (const auto& velocity : flow.velocities) {
      if (velocity.samples.size() < 2U) {
        continue;
      }
      for (std::size_t i = 1U; i < velocity.samples.size(); ++i) {
        const auto& a = velocity.samples[i - 1U];
        const auto& b = velocity.samples[i];
        const auto speed = 0.5 * (a.speed + b.speed);
        scratch.push_back(LineSegment2D{
          .a = a.point,
          .b = b.point,
          .style =
            LineStyle{
              .color =
                viridisColor(static_cast<float>(std::clamp(speed / peakSpeed, 0.0, 1.0)), 0.92F),
              .thicknessPx = thickness,
            },
          .dashOffsetPx = 0.0,
        });
      }
    }
    lineRenderer.drawSegments(std::span<const LineSegment2D>{scratch.data(), scratch.size()},
                              viewport);
    return;
  }

  for (const auto& streamline : flow.streamlines) {
    const auto style = LineStyle{
      .color = viridisColor(static_cast<float>(streamline.psiLevel), 0.90F),
      .thicknessPx = thickness,
    };
    lineRenderer.drawPolyline(streamline.points, style, viewport);
  }
}

void
drawMeanLine(const core::FlowResults& flow,
             const RenderSettings& settings,
             const PlotViewport& viewport,
             SdfLineRenderer2D& lineRenderer) noexcept
{
  const auto& points = flow.areaProfile.midPoints;
  if (points.size() < 2U) {
    return;
  }

  const auto style = LineStyle{
    .color = kMeanLineColor,
    .thicknessPx = std::max(settings.meanLineWidth, 1.0F),
    .dashPeriodPx = 16.0F,
    .dashDuty = 0.58F,
  };
  lineRenderer.drawPolyline(points, style, viewport);
}

void
drawGeometryCurves(const core::MeridionalGeometry& geom,
                   const RenderSettings& settings,
                   const PlotViewport& viewport,
                   SdfLineRenderer2D& lineRenderer) noexcept
{
  const auto hubStyle = LineStyle{
    .color = kHubColor,
    .thicknessPx = std::max(settings.hubLineWidth, 1.0F),
    .outlineColor = kContourOutlineColor,
    .outlineThicknessPx = std::max(settings.hubLineWidth + 2.0F, 1.0F),
  };
  const auto shroudStyle = LineStyle{
    .color = kShroudColor,
    .thicknessPx = std::max(settings.shroudLineWidth, 1.0F),
    .outlineColor = kContourOutlineColor,
    .outlineThicknessPx = std::max(settings.shroudLineWidth + 2.0F, 1.0F),
  };
  lineRenderer.drawPolyline(geom.hubCurve, hubStyle, viewport);
  lineRenderer.drawPolyline(geom.shroudCurve, shroudStyle, viewport);
}

} // namespace

void
GeometryRenderer::clear(int viewportWidth, int viewportHeight) noexcept
{
  if (viewportWidth <= 0 || viewportHeight <= 0) {
    return;
  }
  const auto state = ScopedGlState2D{};
  state.beginFrame(
    viewportWidth, viewportHeight, kBackground.r, kBackground.g, kBackground.b, kBackground.a);
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

  const auto state = ScopedGlState2D{};
  state.beginFrame(
    viewportWidth, viewportHeight, kBackground.r, kBackground.g, kBackground.b, kBackground.a);

  const auto viewport = PlotViewport::fromGeometry(geom, viewportWidth, viewportHeight);
  if (!viewport.hasData()) {
    return {};
  }

  if (flow != nullptr) {
    const double peakSpeed = maxStreamlineSpeed(*flow);
    if (settings.showVelocityHeatmap) {
      drawVelocityHeatmap(*flow, peakSpeed, viewport, meshRenderer_, meshScratch_);
    }
  }

  if (settings.showCoordGrid) {
    drawCoordinateGrid(viewport, lineRenderer_, segmentScratch_);
  }

  if (settings.showComputationalGrid && flow != nullptr) {
    drawComputationalGrid(*flow, viewport, lineRenderer_, segmentScratch_);
  }

  if (flow != nullptr) {
    const double peakSpeed = maxStreamlineSpeed(*flow);
    drawStreamlines(*flow, settings, peakSpeed, viewport, lineRenderer_, segmentScratch_);
    drawMeanLine(*flow, settings, viewport, lineRenderer_);
  }

  drawGeometryCurves(geom, settings, viewport, lineRenderer_);
  return viewport.toMap();
}

} // namespace ggm::gui
