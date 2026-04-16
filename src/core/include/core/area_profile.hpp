#pragma once

#include "core/error.hpp"
#include "core/flow_solver_types.hpp"
#include "core/pump_params.hpp"
#include "math/types.hpp"

#include <span>

namespace ggm::core {

// Compute the area profile F(s) along the equidistant (mean) line.
// - Equidistant: for each row, bisects hub-shroud chord to find the point
//   equidistant from both walls.
// - Area: F = 2*pi*r*chord where chord is measured along the gradient of psi.
// Parallelized with TBB over rows.
[[nodiscard]] Result<AreaProfile>
computeAreaProfile(const FlowSolution& sol,
                   std::span<const math::Vec2> hubPolyline,
                   std::span<const math::Vec2> shroudPolyline,
                   const PumpParams& params) noexcept;

} // namespace ggm::core
