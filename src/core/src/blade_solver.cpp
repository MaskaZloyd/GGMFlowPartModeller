#include "core/blade_solver.hpp"

#include "core/blade_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ggm::core {

namespace {

constexpr double G = 9.80665;
constexpr double WATER_DENSITY = 1000.0;
constexpr double MM2_TO_M2 = 1.0e-6;
constexpr double MM_TO_M = 1.0e-3;
constexpr int SECTION_COUNT = 96;
constexpr int PERFORMANCE_POINTS = 25;
constexpr double SIGMA_Q_SENSITIVITY = 0.35;
constexpr double DEFAULT_SHOCK_LOSS_FRACTION = 0.10;

[[nodiscard]] bool
finitePositive(double value) noexcept
{
  return std::isfinite(value) && value > 0.0;
}

[[nodiscard]] double
degToRad(double value) noexcept
{
  return value * std::numbers::pi / 180.0;
}

[[nodiscard]] double
radToDeg(double value) noexcept
{
  return value * 180.0 / std::numbers::pi;
}

[[nodiscard]] double
cotDeg(double betaDeg) noexcept
{
  return 1.0 / std::tan(degToRad(betaDeg));
}

[[nodiscard]] double
clampBeta(double betaDeg) noexcept
{
  return std::clamp(betaDeg, 5.0, 85.0);
}

[[nodiscard]] double
fallbackInletAreaMm2(const PumpParams& params) noexcept
{
  const double outer = params.din * params.din;
  const double inner = params.dvt * params.dvt;
  return 0.25 * std::numbers::pi * std::max(0.0, outer - inner);
}

[[nodiscard]] double
fallbackOutletAreaMm2(const PumpParams& params) noexcept
{
  return std::numbers::pi * params.d2 * params.b2;
}

[[nodiscard]] double
effectiveBlockageFactor(double value) noexcept
{
  if (!std::isfinite(value) || value <= 0.0) {
    return 1.0;
  }
  return std::clamp(value, 0.50, 1.0);
}

[[nodiscard]] double
computeSlipFactor(const BladeDesignParams& params) noexcept
{
  if (!params.autoSlipFactor) {
    return std::clamp(params.slipFactor, 0.5, 0.98);
  }

  const double z = static_cast<double>(std::max(params.bladeCount, 2));
  const double beta2 = degToRad(clampBeta(params.beta2Deg));

  return std::clamp(1.0 - std::sqrt(std::sin(beta2)) / std::sqrt(z), 0.5, 0.98);
}

[[nodiscard]] double
flowSlipFactor(double sigma0, double qM3s, double designQM3s) noexcept
{
  if (!finitePositive(designQM3s)) {
    return std::clamp(sigma0, 0.5, 0.98);
  }

  const double flowRatio = qM3s / designQM3s;
  const double deviation = flowRatio - 1.0;
  const double sigma = sigma0 * (1.0 - SIGMA_Q_SENSITIVITY * deviation * deviation);
  return std::clamp(sigma, 0.5, 0.98);
}

[[nodiscard]] double
omegaFromRpm(double rpm) noexcept
{
  return 2.0 * std::numbers::pi * rpm / 60.0;
}

[[nodiscard]] double
peripheralSpeed(double radiusMm, double rpm) noexcept
{
  return omegaFromRpm(rpm) * radiusMm * MM_TO_M;
}

[[nodiscard]] double
meridionalVelocity(double qM3s, double areaMm2) noexcept
{
  return qM3s / std::max(areaMm2 * MM2_TO_M2, 1.0e-12);
}

[[nodiscard]] double
hydraulicLossCoefficient(double lossK) noexcept
{
  return std::max(0.0, lossK);
}

[[nodiscard]] double
shockLossCoefficient(double lossK, double designHeadM, double designQM3s) noexcept
{
  const double hydraulicK = hydraulicLossCoefficient(lossK);
  if (hydraulicK > 0.0) {
    return hydraulicK;
  }
  if (!finitePositive(designHeadM) || !finitePositive(designQM3s)) {
    return 0.0;
  }
  return DEFAULT_SHOCK_LOSS_FRACTION * designHeadM / (designQM3s * designQM3s);
}

[[nodiscard]] double
headLoss(double kHyd, double kShock, double qM3s, double designQM3s) noexcept
{
  const double dq = qM3s - designQM3s;
  return kHyd * qM3s * qM3s + kShock * dq * dq;
}

[[nodiscard]] BladeVelocityTriangle
makeTriangle(double radiusMm,
             double rpm,
             double qM3s,
             double areaMm2,
             double cuMs,
             double betaDeg)
{
  const double u = peripheralSpeed(radiusMm, rpm);
  const double cm = meridionalVelocity(qM3s, areaMm2);
  const double wu = u - cuMs;

  return BladeVelocityTriangle{
    .radiusMm = radiusMm,
    .peripheralSpeedMs = u,
    .meridionalVelocityMs = cm,
    .circumferentialVelocityMs = cuMs,
    .relativeCircumferentialMs = wu,
    .relativeSpeedMs = std::hypot(cm, wu),
    .betaDeg = betaDeg,
  };
}

[[nodiscard]] BladePerformancePoint
performanceAt(double qM3s,
              double r2Mm,
              double rpm,
              double outletAreaMm2,
              double beta2Deg,
              double sigma0,
              double designQM3s,
              double kHyd,
              double kShock) noexcept
{
  const double u2 = peripheralSpeed(r2Mm, rpm);
  const double cm2 = meridionalVelocity(qM3s, outletAreaMm2);

  const double cu2Ideal = u2 - cm2 * cotDeg(beta2Deg);
  const double sigma = flowSlipFactor(sigma0, qM3s, designQM3s);
  const double cu2Slip = sigma * cu2Ideal;

  const double headEuler = u2 * cu2Ideal / G;
  const double headSlip = u2 * cu2Slip / G;
  const double loss = headLoss(kHyd, kShock, qM3s, designQM3s);
  const double headReal = headSlip - loss;

  return BladePerformancePoint{
    .qM3s = qM3s,
    .qM3h = qM3s * 3600.0,
    .cm2 = cm2,
    .cu2Ideal = cu2Ideal,
    .cu2Slip = cu2Slip,
    .headEulerM = headEuler,
    .headSlipM = headSlip,
    .headLossM = loss,
    .headRealM = headReal,
    .hydraulicPowerKW = WATER_DENSITY * G * qM3s * std::max(0.0, headReal) / 1000.0,
  };
}

[[nodiscard]] double
autoInletBetaDeg(double cm1, double u1, double cu1) noexcept
{
  const double wu1 = u1 - cu1;
  if (!std::isfinite(wu1) || wu1 <= 0.0) {
    return 85.0;
  }

  return clampBeta(radToDeg(std::atan2(cm1, wu1)));
}

[[nodiscard]] double
autoOutletBetaDeg(BladeDesignParams params,
                  double cm2,
                  double u2,
                  double designQ,
                  double designHeadM) noexcept
{
  double beta2 = clampBeta(params.beta2Deg);

  // sigma depends on beta2, while beta2 depends on sigma.
  // A few fixed-point iterations are enough for this engineering model.
  for (int iter = 0; iter < 4; ++iter) {
    params.beta2Deg = beta2;
    const double sigma = computeSlipFactor(params);

    const double kHyd = hydraulicLossCoefficient(params.hydraulicLossK);
    const double kShock = shockLossCoefficient(params.hydraulicLossK, designHeadM, designQ);
    const double lossesDesign = headLoss(kHyd, kShock, designQ, designQ);
    const double targetSlipHead = std::max(0.0, designHeadM + lossesDesign);

    const double cu2InfRequired =
      G * targetSlipHead / std::max(sigma * u2, 1.0e-9);

    const double wu2 = u2 - cu2InfRequired;
    if (!std::isfinite(wu2) || wu2 <= 0.0) {
      return 85.0;
    }

    beta2 = clampBeta(radToDeg(std::atan2(cm2, wu2)));
  }

  return beta2;
}

}

Result<BladeDesignResults>
BladeSolver::solve(const BladeDesignParams& bladeParams,
                   const BladeInputFromMeridional& meridionalInput,
                   const CancelPredicate& cancel) const noexcept
{
  if (cancel && cancel()) {
    return std::unexpected(CoreError::Cancelled);
  }

  if (bladeParams.latticeType != BladeLatticeType::Cylindrical) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  if (bladeParams.bladeCount < 2 || !finitePositive(bladeParams.rpm)) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  BladeDesignParams effective = bladeParams;
  const auto& pump = meridionalInput.pumpParams;

  const double r1 = 0.5 * pump.din;
  const double r2 = 0.5 * pump.d2;

  if (!finitePositive(r1) || !finitePositive(r2) || r2 <= r1 || pump.dvt >= pump.din ||
      !finitePositive(effective.flowRateM3s)) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  double inletArea = meridionalInput.inletAreaMm2 > 0.0 ? meridionalInput.inletAreaMm2
                                                       : fallbackInletAreaMm2(pump);
  double outletArea = meridionalInput.outletAreaMm2 > 0.0 ? meridionalInput.outletAreaMm2
                                                         : fallbackOutletAreaMm2(pump);

  if (meridionalInput.flowResults) {
    const auto& profile = meridionalInput.flowResults->areaProfile;

    if (profile.f1 > 0.0) {
      inletArea = profile.f1;
    }

    if (profile.f2 > 0.0) {
      outletArea = profile.f2;
    }
  }

  if (!finitePositive(inletArea) || !finitePositive(outletArea)) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  const double blockageFactor = effectiveBlockageFactor(effective.blockageFactor);
  const double effectiveOutletArea = outletArea * blockageFactor;

  if (!finitePositive(effectiveOutletArea)) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  const double u1 = peripheralSpeed(r1, effective.rpm);
  const double u2 = peripheralSpeed(r2, effective.rpm);

  const double cm1 = meridionalVelocity(effective.flowRateM3s, inletArea);
  const double cm2 = meridionalVelocity(effective.flowRateM3s, effectiveOutletArea);

  const double cu1 = 0.0;

  if (effective.autoInletAngle) {
    effective.beta1Deg = autoInletBetaDeg(cm1, u1, cu1);
  }

  if (effective.autoOutletAngle) {
    effective.beta2Deg =
      autoOutletBetaDeg(effective, cm2, u2, effective.flowRateM3s, effective.designHeadM);
  }

  if (effective.beta1Deg < 5.0 || effective.beta1Deg > 85.0 || effective.beta2Deg < 5.0 ||
      effective.beta2Deg > 85.0 || !finitePositive(effective.s1Mm) ||
      !finitePositive(effective.s2Mm) || !finitePositive(effective.sMaxMm)) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  BladeDesignResults result;
  result.inletRadiusMm = r1;
  result.outletRadiusMm = r2;
  result.inletAreaMm2 = inletArea;
  result.outletAreaMm2 = effectiveOutletArea;
  result.slipFactor = computeSlipFactor(effective);
  const double sigma0 = result.slipFactor;
  const double kHyd = hydraulicLossCoefficient(effective.hydraulicLossK);
  const double kShock =
    shockLossCoefficient(effective.hydraulicLossK, effective.designHeadM, effective.flowRateM3s);

  result.sections.reserve(SECTION_COUNT);

  double phi = 0.0;
  double prevR = r1;
  double prevIntegrand = cotDeg(evaluateBladeAngleDeg(effective, 0.0)) / prevR;

  for (int i = 0; i < SECTION_COUNT; ++i) {
    if (cancel && cancel()) {
      return std::unexpected(CoreError::Cancelled);
    }

    const double t = static_cast<double>(i) / static_cast<double>(SECTION_COUNT - 1);
    const double r = r1 + (r2 - r1) * t;
    const double beta = evaluateBladeAngleDeg(effective, t);
    const double thickness = evaluateBladeThicknessMm(effective, t);
    const double pitch = 2.0 * std::numbers::pi * r / static_cast<double>(effective.bladeCount);
    const double blockage = thickness / std::max(pitch, 1.0e-9);

    if (i > 0) {
      const double integrand = cotDeg(beta) / r;
      phi += 0.5 * (prevIntegrand + integrand) * (r - prevR);
      prevIntegrand = integrand;
      prevR = r;
    }

    if (!std::isfinite(phi) || !std::isfinite(beta) || !std::isfinite(thickness) ||
        thickness <= 0.0 || thickness >= pitch) {
      return std::unexpected(CoreError::InvalidParameter);
    }

    result.sections.push_back(BladeSectionSample{
      .rMm = r,
      .phiRad = phi,
      .uMm = r * phi,
      .betaDeg = beta,
      .thicknessMm = thickness,
      .pitchMm = pitch,
      .blockage = blockage,
    });
  }

  auto contour = buildCylindricalBladeContour(result.sections);
  if (!contour) {
    return std::unexpected(contour.error());
  }

  result.singleBlade = *contour;

  result.allBlades.reserve(static_cast<std::size_t>(effective.bladeCount));
  for (int i = 0; i < effective.bladeCount; ++i) {
    const double angle = 2.0 * std::numbers::pi * static_cast<double>(i) /
                         static_cast<double>(effective.bladeCount);
    result.allBlades.push_back(rotateBladeContour(result.singleBlade, angle));
  }

  const double cu2InfDesign = u2 - cm2 * cotDeg(effective.beta2Deg);
  const double sigmaDesign = flowSlipFactor(sigma0, effective.flowRateM3s, effective.flowRateM3s);
  const double cu2SlipDesign = sigmaDesign * cu2InfDesign;

  result.inletTriangle =
    makeTriangle(r1, effective.rpm, effective.flowRateM3s, inletArea, cu1, effective.beta1Deg);

  result.outletTriangle =
    makeTriangle(r2,
                 effective.rpm,
                 effective.flowRateM3s,
                 effectiveOutletArea,
                 cu2SlipDesign,
                 effective.beta2Deg);

  result.performanceCurve.reserve(PERFORMANCE_POINTS);

  const double qMax = std::max(effective.flowRateM3s * 1.6, 1.0e-6);
  for (int i = 0; i < PERFORMANCE_POINTS; ++i) {
    const double q = qMax * static_cast<double>(i) / static_cast<double>(PERFORMANCE_POINTS - 1);

    result.performanceCurve.push_back(performanceAt(q,
                                                    r2,
                                                    effective.rpm,
                                                    effectiveOutletArea,
                                                    effective.beta2Deg,
                                                    sigma0,
                                                    effective.flowRateM3s,
                                                    kHyd,
                                                    kShock));
  }

  if (result.slipFactor < 0.7) {
    result.diagnostics.push_back(
      "Low slip factor: consider increasing blade count or reducing outlet angle.");
  }

  const auto maxBlockage =
    std::max_element(result.sections.begin(),
                     result.sections.end(),
                     [](const auto& lhs, const auto& rhs) {
                       return lhs.blockage < rhs.blockage;
                     });

  if (maxBlockage != result.sections.end() && maxBlockage->blockage > 0.12) {
    result.diagnostics.push_back("High blade blockage relative to pitch.");
  }

  if (cu2InfDesign <= 0.0) {
    result.diagnostics.push_back(
      "Outlet whirl is non-positive at design flow: selected beta2 or flow rate is outside the valid range.");
  }

  if (effective.designHeadM > 0.0) {
    const double designHeadReal = performanceAt(effective.flowRateM3s,
                                                r2,
                                                effective.rpm,
                                                effectiveOutletArea,
                                                effective.beta2Deg,
                                                sigma0,
                                                effective.flowRateM3s,
                                                kHyd,
                                                kShock)
                                    .headRealM;
    const double headError = designHeadReal - effective.designHeadM;

    if (std::abs(headError) > std::max(1.0, 0.05 * effective.designHeadM)) {
      result.diagnostics.push_back(
        "Design head was not matched within tolerance: check D2, rpm, beta2 limits, slip factor and loss coefficient.");
    }
  }

  if (effective.beta2Deg > 84.0) {
    result.diagnostics.push_back(
      "Outlet angle reached upper beta limit: requested head may be too high for selected D2/rpm.");
  }

  if (effective.beta2Deg < 5.5) {
    result.diagnostics.push_back(
      "Outlet angle reached lower beta limit: requested head may be too low for selected D2/rpm.");
  }

  return result;
}

}
