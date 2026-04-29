#pragma once

namespace ggm::core {

enum class BladeLatticeType
{
  Cylindrical,
  Spatial,
};

enum class BladeAngleLaw
{
  Constant,
  Linear,
  Quadratic,
  Bezier,
};

enum class BladeThicknessLaw
{
  Constant,
  Linear,
  Parabolic,
  Bezier,
};

struct BladeDesignParams
{
  BladeLatticeType latticeType{BladeLatticeType::Cylindrical};
  int bladeCount{6};
  double flowRateM3s{0.05};
  double rpm{1450.0};
  double designHeadM{25.0};

  bool autoInletAngle{true};
  bool autoOutletAngle{true};
  double beta1Deg{20.0};
  double beta2Deg{28.0};
  BladeAngleLaw angleLaw{BladeAngleLaw::Linear};

  double s1Mm{4.0};
  double s2Mm{3.0};
  double sMaxMm{6.0};
  BladeThicknessLaw thicknessLaw{BladeThicknessLaw::Parabolic};

  double leadingEdgeBulgeMm{4.0};
  double trailingEdgeBulgeMm{3.0};
  double blockageFactor{1.0};

  double slipFactor{0.86};
  bool autoSlipFactor{true};

  double hydraulicLossK{0.0};

  auto operator==(const BladeDesignParams&) const -> bool = default;
};

}
