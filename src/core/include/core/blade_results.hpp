#pragma once

#include <string>
#include <vector>

namespace ggm::core {

struct BladeSectionSample
{
  double rMm{0.0};
  double phiRad{0.0};
  double uMm{0.0};
  double betaDeg{0.0};
  double thicknessMm{0.0};
  double pitchMm{0.0};
  double blockage{0.0};
};

struct BladePlanPoint
{
  double xMm{0.0};
  double yMm{0.0};
};

struct BladeContour
{
  std::vector<BladePlanPoint> centerline;
  std::vector<BladePlanPoint> pressureSide;
  std::vector<BladePlanPoint> suctionSide;
  std::vector<BladePlanPoint> closedContour;
};

struct BladeVelocityTriangle
{
  double radiusMm{0.0};
  double peripheralSpeedMs{0.0};
  double meridionalVelocityMs{0.0};
  double circumferentialVelocityMs{0.0};
  double relativeCircumferentialMs{0.0};
  double relativeSpeedMs{0.0};
  double betaDeg{0.0};
};

struct BladePerformancePoint
{
  double qM3s{0.0};
  double qM3h{0.0};
  double cm2{0.0};
  double cu2Ideal{0.0};
  double cu2Slip{0.0};
  double headEulerM{0.0};
  double headSlipM{0.0};
  double headLossM{0.0};
  double headRealM{0.0};
  double hydraulicPowerKW{0.0};
};

struct BladeDesignResults
{
  double inletRadiusMm{0.0};
  double outletRadiusMm{0.0};
  double inletAreaMm2{0.0};
  double outletAreaMm2{0.0};
  double slipFactor{0.0};

  BladeVelocityTriangle inletTriangle;
  BladeVelocityTriangle outletTriangle;

  std::vector<BladeSectionSample> sections;
  BladeContour singleBlade;
  std::vector<BladeContour> allBlades;
  std::vector<BladePerformancePoint> performanceCurve;
  std::vector<std::string> diagnostics;
};

}
