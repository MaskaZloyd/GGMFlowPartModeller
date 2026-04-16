#include "math/arc_length.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace ggm::math {

std::vector<double>
cumulativeArcLength(std::span<const Vec2> polyline) noexcept
{
  std::vector<double> s(polyline.size(), 0.0);
  for (std::size_t i = 1; i < polyline.size(); ++i) {
    s[i] = s[i - 1] + (polyline[i] - polyline[i - 1]).norm();
  }
  double totalLen = s.back();
  if (totalLen > 0.0) {
    for (auto& val : s) {
      val /= totalLen;
    }
  }
  return s;
}

std::vector<Vec2>
resampleArcLength(std::span<const Vec2> polyline, int n) noexcept
{
  if (n <= 0 || polyline.size() < 2) {
    return {};
  }
  if (n == 1) {
    return {polyline.front()};
  }

  auto s = cumulativeArcLength(polyline);

  std::vector<Vec2> result;
  result.reserve(static_cast<std::size_t>(n));

  std::size_t idx = 0;
  for (int k = 0; k < n; ++k) {
    double u = static_cast<double>(k) / static_cast<double>(n - 1);

    // Advance idx to find segment containing u
    while (idx + 1 < polyline.size() - 1 && s[idx + 1] < u) {
      ++idx;
    }

    double segLen = s[idx + 1] - s[idx];
    if (segLen < 1e-15) {
      result.push_back(polyline[idx]);
    } else {
      double t = (u - s[idx]) / segLen;
      t = std::clamp(t, 0.0, 1.0);
      result.push_back((1.0 - t) * polyline[idx] + t * polyline[idx + 1]);
    }
  }

  return result;
}

} // namespace ggm::math
