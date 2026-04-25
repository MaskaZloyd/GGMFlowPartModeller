#pragma once

#include "core/error.hpp"
#include "core/flow_solver_types.hpp"

#include <span>
#include <vector>

namespace ggm::core {

/// Compute meridional velocities along streamlines.
///
/// Mathematics:
///   Psi = Q / (2*pi) * psi
///
///   v_z =  Q / (2*pi*r) * dpsi/dr
///   v_r = -Q / (2*pi*r) * dpsi/dz
///
/// Coordinates:
///   point = (z, r)
///
/// Velocity:
///   velocity = (v_z, v_r)
///
/// Units:
///   flowRateM3s        - m^3/s
///   lengthUnitToMeters - conversion factor from project geometry units to meters.
///                        Use 1e-3 if geometry is in millimeters.
///                        Use 1.0  if geometry is already in meters.
[[nodiscard]] Result<std::vector<StreamlineVelocity>>
computeStreamlineVelocities(const FlowSolution& sol,
                            std::span<const Streamline> streamlines,
                            double flowRateM3s,
                            double lengthUnitToMeters);

}
