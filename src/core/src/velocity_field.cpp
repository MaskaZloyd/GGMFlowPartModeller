#include "core/velocity_field.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

namespace ggm::core {

namespace {

constexpr double kElementDetTol = 1e-15;
constexpr double kBarycentricTol = 1e-10;
constexpr double kBarycentricDenomTol = 1e-30;
constexpr double kMinSegmentLength2 = 1e-30;
constexpr double kMinRadiusMeters = 1e-9;
constexpr double kMinFlowRate = 1e-30;

struct CachedTriangle
{
  int i0{-1};
  int i1{-1};
  int i2{-1};

  math::Vec2 p0{0.0, 0.0};
  math::Vec2 p1{0.0, 0.0};
  math::Vec2 p2{0.0, 0.0};

  math::Vec2 elementGradPsi{0.0, 0.0};

  double area{0.0};
};

struct Barycentric
{
  double u{0.0};
  double v{0.0};
  double w{0.0};
  bool valid{false};
};

Barycentric
barycentricCoordinates(const math::Vec2& p,
                       const math::Vec2& a,
                       const math::Vec2& b,
                       const math::Vec2& c) noexcept
{
  const math::Vec2 v0 = b - a;
  const math::Vec2 v1 = c - a;
  const math::Vec2 v2 = p - a;

  const double d00 = v0.dot(v0);
  const double d01 = v0.dot(v1);
  const double d11 = v1.dot(v1);
  const double d20 = v2.dot(v0);
  const double d21 = v2.dot(v1);

  const double denom = d00 * d11 - d01 * d01;

  if (std::abs(denom) < kBarycentricDenomTol) {
    return {};
  }

  const double v = (d11 * d20 - d01 * d21) / denom;
  const double w = (d00 * d21 - d01 * d20) / denom;
  const double u = 1.0 - v - w;

  return Barycentric{.u = u, .v = v, .w = w, .valid = true};
}

bool
isInsideTriangle(const Barycentric& b) noexcept
{
  if (!b.valid) {
    return false;
  }

  return b.u >= -kBarycentricTol && b.v >= -kBarycentricTol && b.w >= -kBarycentricTol &&
         b.u <= 1.0 + kBarycentricTol && b.v <= 1.0 + kBarycentricTol &&
         b.w <= 1.0 + kBarycentricTol;
}

double
segmentDistanceSquared(const math::Vec2& p, const math::Vec2& a, const math::Vec2& b) noexcept
{
  const math::Vec2 ab = b - a;
  const double ab2 = ab.squaredNorm();

  if (ab2 < kMinSegmentLength2) {
    return (p - a).squaredNorm();
  }

  const double t = std::clamp((p - a).dot(ab) / ab2, 0.0, 1.0);

  const math::Vec2 projection = a + t * ab;

  return (p - projection).squaredNorm();
}

double
pointTriangleDistanceSquared(const math::Vec2& p,
                             const math::Vec2& a,
                             const math::Vec2& b,
                             const math::Vec2& c) noexcept
{
  const Barycentric coords = barycentricCoordinates(p, a, b, c);

  if (isInsideTriangle(coords)) {
    return 0.0;
  }

  const double d01 = segmentDistanceSquared(p, a, b);
  const double d12 = segmentDistanceSquared(p, b, c);
  const double d20 = segmentDistanceSquared(p, c, a);

  return std::min({d01, d12, d20});
}

Result<CachedTriangle>
makeCachedTriangle(const FlowSolution& sol, int i0, int i1, int i2)
{
  if (i0 < 0 || i1 < 0 || i2 < 0) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  const auto& grid = sol.grid;

  const auto expectedNodes = static_cast<std::size_t>(grid.nh) * static_cast<std::size_t>(grid.m);

  const auto u0 = static_cast<std::size_t>(i0);
  const auto u1 = static_cast<std::size_t>(i1);
  const auto u2 = static_cast<std::size_t>(i2);

  if (u0 >= expectedNodes || u1 >= expectedNodes || u2 >= expectedNodes) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  if (u0 >= grid.nodes.size() || u1 >= grid.nodes.size() || u2 >= grid.nodes.size() ||
      u0 >= sol.psi.size() || u1 >= sol.psi.size() || u2 >= sol.psi.size()) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  const math::Vec2 p0 = grid.nodes[u0];
  const math::Vec2 p1 = grid.nodes[u1];
  const math::Vec2 p2 = grid.nodes[u2];

  const double psi0 = sol.psi[u0];
  const double psi1 = sol.psi[u1];
  const double psi2 = sol.psi[u2];

  const double detJ = (p1.x() - p0.x()) * (p2.y() - p0.y()) - (p2.x() - p0.x()) * (p1.y() - p0.y());

  if (std::abs(detJ) < kElementDetTol) {
    return std::unexpected(CoreError::SolverFailed);
  }

  math::Vec2 gradPhi0{p1.y() - p2.y(), p2.x() - p1.x()};
  math::Vec2 gradPhi1{p2.y() - p0.y(), p0.x() - p2.x()};
  math::Vec2 gradPhi2{p0.y() - p1.y(), p1.x() - p0.x()};

  gradPhi0 /= detJ;
  gradPhi1 /= detJ;
  gradPhi2 /= detJ;

  const math::Vec2 gradPsi = psi0 * gradPhi0 + psi1 * gradPhi1 + psi2 * gradPhi2;

  const double area = 0.5 * std::abs(detJ);

  return CachedTriangle{.i0 = i0,
                        .i1 = i1,
                        .i2 = i2,
                        .p0 = p0,
                        .p1 = p1,
                        .p2 = p2,
                        .elementGradPsi = gradPsi,
                        .area = area};
}

Result<std::vector<CachedTriangle>>
buildTriangleCache(const FlowSolution& sol)
{
  std::vector<CachedTriangle> cache;
  cache.reserve(sol.grid.triangles.size());

  for (const auto& tri : sol.grid.triangles) {
    auto triangleResult = makeCachedTriangle(sol, tri[0], tri[1], tri[2]);

    if (triangleResult) {
      cache.push_back(*triangleResult);
    }
  }

  if (cache.empty()) {
    return std::unexpected(CoreError::SolverFailed);
  }

  return cache;
}

std::vector<math::Vec2>
recoverNodalGradients(const std::vector<CachedTriangle>& cache, std::size_t numNodes)
{
  std::vector<math::Vec2> nodalGradients(numNodes, math::Vec2{0.0, 0.0});
  std::vector<double> nodalWeights(numNodes, 0.0);

  for (const auto& tri : cache) {
    const auto idx0 = static_cast<std::size_t>(tri.i0);
    const auto idx1 = static_cast<std::size_t>(tri.i1);
    const auto idx2 = static_cast<std::size_t>(tri.i2);

    if (idx0 >= numNodes || idx1 >= numNodes || idx2 >= numNodes) {
      continue;
    }

    nodalGradients[idx0] += tri.area * tri.elementGradPsi;
    nodalWeights[idx0] += tri.area;

    nodalGradients[idx1] += tri.area * tri.elementGradPsi;
    nodalWeights[idx1] += tri.area;

    nodalGradients[idx2] += tri.area * tri.elementGradPsi;
    nodalWeights[idx2] += tri.area;
  }

  for (std::size_t i = 0; i < numNodes; ++i) {
    if (nodalWeights[i] > 0.0) {
      nodalGradients[i] /= nodalWeights[i];
    }
  }

  return nodalGradients;
}

std::optional<math::Vec2>
gradientAtPoint(const math::Vec2& p,
                const std::vector<CachedTriangle>& cache,
                const std::vector<math::Vec2>& nodalGradients) noexcept
{
  const CachedTriangle* closestTri = nullptr;
  double minDistSq = std::numeric_limits<double>::max();

  for (const auto& tri : cache) {
    const Barycentric b = barycentricCoordinates(p, tri.p0, tri.p1, tri.p2);

    if (!isInsideTriangle(b)) {
      continue;
    }

    const auto idx0 = static_cast<std::size_t>(tri.i0);
    const auto idx1 = static_cast<std::size_t>(tri.i1);
    const auto idx2 = static_cast<std::size_t>(tri.i2);

    if (idx0 >= nodalGradients.size() || idx1 >= nodalGradients.size() ||
        idx2 >= nodalGradients.size()) {
      return std::nullopt;
    }

    const math::Vec2 grad =
      b.u * nodalGradients[idx0] + b.v * nodalGradients[idx1] + b.w * nodalGradients[idx2];

    return grad;
  }

  for (const auto& tri : cache) {
    const double distSq = pointTriangleDistanceSquared(p, tri.p0, tri.p1, tri.p2);

    if (distSq < minDistSq) {
      minDistSq = distSq;
      closestTri = &tri;
    }
  }

  if (closestTri != nullptr) {
    return closestTri->elementGradPsi;
  }

  return std::nullopt;
}

Result<math::Vec2>
velocityFromGradient(const math::Vec2& point,
                     const math::Vec2& gradPsiProjectUnits,
                     double flowRateM3s,
                     double lengthUnitToMeters)
{
  if (!std::isfinite(flowRateM3s) || std::abs(flowRateM3s) <= kMinFlowRate) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  if (!std::isfinite(lengthUnitToMeters) || lengthUnitToMeters <= 0.0) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  const double radiusMeters = point.y() * lengthUnitToMeters;

  if (!std::isfinite(radiusMeters) || radiusMeters <= kMinRadiusMeters) {
    return std::unexpected(CoreError::SolverFailed);
  }

  const double psiZPerMeter = gradPsiProjectUnits.x() / lengthUnitToMeters;
  const double psiRPerMeter = gradPsiProjectUnits.y() / lengthUnitToMeters;

  if (!std::isfinite(psiZPerMeter) || !std::isfinite(psiRPerMeter)) {
    return std::unexpected(CoreError::SolverFailed);
  }

  const double scale = flowRateM3s / (2.0 * std::numbers::pi);

  const double vz = scale * psiRPerMeter / radiusMeters;
  const double vr = -scale * psiZPerMeter / radiusMeters;

  if (!std::isfinite(vz) || !std::isfinite(vr)) {
    return std::unexpected(CoreError::SolverFailed);
  }

  return math::Vec2{vz, vr};
}

}

Result<std::vector<StreamlineVelocity>>
computeStreamlineVelocities(const FlowSolution& sol,
                            std::span<const Streamline> streamlines,
                            double flowRateM3s,
                            double lengthUnitToMeters)
{
  const auto& grid = sol.grid;

  if (grid.nh < 2 || grid.m < 2) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  if (!std::isfinite(flowRateM3s) || std::abs(flowRateM3s) <= kMinFlowRate) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  if (!std::isfinite(lengthUnitToMeters) || lengthUnitToMeters <= 0.0) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  const auto expectedNodes = static_cast<std::size_t>(grid.nh) * static_cast<std::size_t>(grid.m);

  if (grid.nodes.size() < expectedNodes) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  if (sol.psi.size() < expectedNodes) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  if (grid.triangles.empty()) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  auto cacheResult = buildTriangleCache(sol);

  if (!cacheResult) {
    return std::unexpected(cacheResult.error());
  }

  const std::vector<CachedTriangle> cache = *cacheResult;
  const std::vector<math::Vec2> nodalGradients = recoverNodalGradients(cache, expectedNodes);

  std::vector<StreamlineVelocity> results;
  results.reserve(streamlines.size());

  for (const auto& streamline : streamlines) {
    StreamlineVelocity lineVelocity;
    lineVelocity.psiLevel = streamline.psiLevel;
    lineVelocity.samples.reserve(streamline.points.size());

    double currentArcLength = 0.0;

    for (std::size_t i = 0; i < streamline.points.size(); ++i) {
      const math::Vec2& point = streamline.points[i];

      if (i > 0) {
        currentArcLength += (point - streamline.points[i - 1]).norm();
      }

      const std::optional<math::Vec2> gradPsi = gradientAtPoint(point, cache, nodalGradients);

      if (!gradPsi.has_value()) {
        return std::unexpected(CoreError::SolverFailed);
      }

      auto velocityResult = velocityFromGradient(point, *gradPsi, flowRateM3s, lengthUnitToMeters);

      if (!velocityResult) {
        return std::unexpected(velocityResult.error());
      }

      const math::Vec2 velocity = *velocityResult;
      const double speed = velocity.norm();

      if (!std::isfinite(speed)) {
        return std::unexpected(CoreError::SolverFailed);
      }

      lineVelocity.samples.push_back(VelocitySample{.point = point,
                                                    .velocity = velocity,
                                                    .speed = speed,
                                                    .radius = point.y(),
                                                    .arcLength = currentArcLength});
    }

    results.push_back(std::move(lineVelocity));
  }

  return results;
}

}
