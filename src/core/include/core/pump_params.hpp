#pragma once

namespace ggm::core {

// Meridional geometry parameters for centrifugal pump impeller.
// All lengths in mm, angles in degrees (converted internally to radians).
// Parameter names follow the reference Python implementation (hub_shr_build.py).
struct PumpParams {
  // Hub/shroud shared geometry
  double xa = 20.0;    // axial inlet offset
  double dvt = 152.0;  // hub (shaft) bore diameter
  double d2 = 407.0;   // impeller outlet diameter

  // Hub arc radii
  double r1 = 50.0;  // first hub arc radius
  double r2 = 62.0;  // second hub arc radius

  // Shroud arc radii
  double r3 = 22.0;  // first shroud arc radius
  double r4 = 31.0;  // second shroud arc radius

  // Hub tilt angle at exit (degrees)
  double al1Deg = 2.0;

  // Shroud exit tilt angle (degrees)
  double al2Deg = -2.0;

  // Shroud inlet throat tilt angle (degrees)
  double al02Deg = -5.0;

  // First hub arc angular span (degrees)
  double be1Deg = 52.333;

  // First shroud arc raw angular span (degrees)
  double be3RawDeg = 26.383;

  // Axial width at impeller exit
  double b2 = 27.5;

  // Inlet duct diameter
  double din = 212.0;

  auto operator==(const PumpParams&) const -> bool = default;
};

} // namespace ggm::core
