#include "math/bezier.hpp"

#include <cmath>

namespace ggm::math {

ArcBezier
arcToBezier(const Vec2& center, double radius, double startAngle, double endAngle) noexcept
{
  double delta = endAngle - startAngle;
  double phi = delta / 2.0;
  double weight = std::cos(phi);

  Vec2 pointStart = center + radius * Vec2{std::cos(startAngle), std::sin(startAngle)};
  Vec2 pointEnd = center + radius * Vec2{std::cos(endAngle), std::sin(endAngle)};

  double sign = delta > 0.0 ? 1.0 : -1.0;
  Vec2 tangent = sign * Vec2{-std::sin(startAngle), std::cos(startAngle)};
  double handle = radius * std::tan(std::abs(phi));
  Vec2 pointMid = pointStart + handle * tangent;

  return {.p0 = pointStart, .p1 = pointMid, .p2 = pointEnd, .w1 = weight};
}

ArcBezier
segToBezier(const Vec2& start, const Vec2& end) noexcept
{
  Vec2 mid = (start + end) / 2.0;
  return {.p0 = start, .p1 = mid, .p2 = end, .w1 = 1.0};
}

Vec2
evalRationalQuadratic(const ArcBezier& seg, double param) noexcept
{
  double oneMinusT = 1.0 - param;
  double basis0 = oneMinusT * oneMinusT;
  double basis1 = 2.0 * oneMinusT * param;
  double basis2 = param * param;

  double denom = basis0 + (basis1 * seg.w1) + basis2;
  Vec2 numer = basis0 * seg.p0 + (basis1 * seg.w1) * seg.p1 + basis2 * seg.p2;
  return numer / denom;
}

std::vector<Vec2>
evaluateSegment(const ArcBezier& seg, int numPoints) noexcept
{
  std::vector<Vec2> result;
  result.reserve(static_cast<std::size_t>(numPoints));
  for (int idx = 0; idx < numPoints; ++idx) {
    double param = static_cast<double>(idx) / static_cast<double>(numPoints - 1);
    result.push_back(evalRationalQuadratic(seg, param));
  }
  return result;
}

}
