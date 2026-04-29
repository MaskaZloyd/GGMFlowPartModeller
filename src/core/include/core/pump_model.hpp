#pragma once

#include "core/blade_params.hpp"
#include "core/computation_settings.hpp"
#include "core/error.hpp"
#include "core/geometry.hpp"
#include "core/pump_params.hpp"

namespace ggm::core {

class PumpModel
{
public:
  PumpModel() = default;

  [[nodiscard]] const PumpParams& params() const noexcept { return params_; }
  [[nodiscard]] const MeridionalGeometry& geometry() const noexcept { return geometry_; }
  [[nodiscard]] bool geometryValid() const noexcept { return geometryValid_; }

  [[nodiscard]] const ComputationSettings& compSettings() const noexcept { return compSettings_; }
  [[nodiscard]] const BladeDesignParams& bladeDesignParams() const noexcept
  {
    return bladeDesignParams_;
  }

  /// Set new parameters. Returns the old params (for undo).
  PumpParams setParams(PumpParams newParams) noexcept;

  void setCompSettings(ComputationSettings settings) noexcept { compSettings_ = settings; }
  void setBladeDesignParams(BladeDesignParams params) noexcept { bladeDesignParams_ = params; }
  void setFlowRateM3s(double flowRateM3s) noexcept { params_.qM3s = flowRateM3s; }

  /// Rebuild geometry (fast — NURBS eval only). Safe to call on UI thread.
  [[nodiscard]] Result<void> rebuildGeometry() noexcept;

private:
  PumpParams params_;
  MeridionalGeometry geometry_;
  ComputationSettings compSettings_;
  BladeDesignParams bladeDesignParams_;
  bool geometryValid_ = false;
};

}
