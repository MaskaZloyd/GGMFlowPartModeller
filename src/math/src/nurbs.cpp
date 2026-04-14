#include "math/nurbs.hpp"

#include <cmath>

namespace ggm::math {

NurbsCurve buildFromSegments(std::span<const ArcBezier> segments) noexcept {
  NurbsCurve curve;
  curve.degree = 2;

  for (std::size_t idx = 0; idx < segments.size(); ++idx) {
    const auto& seg = segments[idx];
    if (idx == 0) {
      curve.controlPoints.push_back(seg.p0);
      curve.weights.push_back(1.0);
    }
    curve.controlPoints.push_back(seg.p1);
    curve.weights.push_back(seg.w1);
    curve.controlPoints.push_back(seg.p2);
    curve.weights.push_back(1.0);
  }

  auto nseg = static_cast<double>(segments.size());
  curve.knots = {0.0, 0.0, 0.0};
  for (std::size_t idx = 1; idx < segments.size(); ++idx) {
    double knot = static_cast<double>(idx) / nseg;
    curve.knots.push_back(knot);
    curve.knots.push_back(knot);
  }
  curve.knots.push_back(1.0);
  curve.knots.push_back(1.0);
  curve.knots.push_back(1.0);

  return curve;
}

namespace {

// Recursive B-spline basis function (Cox-de Boor).
double basisFunc(int idx,
                 int degree,
                 double param,
                 const std::vector<double>& knots) noexcept {
  if (degree == 0) {
    return (knots[static_cast<std::size_t>(idx)] <= param &&
            param < knots[static_cast<std::size_t>(idx + 1)])
               ? 1.0
               : 0.0;
  }

  auto uidx = static_cast<std::size_t>(idx);
  auto udeg = static_cast<std::size_t>(degree);

  double left = 0.0;
  double denomLeft = knots[uidx + udeg] - knots[uidx];
  if (denomLeft != 0.0) {
    left = (param - knots[uidx]) / denomLeft * basisFunc(idx, degree - 1, param, knots);
  }

  double right = 0.0;
  double denomRight = knots[uidx + udeg + 1] - knots[uidx + 1];
  if (denomRight != 0.0) {
    right = (knots[uidx + udeg + 1] - param) / denomRight *
            basisFunc(idx + 1, degree - 1, param, knots);
  }

  return left + right;
}

} // namespace

std::vector<Vec2> evaluate(const NurbsCurve& curve, int numPoints) noexcept {
  std::vector<Vec2> result;
  result.reserve(static_cast<std::size_t>(numPoints));

  auto numCps = static_cast<int>(curve.controlPoints.size());

  for (int ptIdx = 0; ptIdx < numPoints; ++ptIdx) {
    double param = static_cast<double>(ptIdx) / static_cast<double>(numPoints - 1);

    // Handle endpoint exactly
    if (ptIdx == numPoints - 1) {
      result.push_back(curve.controlPoints.back());
      continue;
    }

    double denominator = 0.0;
    Vec2 numerator = Vec2::Zero();

    for (int cpIdx = 0; cpIdx < numCps; ++cpIdx) {
      double basis = basisFunc(cpIdx, curve.degree, param, curve.knots);
      double weighted = basis * curve.weights[static_cast<std::size_t>(cpIdx)];
      numerator += weighted * curve.controlPoints[static_cast<std::size_t>(cpIdx)];
      denominator += weighted;
    }

    if (std::abs(denominator) > 1e-15) {
      result.push_back(numerator / denominator);
    } else {
      result.push_back(numerator);
    }
  }

  return result;
}

} // namespace ggm::math
