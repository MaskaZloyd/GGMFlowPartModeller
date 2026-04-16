#pragma once

#include "core/error.hpp"
#include "core/flow_solver_types.hpp"

namespace ggm::core {

// Solve the axisymmetric streamfunction PDE on the strip grid:
//   div(grad(psi)) - (1/r) * dpsi/dr = 0
// BCs: psi=0 on hub, psi=1 on shroud.
// Assembly is parallelized with TBB.
[[nodiscard]] Result<FlowSolution>
solveFem(StripGrid grid) noexcept;

} // namespace ggm::core
