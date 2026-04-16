#include "math/nurbs.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>

namespace ggm::math {

NurbsCurve
buildFromSegments(std::span<const ArcBezier> segments) noexcept
{
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

// Binary-search for the knot span such that knots[k] <= param < knots[k+1].
// numCps = controlPoints.size() - 1, deg = curve degree. Span index in [deg, numCps].
[[nodiscard]] std::size_t
findSpan(std::size_t numCps,
         std::size_t deg,
         double param,
         const std::vector<double>& knots) noexcept
{
  if (param >= knots[numCps + 1]) {
    return numCps;
  }
  if (param <= knots[deg]) {
    return deg;
  }
  std::size_t low = deg;
  std::size_t high = numCps + 1;
  std::size_t mid = (low + high) / 2;
  while (param < knots[mid] || param >= knots[mid + 1]) {
    if (param < knots[mid]) {
      high = mid;
    } else {
      low = mid;
    }
    mid = (low + high) / 2;
  }
  return mid;
}

// Compute the (deg+1) non-zero basis functions N[span-deg..span] at param
// using the triangular recurrence from The NURBS Book section 2.5
// (Piegl and Tiller, algorithm A2.2).
// Only for deg = 2 in this codebase — the fixed-size arrays reflect that.
void
basisFunctions(std::size_t span,
               double param,
               std::size_t deg,
               const std::vector<double>& knots,
               std::array<double, 3>& out) noexcept
{
  std::array<double, 3> leftDelta{};
  std::array<double, 3> rightDelta{};
  out[0] = 1.0;
  for (std::size_t jj = 1; jj <= deg; ++jj) {
    leftDelta[jj] = param - knots[span + 1 - jj];
    rightDelta[jj] = knots[span + jj] - param;
    double saved = 0.0;
    for (std::size_t rr = 0; rr < jj; ++rr) {
      const double denom = rightDelta[rr + 1] + leftDelta[jj - rr];
      const double temp = denom != 0.0 ? out[rr] / denom : 0.0;
      out[rr] = saved + (rightDelta[rr + 1] * temp);
      saved = leftDelta[jj - rr] * temp;
    }
    out[jj] = saved;
  }
}

} // namespace

std::vector<Vec2>
evaluate(const NurbsCurve& curve, int numPoints) noexcept
{
  std::vector<Vec2> result;
  if (numPoints <= 0 || curve.controlPoints.empty()) {
    return result;
  }
  assert(curve.degree == 2 && "evaluate: fixed-size basis arrays assume degree == 2");
  result.reserve(static_cast<std::size_t>(numPoints));

  const auto deg = static_cast<std::size_t>(curve.degree);
  const std::size_t numCps = curve.controlPoints.size() - 1;

  for (int ptIdx = 0; ptIdx < numPoints; ++ptIdx) {
    if (ptIdx == numPoints - 1) {
      result.push_back(curve.controlPoints.back());
      continue;
    }
    const double param = static_cast<double>(ptIdx) / static_cast<double>(numPoints - 1);

    const std::size_t span = findSpan(numCps, deg, param, curve.knots);
    std::array<double, 3> basis{};
    basisFunctions(span, param, deg, curve.knots, basis);

    Vec2 numerator = Vec2::Zero();
    double denominator = 0.0;
    for (std::size_t jj = 0; jj <= deg; ++jj) {
      const std::size_t cpIdx = span - deg + jj;
      const double wgt = curve.weights[cpIdx] * basis[jj];
      numerator += wgt * curve.controlPoints[cpIdx];
      denominator += wgt;
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
