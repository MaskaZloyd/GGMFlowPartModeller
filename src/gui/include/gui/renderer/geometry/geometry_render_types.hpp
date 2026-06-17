#pragma once

#include "math/types.hpp"

namespace ggm::gui {

struct Rgba
{
  float r = 0.0F;
  float g = 0.0F;
  float b = 0.0F;
  float a = 1.0F;
};

struct LineStyle
{
  Rgba color{};
  float thicknessPx = 1.0F;
  Rgba outlineColor{};
  float outlineThicknessPx = 0.0F;
  float dashPeriodPx = 0.0F;
  float dashDuty = 1.0F;
};

struct LineSegment2D
{
  math::Vec2 a{0.0, 0.0};
  math::Vec2 b{0.0, 0.0};
  LineStyle style{};
  double dashOffsetPx = 0.0;
};

struct MeshVertex2D
{
  math::Vec2 point{0.0, 0.0};
  Rgba color{};
};

} // namespace ggm::gui
