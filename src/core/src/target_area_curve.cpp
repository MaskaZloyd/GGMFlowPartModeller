#include "core/target_area_curve.hpp"

#include <algorithm>
#include <cmath>

namespace ggm::core {

namespace {

constexpr double kMinAreaValue = 1e-6;
constexpr double kDuplicateXiTolerance = 1e-6;

[[nodiscard]] double
sign(double value) noexcept
{
  if (value > 0.0) {
    return 1.0;
  }
  if (value < 0.0) {
    return -1.0;
  }
  return 0.0;
}

[[nodiscard]] double
sanitizeXi(double xi) noexcept
{
  if (!std::isfinite(xi)) {
    return 0.0;
  }
  return std::clamp(xi, 0.0, 1.0);
}

[[nodiscard]] double
sanitizeAreaValue(double value) noexcept
{
  if (!std::isfinite(value)) {
    return kMinAreaValue;
  }
  return std::max(value, kMinAreaValue);
}

[[nodiscard]] double
filteredEndpointDerivative(double derivative, double adjacentSlope, double nextSlope) noexcept
{
  if (sign(derivative) != sign(adjacentSlope)) {
    return 0.0;
  }
  if (sign(adjacentSlope) != sign(nextSlope) &&
      std::abs(derivative) > 3.0 * std::abs(adjacentSlope)) {
    return 3.0 * adjacentSlope;
  }
  return derivative;
}

[[nodiscard]] double
pchipDerivativeAt(std::span<const TargetAreaPoint> points, std::size_t pointIndex)
{
  if (points.size() < 2U || pointIndex >= points.size()) {
    return 0.0;
  }

  const auto segmentLength = [points](std::size_t segmentIndex) noexcept {
    return points[segmentIndex + 1U].xi - points[segmentIndex].xi;
  };
  const auto segmentSlope = [points, segmentLength](std::size_t segmentIndex) noexcept {
    const double dx = segmentLength(segmentIndex);
    if (dx <= kDuplicateXiTolerance) {
      return 0.0;
    }
    return (points[segmentIndex + 1U].value - points[segmentIndex].value) / dx;
  };

  if (points.size() == 2U) {
    return segmentSlope(0U);
  }

  if (pointIndex == 0U) {
    const double firstH = segmentLength(0U);
    const double secondH = segmentLength(1U);
    if (firstH <= kDuplicateXiTolerance || secondH <= kDuplicateXiTolerance) {
      return 0.0;
    }
    const double firstSlope = segmentSlope(0U);
    const double secondSlope = segmentSlope(1U);
    const double derivative =
      (((2.0 * firstH) + secondH) * firstSlope - firstH * secondSlope) / (firstH + secondH);
    return filteredEndpointDerivative(derivative, firstSlope, secondSlope);
  }

  const auto lastIndex = points.size() - 1U;
  if (pointIndex == lastIndex) {
    const auto lastSegment = lastIndex - 1U;
    const auto previousSegment = lastIndex - 2U;
    const double lastH = segmentLength(lastSegment);
    const double previousH = segmentLength(previousSegment);
    if (lastH <= kDuplicateXiTolerance || previousH <= kDuplicateXiTolerance) {
      return 0.0;
    }
    const double lastSlope = segmentSlope(lastSegment);
    const double previousSlope = segmentSlope(previousSegment);
    const double derivative =
      (((2.0 * lastH) + previousH) * lastSlope - lastH * previousSlope) / (lastH + previousH);
    return filteredEndpointDerivative(derivative, lastSlope, previousSlope);
  }

  const double leftSlope = segmentSlope(pointIndex - 1U);
  const double rightSlope = segmentSlope(pointIndex);
  if (leftSlope == 0.0 || rightSlope == 0.0 || sign(leftSlope) != sign(rightSlope)) {
    return 0.0;
  }

  const double leftH = segmentLength(pointIndex - 1U);
  const double rightH = segmentLength(pointIndex);
  if (leftH <= kDuplicateXiTolerance || rightH <= kDuplicateXiTolerance) {
    return 0.0;
  }

  const double w1 = (2.0 * rightH) + leftH;
  const double w2 = rightH + (2.0 * leftH);
  return (w1 + w2) / ((w1 / leftSlope) + (w2 / rightSlope));
}

[[nodiscard]] double
pchipEvaluate(std::span<const TargetAreaPoint> points,
              std::size_t leftIndex,
              std::size_t rightIndex,
              double t,
              double dx)
{
  const auto& left = points[leftIndex];
  const auto& right = points[rightIndex];
  const double leftDerivative = pchipDerivativeAt(points, leftIndex);
  const double rightDerivative = pchipDerivativeAt(points, rightIndex);
  const double t2 = t * t;
  const double t3 = t2 * t;
  const double h00 = (2.0 * t3) - (3.0 * t2) + 1.0;
  const double h10 = t3 - (2.0 * t2) + t;
  const double h01 = (-2.0 * t3) + (3.0 * t2);
  const double h11 = t3 - t2;
  const double value = (h00 * left.value) + (h10 * dx * leftDerivative) + (h01 * right.value) +
                       (h11 * dx * rightDerivative);
  return sanitizeAreaValue(value);
}

}

TargetAreaCurve::TargetAreaCurve() : points_{{0.0, 1.0}, {1.0, 1.0}} {}

TargetAreaCurve::TargetAreaCurve(std::vector<TargetAreaPoint> points) : points_(std::move(points))
{
  sortAndClamp();
}

std::span<const TargetAreaPoint>
TargetAreaCurve::points() const noexcept
{
  return points_;
}

std::span<TargetAreaPoint>
TargetAreaCurve::points() noexcept
{
  return points_;
}

void
TargetAreaCurve::setPoints(std::vector<TargetAreaPoint> points)
{
  points_ = std::move(points);
  sortAndClamp();
}

double
TargetAreaCurve::evaluate(double xi) const noexcept
{
  if (points_.empty()) {
    return 0.0;
  }

  if (points_.size() == 1U) {
    return sanitizeAreaValue(points_.front().value);
  }

  const double clampedXi = sanitizeXi(xi);
  if (clampedXi <= points_.front().xi) {
    return points_.front().value;
  }
  if (clampedXi >= points_.back().xi) {
    return points_.back().value;
  }

  const auto it = std::lower_bound(
    points_.begin(), points_.end(), clampedXi, [](const TargetAreaPoint& point, double value) {
      return point.xi < value;
    });
  if (it == points_.begin() || it == points_.end()) {
    return points_.back().value;
  }

  const auto rightIndex = static_cast<std::size_t>(std::distance(points_.begin(), it));
  const auto leftIndex = rightIndex - 1U;
  const auto& left = points_[leftIndex];
  const auto& right = points_[rightIndex];

  const double dx = right.xi - left.xi;
  if (std::abs(dx) <= kDuplicateXiTolerance) {
    return right.value;
  }

  const double t = std::clamp((clampedXi - left.xi) / dx, 0.0, 1.0);
  return pchipEvaluate(points_, leftIndex, rightIndex, t, dx);
}

void
TargetAreaCurve::sortAndClamp()
{
  for (auto& point : points_) {
    point.xi = sanitizeXi(point.xi);
    point.value = sanitizeAreaValue(point.value);
  }

  std::sort(points_.begin(),
            points_.end(),
            [](const TargetAreaPoint& lhs, const TargetAreaPoint& rhs) { return lhs.xi < rhs.xi; });

  std::vector<TargetAreaPoint> cleaned;
  cleaned.reserve(points_.size());

  for (const auto& point : points_) {
    if (cleaned.empty() || std::abs(cleaned.back().xi - point.xi) > kDuplicateXiTolerance) {
      cleaned.push_back(point);
      continue;
    }

    cleaned.back().value = 0.5 * (cleaned.back().value + point.value);
  }

  points_ = std::move(cleaned);
}

bool
TargetAreaCurve::isValid() const noexcept
{
  if (points_.size() < 2U) {
    return false;
  }

  for (std::size_t i = 0; i < points_.size(); ++i) {
    const auto& point = points_[i];
    if (!std::isfinite(point.xi) || !std::isfinite(point.value) || point.xi < 0.0 ||
        point.xi > 1.0 || point.value < kMinAreaValue) {
      return false;
    }

    if (i > 0U && point.xi <= points_[i - 1U].xi) {
      return false;
    }
  }

  return true;
}

}
