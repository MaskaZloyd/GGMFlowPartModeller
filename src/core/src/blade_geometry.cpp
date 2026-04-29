#include "core/blade_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <vector>

namespace ggm::core {

namespace {

[[nodiscard]] bool
isFinitePoint(const BladePlanPoint& point) noexcept
{
  return std::isfinite(point.xMm) && std::isfinite(point.yMm);
}

[[nodiscard]] BladePlanPoint
sub(const BladePlanPoint& lhs, const BladePlanPoint& rhs) noexcept
{
  return {.xMm = lhs.xMm - rhs.xMm, .yMm = lhs.yMm - rhs.yMm};
}

[[nodiscard]] BladePlanPoint
add(const BladePlanPoint& lhs, const BladePlanPoint& rhs) noexcept
{
  return {.xMm = lhs.xMm + rhs.xMm, .yMm = lhs.yMm + rhs.yMm};
}

[[nodiscard]] BladePlanPoint
mul(const BladePlanPoint& point, double scale) noexcept
{
  return {.xMm = point.xMm * scale, .yMm = point.yMm * scale};
}

[[nodiscard]] double
norm(const BladePlanPoint& point) noexcept
{
  return std::hypot(point.xMm, point.yMm);
}

[[nodiscard]] BladePlanPoint
normalForTangent(const BladePlanPoint& tangent) noexcept
{
  const double length = norm(tangent);
  if (length <= 1e-12) {
    return {.xMm = 0.0, .yMm = 0.0};
  }
  return {.xMm = -tangent.yMm / length, .yMm = tangent.xMm / length};
}

[[nodiscard]] BladePlanPoint
normalized(const BladePlanPoint& point) noexcept
{
  const double length = norm(point);
  if (length <= 1e-12) {
    return {};
  }
  return {.xMm = point.xMm / length, .yMm = point.yMm / length};
}

[[nodiscard]] BladePlanPoint
pointOnCircle(double radius, double angleRad) noexcept
{
  return {.xMm = radius * std::cos(angleRad), .yMm = radius * std::sin(angleRad)};
}

[[nodiscard]] BladePlanPoint
lerp(const BladePlanPoint& a, const BladePlanPoint& b, double t) noexcept
{
  return {.xMm = a.xMm + (b.xMm - a.xMm) * t, .yMm = a.yMm + (b.yMm - a.yMm) * t};
}

[[nodiscard]] std::optional<BladePlanPoint>
lineCircleIntersection(const BladePlanPoint& a,
                       const BladePlanPoint& b,
                       double radius,
                       bool preferSegment) noexcept
{
  const BladePlanPoint d = sub(b, a);
  const double qa = d.xMm * d.xMm + d.yMm * d.yMm;
  const double qb = 2.0 * (a.xMm * d.xMm + a.yMm * d.yMm);
  const double qc = a.xMm * a.xMm + a.yMm * a.yMm - radius * radius;
  if (qa <= 1.0e-18) {
    return std::nullopt;
  }

  const double discriminant = qb * qb - 4.0 * qa * qc;
  if (discriminant < 0.0) {
    return std::nullopt;
  }

  const double root = std::sqrt(std::max(0.0, discriminant));
  const double t0 = (-qb - root) / (2.0 * qa);
  const double t1 = (-qb + root) / (2.0 * qa);

  const auto validSegmentT = [](double t) { return t >= -1.0e-9 && t <= 1.0 + 1.0e-9; };
  std::optional<double> best;
  for (double t : {t0, t1}) {
    if (preferSegment && !validSegmentT(t)) {
      continue;
    }
    if (!best || std::abs(t - 1.0) < std::abs(*best - 1.0)) {
      best = t;
    }
  }
  if (!best && preferSegment) {
    return std::nullopt;
  }
  return lerp(a, b, *best);
}

[[nodiscard]] double
cross(const BladePlanPoint& lhs, const BladePlanPoint& rhs) noexcept
{
  return lhs.xMm * rhs.yMm - lhs.yMm * rhs.xMm;
}

[[nodiscard]] double
normalizeAnglePositive(double angleRad) noexcept
{
  const double tau = 2.0 * std::numbers::pi;
  angleRad = std::fmod(angleRad, tau);
  if (angleRad < 0.0) {
    angleRad += tau;
  }
  return angleRad;
}

[[nodiscard]] double
advanceAngle(double fromRad, double toRad, int direction) noexcept
{
  const double tau = 2.0 * std::numbers::pi;
  fromRad = normalizeAnglePositive(fromRad);
  toRad = normalizeAnglePositive(toRad);
  if (direction >= 0) {
    double delta = toRad - fromRad;
    if (delta < 0.0) {
      delta += tau;
    }
    return delta;
  }
  double delta = fromRad - toRad;
  if (delta < 0.0) {
    delta += tau;
  }
  return -delta;
}

void
appendCircularArc(std::vector<BladePlanPoint>& out,
                  BladePlanPoint center,
                  double radius,
                  double startAngleRad,
                  double endAngleRad,
                  int direction,
                  int segmentCount,
                  bool skipFirst)
{
  if (radius <= 1e-12 || segmentCount <= 0) {
    return;
  }
  const double sweep = advanceAngle(startAngleRad, endAngleRad, direction);
  const int begin = skipFirst ? 1 : 0;
  for (int i = begin; i <= segmentCount; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(segmentCount);
    const double angle = startAngleRad + sweep * t;
    out.push_back(add(center, pointOnCircle(radius, angle)));
  }
}

void
appendRadiusArc(std::vector<BladePlanPoint>& out,
                double radius,
                const BladePlanPoint& start,
                const BladePlanPoint& end,
                bool skipFirst)
{
  const double startAngle = std::atan2(start.yMm, start.xMm);
  const double endAngle = std::atan2(end.yMm, end.xMm);
  const double ccwSweep = advanceAngle(startAngle, endAngle, 1);
  const double cwSweep = advanceAngle(startAngle, endAngle, -1);
  const int direction = std::abs(ccwSweep) <= std::abs(cwSweep) ? 1 : -1;
  appendCircularArc(out, {}, radius, startAngle, endAngle, direction, 10, skipFirst);
}

void
trimSideToOutletRadius(std::vector<BladePlanPoint>& side, double radius) noexcept
{
  if (side.size() < 2U || radius <= 0.0) {
    return;
  }

  constexpr double eps = 1.0e-9;
  for (std::size_t i = 1U; i < side.size(); ++i) {
    const double currentRadius = norm(side[i]);
    if (currentRadius >= radius - eps) {
      if (auto intersection = lineCircleIntersection(side[i - 1U], side[i], radius, true)) {
        side.resize(i);
        side.push_back(*intersection);
        return;
      }
    }
  }

  if (auto intersection = lineCircleIntersection(side[side.size() - 2U], side.back(), radius, false)) {
    side.back() = *intersection;
  }
}

}

double
evaluateBladeAngleDeg(const BladeDesignParams& params, double t) noexcept
{
  t = std::clamp(t, 0.0, 1.0);
  switch (params.angleLaw) {
    case BladeAngleLaw::Constant:
      return params.beta1Deg;
    case BladeAngleLaw::Linear:
      return params.beta1Deg + (params.beta2Deg - params.beta1Deg) * t;
    case BladeAngleLaw::Quadratic: {
      const double mid = 0.5 * (params.beta1Deg + params.beta2Deg);
      const double blend = 4.0 * t * (1.0 - t);
      return (1.0 - t) * params.beta1Deg + t * params.beta2Deg + 0.25 * blend * (mid - params.beta1Deg);
    }
    case BladeAngleLaw::Bezier: {
      const double p0 = params.beta1Deg;
      const double p3 = params.beta2Deg;
      const double p1 = p0 + (p3 - p0) / 3.0;
      const double p2 = p0 + 2.0 * (p3 - p0) / 3.0;
      const double omt = 1.0 - t;
      return (omt * omt * omt * p0) + (3.0 * omt * omt * t * p1) +
             (3.0 * omt * t * t * p2) + (t * t * t * p3);
    }
  }
  return params.beta1Deg;
}

double
evaluateBladeThicknessMm(const BladeDesignParams& params, double t) noexcept
{
  t = std::clamp(t, 0.0, 1.0);
  switch (params.thicknessLaw) {
    case BladeThicknessLaw::Constant:
      return params.s1Mm;
    case BladeThicknessLaw::Linear:
      return params.s1Mm + (params.s2Mm - params.s1Mm) * t;
    case BladeThicknessLaw::Parabolic: {
      const double base = params.s1Mm + (params.s2Mm - params.s1Mm) * t;
      const double bulge = std::max(0.0, params.sMaxMm - std::max(params.s1Mm, params.s2Mm));
      return base + 4.0 * bulge * t * (1.0 - t);
    }
    case BladeThicknessLaw::Bezier: {
      const double p0 = params.s1Mm;
      const double p3 = params.s2Mm;
      const double p1 = params.sMaxMm;
      const double p2 = params.sMaxMm;
      const double omt = 1.0 - t;
      return (omt * omt * omt * p0) + (3.0 * omt * omt * t * p1) +
             (3.0 * omt * t * t * p2) + (t * t * t * p3);
    }
  }
  return params.s1Mm;
}

Result<BladeContour>
buildCylindricalBladeContour(std::span<const BladeSectionSample> sections) noexcept
{
  if (sections.size() < 2U) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  BladeContour contour;
  contour.centerline.reserve(sections.size());
  contour.pressureSide.reserve(sections.size());
  contour.suctionSide.reserve(sections.size());

  for (const auto& section : sections) {
    const double x = section.rMm * std::cos(section.phiRad);
    const double y = section.rMm * std::sin(section.phiRad);
    BladePlanPoint point{.xMm = x, .yMm = y};
    if (!isFinitePoint(point) || !std::isfinite(section.thicknessMm)) {
      return std::unexpected(CoreError::InvalidParameter);
    }
    contour.centerline.push_back(point);
  }

  for (std::size_t i = 0; i < contour.centerline.size(); ++i) {
    BladePlanPoint tangent{};
    if (i == 0U) {
      tangent = sub(contour.centerline[1], contour.centerline[0]);
    } else if (i + 1U == contour.centerline.size()) {
      tangent = sub(contour.centerline[i], contour.centerline[i - 1U]);
    } else {
      tangent = sub(contour.centerline[i + 1U], contour.centerline[i - 1U]);
    }

    const BladePlanPoint normal = normalForTangent(tangent);
    const double halfThickness = 0.5 * sections[i].thicknessMm;
    const BladePlanPoint center = contour.centerline[i];
    contour.pressureSide.push_back(
      {.xMm = center.xMm + halfThickness * normal.xMm,
       .yMm = center.yMm + halfThickness * normal.yMm});
    contour.suctionSide.push_back(
      {.xMm = center.xMm - halfThickness * normal.xMm,
       .yMm = center.yMm - halfThickness * normal.yMm});
  }

  const double outletRadius = sections.back().rMm;
  trimSideToOutletRadius(contour.pressureSide, outletRadius);
  trimSideToOutletRadius(contour.suctionSide, outletRadius);

  contour.closedContour.reserve(contour.pressureSide.size() + contour.suctionSide.size() + 24U);
  for (const auto& point : contour.pressureSide) {
    contour.closedContour.push_back(point);
  }

  appendRadiusArc(contour.closedContour,
                  outletRadius,
                  contour.pressureSide.back(),
                  contour.suctionSide.back(),
                  true);

  for (auto it = contour.suctionSide.rbegin() + 1; it != contour.suctionSide.rend(); ++it) {
    contour.closedContour.push_back(*it);
  }

  const BladePlanPoint inletTangent = sub(contour.centerline[1], contour.centerline[0]);
  const BladePlanPoint inletDirection = normalized(inletTangent);
  const BladePlanPoint inletNormal = normalForTangent(inletTangent);
  const BladePlanPoint inletCenter = contour.centerline.front();
  const double inletRadius = 0.5 * sections.front().thicknessMm;
  const double pressureAngle =
    std::atan2(contour.pressureSide.front().yMm - inletCenter.yMm,
               contour.pressureSide.front().xMm - inletCenter.xMm);
  const double suctionAngle =
    std::atan2(contour.suctionSide.front().yMm - inletCenter.yMm,
               contour.suctionSide.front().xMm - inletCenter.xMm);
  const int inletArcDirection = cross(inletNormal, mul(inletDirection, -1.0)) >= 0.0 ? -1 : 1;
  appendCircularArc(contour.closedContour,
                    inletCenter,
                    inletRadius,
                    suctionAngle,
                    pressureAngle,
                    inletArcDirection,
                    14,
                    true);

  if (!contour.closedContour.empty()) {
    contour.closedContour.push_back(contour.closedContour.front());
  }

  return contour;
}

BladeContour
rotateBladeContour(const BladeContour& contour, double angleRad)
{
  const double c = std::cos(angleRad);
  const double s = std::sin(angleRad);
  const auto rotatePoint = [&](const BladePlanPoint& point) {
    return BladePlanPoint{
      .xMm = point.xMm * c - point.yMm * s,
      .yMm = point.xMm * s + point.yMm * c,
    };
  };
  const auto rotateVector = [&](const std::vector<BladePlanPoint>& points) {
    std::vector<BladePlanPoint> out;
    out.reserve(points.size());
    for (const auto& point : points) {
      out.push_back(rotatePoint(point));
    }
    return out;
  };

  return BladeContour{
    .centerline = rotateVector(contour.centerline),
    .pressureSide = rotateVector(contour.pressureSide),
    .suctionSide = rotateVector(contour.suctionSide),
    .closedContour = rotateVector(contour.closedContour),
  };
}

}
