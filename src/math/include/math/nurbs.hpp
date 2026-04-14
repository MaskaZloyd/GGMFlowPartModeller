#pragma once

#include "math/bezier.hpp"
#include "math/types.hpp"

#include <span>
#include <vector>

namespace ggm::math {

struct NurbsCurve {
  std::vector<Vec2> controlPoints;
  std::vector<double> weights;
  std::vector<double> knots;
  int degree = 2;
};

// Assemble a piecewise rational quadratic NURBS from a sequence of Bezier segments.
// Segments must be G0-continuous (end of segment i == start of segment i+1).
// Knot vector uses doubled internal knots for C0 joins.
[[nodiscard]] NurbsCurve buildFromSegments(std::span<const ArcBezier> segments) noexcept;

// Evaluate a NURBS curve to a polyline of numPoints points.
[[nodiscard]] std::vector<Vec2> evaluate(const NurbsCurve& curve, int numPoints = 400) noexcept;

} // namespace ggm::math
