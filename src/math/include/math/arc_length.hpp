#pragma once

#include "math/types.hpp"

#include <span>
#include <vector>

namespace ggm::math {

/// Resample a polyline to n equally arc-length spaced points.
/// Input must have at least 2 points. Returns n points including endpoints.
[[nodiscard]] std::vector<Vec2>
resampleArcLength(std::span<const Vec2> polyline, int n) noexcept;

/// Compute cumulative arc-length values normalized to [0,1].
/// Returns vector of same size as input.
[[nodiscard]] std::vector<double>
cumulativeArcLength(std::span<const Vec2> polyline) noexcept;

}
