#pragma once

#include "math/types.hpp"

#include <vector>

namespace ggm::math {

struct ArcBezier
{
  Vec2 p0;
  Vec2 p1;
  Vec2 p2;
  double w1 = 1.0;
};

// Build a rational quadratic Bezier for a circular arc.
// center — arc center, radius — arc radius,
// a0, a1 — start/end angles in radians (CCW from +X axis).
[[nodiscard]] ArcBezier
arcToBezier(const Vec2& center, double radius, double startAngle, double endAngle) noexcept;

// Build a degenerate Bezier for a straight line segment (w1=1).
[[nodiscard]] ArcBezier
segToBezier(const Vec2& start, const Vec2& end) noexcept;

// Evaluate a single rational quadratic Bezier at parameter param ∈ [0, 1].
[[nodiscard]] Vec2
evalRationalQuadratic(const ArcBezier& seg, double param) noexcept;

// Evaluate a rational quadratic Bezier to a polyline of numPoints points.
[[nodiscard]] std::vector<Vec2>
evaluateSegment(const ArcBezier& seg, int numPoints) noexcept;

} // namespace ggm::math
