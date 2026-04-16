#pragma once

namespace ggm::core {

struct ComputationSettings
{
  int nurbsEvalPoints = 400; // points for NURBS curve display
  int nh = 200;              // arc-length stations along curves (stream-wise)
  int m = 80;                // transverse grid stations across gap
  int streamlineCount = 5;   // interior streamlines to extract

  auto operator==(const ComputationSettings&) const -> bool = default;
};

} // namespace ggm::core
