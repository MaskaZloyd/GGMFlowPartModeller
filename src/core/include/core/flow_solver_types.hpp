#pragma once

#include "math/types.hpp"

#include <array>
#include <vector>

namespace ggm::core {

struct StripGrid
{
  int nh = 0;
  int m = 0;
  std::vector<math::Vec2> nodes; // flat [i*m + j], (z,r)
  std::vector<std::array<int, 3>> triangles;
  std::vector<int> hubNodes;     // j==0 boundary
  std::vector<int> shroudNodes;  // j==m-1 boundary
};

struct FlowSolution
{
  StripGrid grid;
  std::vector<double> psi; // nodal streamfunction, size == nh*m
};

struct Streamline
{
  std::vector<math::Vec2> points;
  double psiLevel = 0.0;
};

struct VelocitySample
{
  math::Vec2 point{0.0, 0.0};    // (z, r)
  math::Vec2 velocity{0.0, 0.0}; // (v_z, v_r)
  double speed{0.0};             // sqrt(v_z^2 + v_r^2)
  double radius{0.0};            // r
  double arcLength{0.0};         // distance along streamline
};

struct StreamlineVelocity
{
  double psiLevel{0.0};
  std::vector<VelocitySample> samples;
};

struct AreaProfile
{
  std::vector<math::Vec2> midPoints; // equidistant curve (z,r)
  std::vector<double> chordLengths;  // physical chord along grad(psi)
  std::vector<double> flowAreas;     // 2*pi*r*chord per station
  std::vector<double> arcLengths;    // cumulative arc length along midline
  double f1 = 0.0;                   // inlet area (mm^2)
  double f2 = 0.0;                   // outlet area (mm^2)
};

struct FlowResults
{
  FlowSolution solution;
  std::vector<Streamline> streamlines;
  std::vector<StreamlineVelocity> velocities;
  AreaProfile areaProfile;
};

} // namespace ggm::core
