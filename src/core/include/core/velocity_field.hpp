#pragma once

#include "core/error.hpp"
#include "core/flow_solver_types.hpp"
#include "math/types.hpp"

#include <span>
#include <vector>

namespace ggm::core {

// Compute velocities along streamlines based on the flow solution.
// Uses nodal gradient recovery for smooth interpolation.
// velocity = (v_z, v_r), point = (z, r).
[[nodiscard]] Result<std::vector<StreamlineVelocity>>
computeStreamlineVelocities(const FlowSolution& sol,
                            std::span<const Streamline> streamlines,
                            double flowRateM3s);

} // namespace ggm::core
