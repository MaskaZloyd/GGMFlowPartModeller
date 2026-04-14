#include "core/geometry.hpp"

#include "math/bezier.hpp"
#include "math/nurbs.hpp"

#include <Eigen/LU>

#include <array>
#include <cmath>
#include <numbers>

namespace ggm::core {

namespace {

constexpr double DEG_TO_RAD = std::numbers::pi / 180.0;

double vecAngle(const math::Vec2& vec) noexcept {
  return std::atan2(vec.y(), vec.x());
}

} // namespace

Result<MeridionalGeometry> buildGeometry(const PumpParams& params) noexcept {
  // Convert angles from degrees to radians
  double al1 = params.al1Deg * DEG_TO_RAD;
  double al2 = params.al2Deg * DEG_TO_RAD;
  double al02 = params.al02Deg * DEG_TO_RAD;
  double be1 = params.be1Deg * DEG_TO_RAD;
  double be2 = std::numbers::pi / 2.0 - be1 + al1;
  double be3Raw = params.be3RawDeg * DEG_TO_RAD;
  double be3 = be3Raw - std::abs(al02);
  double be4 = std::numbers::pi / 2.0 - be3 + al2;

  // ---- HUB geometry ----
  math::Vec2 hubP0{0.0, params.dvt / 2.0};
  math::Vec2 hubP1 = hubP0 + math::Vec2{params.xa, 0.0};

  // Arc 1: center above P1
  math::Vec2 hubO1 = hubP1 + math::Vec2{0.0, params.r1};
  double hubArc1Start = -std::numbers::pi / 2.0;
  auto hubArc1 = math::arcToBezier(hubO1, params.r1, hubArc1Start, hubArc1Start + be1);
  math::Vec2 hubP2 = hubArc1.p2;

  // Arc 2: G1-continuous with arc 1
  math::Vec2 radialDir = (hubO1 - hubP2).normalized();
  math::Vec2 hubO2 = hubP2 + params.r2 * radialDir;
  double hubArc2Start = vecAngle(hubP2 - hubO2);
  auto hubArc2 = math::arcToBezier(hubO2, params.r2, hubArc2Start, hubArc2Start + be2);
  math::Vec2 hubP3 = hubArc2.p2;

  // Exit line: from P3 to outlet radius
  double alphaStar = -std::numbers::pi / 2.0 + al1;
  math::Vec2 exitDir{std::cos(alphaStar), std::sin(alphaStar)};
  double exitParam = (params.d2 / 2.0 - hubP3.y()) / exitDir.y();
  math::Vec2 hubP4 = hubP3 + exitParam * exitDir;

  auto hubSeg0 = math::segToBezier(hubP0, hubP1);
  auto hubSeg3 = math::segToBezier(hubP3, hubP4);

  std::array<math::ArcBezier, 4> hubSegments = {hubSeg0, hubArc1, hubArc2, hubSeg3};
  auto hubNurbs = math::buildFromSegments(hubSegments);
  auto hubCurve = math::evaluate(hubNurbs, 1500);

  // ---- SHROUD geometry ----
  math::Vec2 shrP5 = hubP4 - math::Vec2{params.b2, 0.0};
  double alpha2Star = -std::numbers::pi / 2.0 + al2;
  math::Vec2 exitDir5{std::cos(alpha2Star), std::sin(alpha2Star)};

  // Intersection point IS on shroud inlet line
  math::Vec2 intersectionPt =
      shrP5 + ((params.din / 2.0 - shrP5.y()) / exitDir5.y()) * exitDir5;

  // Solve 2x2 system for arc centers
  math::Vec2 vec1{1.0, 0.0};
  math::Vec2 vec2Shr{std::cos(be3 + be4), std::sin(be3 + be4)};
  math::Vec2 normal1{0.0, 1.0};
  math::Vec2 normal2{-vec2Shr.y(), vec2Shr.x()};
  math::Vec2 wVec{-std::sin(be3), std::cos(be3)};

  Eigen::Matrix2d matA;
  matA.col(0) = -vec1;
  matA.col(1) = vec2Shr;
  math::Vec2 rhs = (params.r4 - params.r3) * wVec + params.r3 * normal1 - params.r4 * normal2;

  double det = matA.determinant();
  if (std::abs(det) < 1e-12) {
    return std::unexpected(CoreError::GeometryBuildFailed);
  }
  math::Vec2 solution = matA.inverse() * rhs;
  double dist1 = solution.x();
  double dist2 = solution.y();

  math::Vec2 shrP8 = intersectionPt + dist1 * vec1;
  math::Vec2 shrP6 = intersectionPt + dist2 * vec2Shr;
  math::Vec2 shrO3 = shrP8 + params.r3 * normal1;
  math::Vec2 shrO4 = shrP6 + params.r4 * normal2;

  // Shroud inlet point P9
  math::Vec2 shrP9;
  if (std::abs(al02) < 1e-12) {
    shrP9 = math::Vec2{0.0, params.din / 2.0};
  } else {
    // Rotate (0, -1) by -al02 to get tangent at P8
    double cosAl02 = std::cos(-al02);
    double sinAl02 = std::sin(-al02);
    math::Vec2 vecDown{0.0, -1.0};
    math::Vec2 rotated{vecDown.x() * cosAl02 - vecDown.y() * sinAl02,
                       vecDown.x() * sinAl02 + vecDown.y() * cosAl02};
    shrP8 = shrO3 + params.r3 * rotated;
    math::Vec2 dir9{std::cos(al02), std::sin(al02)};
    double param9 = -shrP8.x() / dir9.x();
    shrP9 = shrP8 + param9 * dir9;
  }

  // Build shroud segments (inlet to exit order)
  auto shrSeg0 = math::segToBezier(shrP9, shrP8);
  double shrArc1Start = vecAngle(shrP8 - shrO3);
  auto shrArc1 = math::arcToBezier(shrO3, params.r3, shrArc1Start, shrArc1Start + be3);
  // P7 is the junction between arcs — use constraint formula from Python, not arc endpoint
  math::Vec2 shrP7 = shrO3 - params.r3 * wVec;
  double shrArc2Start = vecAngle(shrP7 - shrO4);
  auto shrArc2 = math::arcToBezier(shrO4, params.r4, shrArc2Start, shrArc2Start + be4);
  auto shrSeg3 = math::segToBezier(shrP6, shrP5);

  std::array<math::ArcBezier, 4> shroudSegments = {shrSeg0, shrArc1, shrArc2, shrSeg3};
  auto shroudNurbs = math::buildFromSegments(shroudSegments);
  auto shroudCurve = math::evaluate(shroudNurbs, 1500);

  return MeridionalGeometry{
      .hubCurve = std::move(hubCurve),
      .shroudCurve = std::move(shroudCurve),
      .hubNurbs = std::move(hubNurbs),
      .shroudNurbs = std::move(shroudNurbs),
  };
}

} // namespace ggm::core
