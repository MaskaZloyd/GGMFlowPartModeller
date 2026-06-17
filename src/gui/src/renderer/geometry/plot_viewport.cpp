#include "gui/renderer/geometry/plot_viewport.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <span>

namespace ggm::gui {
namespace {

constexpr double kViewportPaddingFraction = 0.1;
constexpr double kMinimumExtent = 1.0e-3;

struct Bounds
{
  double minZ = std::numeric_limits<double>::max();
  double maxZ = std::numeric_limits<double>::lowest();
  double minR = std::numeric_limits<double>::max();
  double maxR = std::numeric_limits<double>::lowest();

  void expand(std::span<const math::Vec2> points) noexcept
  {
    for (const auto& point : points) {
      if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
        continue;
      }
      minZ = std::min(minZ, point.x());
      maxZ = std::max(maxZ, point.x());
      minR = std::min(minR, point.y());
      maxR = std::max(maxR, point.y());
    }
  }

  [[nodiscard]] bool hasData() const noexcept { return minZ <= maxZ && minR <= maxR; }

  void addPadding(double fraction) noexcept
  {
    const auto rangeZ = maxZ - minZ;
    const auto rangeR = maxR - minR;
    minZ -= rangeZ * fraction;
    maxZ += rangeZ * fraction;
    minR -= rangeR * fraction;
    maxR += rangeR * fraction;
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

} // namespace

PlotViewport
PlotViewport::fromGeometry(const core::MeridionalGeometry& geom, int widthPx, int heightPx) noexcept
{
  auto result = PlotViewport{};
  result.widthPx_ = widthPx;
  result.heightPx_ = heightPx;

  if (widthPx <= 0 || heightPx <= 0) {
    return result;
  }

  auto bounds = Bounds{};
  bounds.expand(geom.hubCurve);
  bounds.expand(geom.shroudCurve);
  if (!bounds.hasData()) {
    return result;
  }

  bounds.addPadding(kViewportPaddingFraction);
  bounds.ensureNonDegenerate();

  auto rangeZ = bounds.maxZ - bounds.minZ;
  auto rangeR = bounds.maxR - bounds.minR;
  const auto aspectView = static_cast<double>(widthPx) / static_cast<double>(heightPx);
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

  if (rangeZ <= 0.0 || rangeR <= 0.0) {
    return result;
  }

  result.hasData_ = true;
  result.minZ_ = bounds.minZ;
  result.maxZ_ = bounds.maxZ;
  result.minR_ = bounds.minR;
  result.maxR_ = bounds.maxR;
  return result;
}

ViewportMap
PlotViewport::toMap() const noexcept
{
  return ViewportMap{
    .minZ = minZ_,
    .maxZ = maxZ_,
    .minR = minR_,
    .maxR = maxR_,
    .widthPx = widthPx_,
    .heightPx = heightPx_,
  };
}

PixelPoint
PlotViewport::dataToPixel(const math::Vec2& point) const noexcept
{
  if (!hasData_ || rangeZ() <= 0.0 || rangeR() <= 0.0 || widthPx_ <= 0 || heightPx_ <= 0) {
    return {};
  }

  return PixelPoint{
    .x = ((point.x() - minZ_) / rangeZ()) * static_cast<double>(widthPx_),
    .y = ((point.y() - minR_) / rangeR()) * static_cast<double>(heightPx_),
  };
}

} // namespace ggm::gui
