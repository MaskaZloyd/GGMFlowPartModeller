#pragma once

namespace ggm::core {

struct ComputationSettings
{
  int nurbsEvalPoints = 100; ///< points for NURBS curve display
  int nh = 200;              ///< arc-length stations along curves (stream-wise)
  int m = 10;                ///< transverse grid stations across gap
  int streamlineCount = 7;   ///< streamlines to extract and display

  auto operator==(const ComputationSettings&) const -> bool = default;
};

}
