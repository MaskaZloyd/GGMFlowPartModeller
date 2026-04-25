#include "core/geometry.hpp"

#include "math/bezier.hpp"
#include "math/nurbs.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <utility>

#include <Eigen/LU>

namespace ggm::core {

namespace {

constexpr double DEG_TO_RAD = std::numbers::pi / 180.0;
constexpr double DET_TOL = 1e-12;
constexpr double ANGLE_TOL = 1e-12;
constexpr double JOIN_TOL = 1e-7;

double
cross2d(const math::Vec2& a, const math::Vec2& b) noexcept
{
  return a.x() * b.y() - a.y() * b.x();
}

double
vecAngle(const math::Vec2& vec) noexcept
{
  return std::atan2(vec.y(), vec.x());
}

math::Vec2
unitFromAngle(double angle) noexcept
{
  return {std::cos(angle), std::sin(angle)};
}

math::Vec2
leftNormal(const math::Vec2& tangent) noexcept
{
  return {-tangent.y(), tangent.x()};
}

std::optional<math::Vec2>
intersectLines(const math::Vec2& p0,
               const math::Vec2& d0,
               const math::Vec2& p1,
               const math::Vec2& d1) noexcept
{
  const double det = cross2d(d0, d1);

  if (std::abs(det) < DET_TOL) {
    return std::nullopt;
  }

  const double t = cross2d(p1 - p0, d1) / det;
  return p0 + t * d0;
}

bool
isFiniteVec(const math::Vec2& v) noexcept
{
  return std::isfinite(v.x()) && std::isfinite(v.y());
}

}

Result<MeridionalGeometry>
buildGeometry(const PumpParams& params)
{
  const double al1 = params.al1Deg * DEG_TO_RAD;
  const double al2 = params.al2Deg * DEG_TO_RAD;
  const double al02 = params.al02Deg * DEG_TO_RAD;
  const double be1 = params.be1Deg * DEG_TO_RAD;

  const double be2 = std::numbers::pi / 2.0 - be1 + al1;

  const double be3Junction = params.be3RawDeg * DEG_TO_RAD;
  const double be3Sweep = be3Junction - al02;

  const double outletShroudAngle = std::numbers::pi / 2.0 + al2;
  const double be4 = outletShroudAngle - be3Junction;

  if (be1 <= 0.0 || be2 <= 0.0 || be3Sweep <= 0.0 || be4 <= 0.0) {
    return std::unexpected(CoreError::GeometryBuildFailed);
  }

  const math::Vec2 hubP0{0.0, params.dvt / 2.0};
  const math::Vec2 hubP1 = hubP0 + math::Vec2{params.xa, 0.0};

  const math::Vec2 hubO1 = hubP1 + math::Vec2{0.0, params.r1};
  const double hubArc1Start = -std::numbers::pi / 2.0;
  const auto hubArc1 = math::arcToBezier(hubO1, params.r1, hubArc1Start, hubArc1Start + be1);

  const math::Vec2 hubP2 = hubArc1.p2;

  const math::Vec2 radialDir = (hubO1 - hubP2).normalized();
  const math::Vec2 hubO2 = hubP2 + params.r2 * radialDir;

  const double hubArc2Start = vecAngle(hubP2 - hubO2);
  const auto hubArc2 = math::arcToBezier(hubO2, params.r2, hubArc2Start, hubArc2Start + be2);

  const math::Vec2 hubP3 = hubArc2.p2;

  const double hubExitAngle = -std::numbers::pi / 2.0 + al1;
  const math::Vec2 hubExitDir = unitFromAngle(hubExitAngle);

  if (std::abs(hubExitDir.y()) < DET_TOL) {
    return std::unexpected(CoreError::GeometryBuildFailed);
  }

  const double hubExitParam = (params.d2 / 2.0 - hubP3.y()) / hubExitDir.y();

  const math::Vec2 hubP4 = hubP3 + hubExitParam * hubExitDir;

  const auto hubSeg0 = math::segToBezier(hubP0, hubP1);
  const auto hubSeg3 = math::segToBezier(hubP3, hubP4);

  const std::array<math::ArcBezier, 4> hubSegments = {hubSeg0, hubArc1, hubArc2, hubSeg3};

  auto hubNurbs = math::buildFromSegments(hubSegments);
  auto hubCurve = math::evaluate(hubNurbs, 1500);

  const math::Vec2 shrP9{0.0, params.din / 2.0};
  const math::Vec2 shrP5 = hubP4 - math::Vec2{params.b2, 0.0};

  const math::Vec2 inletTangent = unitFromAngle(al02);
  const math::Vec2 outletTangent = unitFromAngle(outletShroudAngle);

  const math::Vec2 inletNormal = leftNormal(inletTangent);
  const math::Vec2 outletNormal = leftNormal(outletTangent);

  const math::Vec2 junctionNormal = leftNormal(unitFromAngle(be3Junction));

  const auto intersectionOpt = intersectLines(shrP9, inletTangent, shrP5, outletTangent);

  if (!intersectionOpt.has_value()) {
    return std::unexpected(CoreError::GeometryBuildFailed);
  }

  const math::Vec2 tangentIntersection = *intersectionOpt;

  Eigen::Matrix2d matA;
  matA.col(0) = -inletTangent;
  matA.col(1) = outletTangent;

  const math::Vec2 rhs =
    (params.r4 - params.r3) * junctionNormal + params.r3 * inletNormal - params.r4 * outletNormal;

  const double det = matA.determinant();

  if (std::abs(det) < DET_TOL) {
    return std::unexpected(CoreError::GeometryBuildFailed);
  }

  const math::Vec2 solution = matA.fullPivLu().solve(rhs);

  if (!isFiniteVec(solution)) {
    return std::unexpected(CoreError::GeometryBuildFailed);
  }

  const double distIn = solution.x();
  const double distOut = solution.y();

  const math::Vec2 shrP8 = tangentIntersection + distIn * inletTangent;

  const math::Vec2 shrP6 = tangentIntersection + distOut * outletTangent;

  const math::Vec2 shrO3 = shrP8 + params.r3 * inletNormal;
  const math::Vec2 shrO4 = shrP6 + params.r4 * outletNormal;

  const math::Vec2 shrP7 = shrO3 - params.r3 * junctionNormal;

  if ((shrP8 - shrP9).dot(inletTangent) <= 0.0) {
    return std::unexpected(CoreError::GeometryBuildFailed);
  }

  if ((shrP5 - shrP6).dot(outletTangent) <= 0.0) {
    return std::unexpected(CoreError::GeometryBuildFailed);
  }

  const auto shrSeg0 = math::segToBezier(shrP9, shrP8);

  const double shrArc1Start = vecAngle(shrP8 - shrO3);
  const auto shrArc1 = math::arcToBezier(shrO3, params.r3, shrArc1Start, shrArc1Start + be3Sweep);

  if ((shrArc1.p2 - shrP7).norm() > JOIN_TOL) {
    return std::unexpected(CoreError::GeometryBuildFailed);
  }

  const double shrArc2Start = vecAngle(shrP7 - shrO4);
  const auto shrArc2 = math::arcToBezier(shrO4, params.r4, shrArc2Start, shrArc2Start + be4);

  if ((shrArc2.p2 - shrP6).norm() > JOIN_TOL) {
    return std::unexpected(CoreError::GeometryBuildFailed);
  }

  const auto shrSeg3 = math::segToBezier(shrP6, shrP5);

  const std::array<math::ArcBezier, 4> shroudSegments = {shrSeg0, shrArc1, shrArc2, shrSeg3};

  auto shroudNurbs = math::buildFromSegments(shroudSegments);
  auto shroudCurve = math::evaluate(shroudNurbs, 1500);

  return MeridionalGeometry{
    .hubCurve = std::move(hubCurve),
    .shroudCurve = std::move(shroudCurve),
    .hubNurbs = std::move(hubNurbs),
    .shroudNurbs = std::move(shroudNurbs),
  };
}

}
