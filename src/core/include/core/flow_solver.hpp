#pragma once

#include "core/computation_settings.hpp"
#include "core/error.hpp"
#include "core/flow_solver_types.hpp"
#include "core/geometry.hpp"
#include "core/pump_params.hpp"

#include <functional>

namespace ggm::core {

// Signature: returns true if the operation should be cancelled.
// Called between pipeline stages. May also be called inside FEM assembly.
using CancelPredicate = std::function<bool()>;

class FlowSolver {
public:
  FlowSolver() = default;

  void setConfig(ComputationSettings config) noexcept { config_ = config; }
  [[nodiscard]] const ComputationSettings& config() const noexcept { return config_; }

  // Run the full pipeline: resample -> grid -> FEM -> streamlines -> area.
  // If isCancelled is set and returns true between stages, returns CoreError::Cancelled.
  [[nodiscard]] Result<FlowResults> solve(const MeridionalGeometry& geom,
                                          const PumpParams& params,
                                          const CancelPredicate& isCancelled = {}) noexcept;

private:
  ComputationSettings config_;
};

} // namespace ggm::core
