#pragma once

#include "core/geometry.hpp"
#include "gui/renderer/viewport_map.hpp"
#include "math/types.hpp"

namespace ggm::gui {

struct PixelPoint
{
  double x = 0.0;
  double y = 0.0;
};

class PlotViewport
{
public:
  [[nodiscard]] static PlotViewport fromGeometry(const core::MeridionalGeometry& geom,
                                                 int widthPx,
                                                 int heightPx) noexcept;

  [[nodiscard]] bool hasData() const noexcept { return hasData_; }
  [[nodiscard]] int widthPx() const noexcept { return widthPx_; }
  [[nodiscard]] int heightPx() const noexcept { return heightPx_; }
  [[nodiscard]] double rangeZ() const noexcept { return maxZ_ - minZ_; }
  [[nodiscard]] double rangeR() const noexcept { return maxR_ - minR_; }
  [[nodiscard]] ViewportMap toMap() const noexcept;
  [[nodiscard]] PixelPoint dataToPixel(const math::Vec2& point) const noexcept;

private:
  bool hasData_ = false;
  double minZ_ = 0.0;
  double maxZ_ = 0.0;
  double minR_ = 0.0;
  double maxR_ = 0.0;
  int widthPx_ = 0;
  int heightPx_ = 0;
};

} // namespace ggm::gui
