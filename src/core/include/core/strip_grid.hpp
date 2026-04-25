#pragma once

#include "core/error.hpp"
#include "core/flow_solver_types.hpp"
#include "math/types.hpp"

#include <span>

namespace ggm::core {

/// Build a logically rectangular strip grid between hub and shroud polylines.
/// Both polylines must have the same length (nh points, arc-length resampled).
/// m = number of transverse points across the gap (including boundaries).
[[nodiscard]] Result<StripGrid>
buildStripGrid(std::span<const math::Vec2> hub, std::span<const math::Vec2> shroud, int m) noexcept;

}
