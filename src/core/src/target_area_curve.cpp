#include "core/target_area_curve.hpp"

#include <algorithm>
#include <cmath>

namespace ggm::core {

namespace {

constexpr double kMinAreaValue = 1e-6;
constexpr double kDuplicateXiTolerance = 1e-6;

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

} // namespace

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
  return ((1.0 - t) * left.value) + (t * right.value);
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

} // namespace ggm::core
