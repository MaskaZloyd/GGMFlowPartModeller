#pragma once

#include "core/blade_params.hpp"
#include "core/blade_results.hpp"
#include "core/error.hpp"
#include "core/flow_solver.hpp"
#include "core/flow_solver_types.hpp"
#include "core/geometry.hpp"
#include "core/pump_params.hpp"

#include <optional>

namespace ggm::core {

struct BladeInputFromMeridional
{
  PumpParams pumpParams;
  MeridionalGeometry geometry;
  std::optional<FlowResults> flowResults;
  double inletAreaMm2{0.0};
  double outletAreaMm2{0.0};
};

class BladeSolver
{
public:
  [[nodiscard]] Result<BladeDesignResults>
  solve(const BladeDesignParams& bladeParams,
        const BladeInputFromMeridional& meridionalInput,
        const CancelPredicate& cancel = {}) const noexcept;
};

}
