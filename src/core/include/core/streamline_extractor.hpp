#pragma once

#include "core/flow_solver_types.hpp"

#include <vector>

namespace ggm::core {

// Generate n equidistant psi levels in (0,1), symmetric around 0.5.
[[nodiscard]] std::vector<double> equidistantLevels(int n) noexcept;

// Extract streamlines at given psi levels from the FEM solution.
// For each level, walks along grid rows and interpolates the crossing point.
[[nodiscard]] std::vector<Streamline> extractStreamlines(
    const FlowSolution& sol,
    const std::vector<double>& psiLevels) noexcept;

} // namespace ggm::core
