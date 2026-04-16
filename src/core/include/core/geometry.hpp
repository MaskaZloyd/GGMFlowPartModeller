#pragma once

#include "core/error.hpp"
#include "core/pump_params.hpp"
#include "math/nurbs.hpp"
#include "math/types.hpp"

#include <vector>

namespace ggm::core {

struct MeridionalGeometry
{
  std::vector<math::Vec2> hubCurve;
  std::vector<math::Vec2> shroudCurve;
  math::NurbsCurve hubNurbs;
  math::NurbsCurve shroudNurbs;
};

// Build meridional geometry from pump parameters.
// Returns evaluated hub and shroud curves + their NURBS representations.
[[nodiscard]] Result<MeridionalGeometry>
buildGeometry(const PumpParams& params) noexcept;

} // namespace ggm::core
