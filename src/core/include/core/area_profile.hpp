#pragma once

#include "core/error.hpp"
#include "core/flow_solver_types.hpp"
#include "core/pump_params.hpp"
#include "math/types.hpp"

#include <span>

namespace ggm::core {

// Compute the meridional area profile F(s) along a local mean line.
//
// For each grid row:
// - a local chord is constructed between hub and shroud;
// - chord direction is taken from grad(psi), i.e. normal to local streamlines;
// - the mean point is the midpoint of this local chord;
// - the axisymmetric flow area is computed as:
//
//     F = 2*pi * integral(r ds)
//
//   For a straight chord this becomes:
//
//     F = pi * (r_hub + r_shroud) * chord_length
//
// This avoids the unstable global equidistant construction based on
// distance-to-polyline, which can fail on curved or locally concave geometry.
//
// Parallelized with TBB over rows.
[[nodiscard]] Result<AreaProfile>
computeAreaProfile(const FlowSolution& sol,
                   std::span<const math::Vec2> hubPolyline,
                   std::span<const math::Vec2> shroudPolyline,
                   const PumpParams& params);

} // namespace ggm::core
