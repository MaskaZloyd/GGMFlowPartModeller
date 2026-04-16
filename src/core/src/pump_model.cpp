#include "core/pump_model.hpp"

#include <utility>

namespace ggm::core {

PumpParams
PumpModel::setParams(PumpParams newParams) noexcept
{
  PumpParams old = params_;
  params_ = newParams;
  geometryValid_ = false;
  return old;
}

Result<void>
PumpModel::rebuildGeometry() noexcept
{
  auto result = buildGeometry(params_);
  if (!result) {
    geometry_ = {};
    geometryValid_ = false;
    return std::unexpected(result.error());
  }
  geometry_ = std::move(*result);
  geometryValid_ = true;
  return {};
}

} // namespace ggm::core
