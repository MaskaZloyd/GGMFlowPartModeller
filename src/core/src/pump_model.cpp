#include "core/pump_model.hpp"

#include <utility>

namespace ggm::core {

PumpParams PumpModel::setParams(PumpParams newParams) noexcept {
  PumpParams old = params_;
  params_ = newParams;
  geometryValid_ = false;
  return old;
}

void PumpModel::rebuildGeometry() noexcept {
  auto result = buildGeometry(params_);
  if (result) {
    geometry_ = std::move(*result);
    geometryValid_ = true;
  } else {
    geometry_ = {};
    geometryValid_ = false;
  }
}

} // namespace ggm::core
