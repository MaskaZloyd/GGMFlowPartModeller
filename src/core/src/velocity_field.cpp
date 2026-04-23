#include "core/velocity_field.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace ggm::core {

namespace {

constexpr double kEps = 1e-10;

struct CachedTriangle
{
  int i0, i1, i2;
  math::Vec2 p0, p1, p2;
  math::Vec2 elementGradPsi; // gradient on the element
  double area;
};

struct Barycentric
{
  double u, v, w;
};

Barycentric
barycentricCoordinates(const math::Vec2& p,
                       const math::Vec2& a,
                       const math::Vec2& b,
                       const math::Vec2& c)
{
  math::Vec2 v0 = b - a;
  math::Vec2 v1 = c - a;
  math::Vec2 v2 = p - a;
  double d00 = v0.dot(v0);
  double d01 = v0.dot(v1);
  double d11 = v1.dot(v1);
  double d20 = v2.dot(v0);
  double d21 = v2.dot(v1);
  double denom = d00 * d11 - d01 * d01;
  if (std::abs(denom) < kEps) {
    return {-1.0, -1.0, -1.0}; // Degenerate case
  }
  double v = (d11 * d20 - d01 * d21) / denom;
  double w = (d00 * d21 - d01 * d20) / denom;
  double u = 1.0 - v - w;
  return {u, v, w};
}

bool
isInsideTriangle(const Barycentric& b)
{
  return b.u >= -kEps && b.v >= -kEps && b.w >= -kEps && b.u <= 1.0 + kEps &&
         b.v <= 1.0 + kEps && b.w <= 1.0 + kEps;
}

double
segmentDistanceSquared(const math::Vec2& p,
                       const math::Vec2& a,
                       const math::Vec2& b)
{
  math::Vec2 ab = b - a;
  double l2 = ab.squaredNorm();
  if (l2 < kEps)
    return (p - a).squaredNorm();
  double t = std::max(0.0, std::min(1.0, (p - a).dot(ab) / l2));
  math::Vec2 projection = a + t * ab;
  return (p - projection).squaredNorm();
}

double
pointTriangleDistanceSquared(const math::Vec2& p,
                             const math::Vec2& a,
                             const math::Vec2& b,
                             const math::Vec2& c)
{
  Barycentric bCoords = barycentricCoordinates(p, a, b, c);
  if (isInsideTriangle(bCoords)) {
    return 0.0;
  }
  double d1 = segmentDistanceSquared(p, a, b);
  double d2 = segmentDistanceSquared(p, b, c);
  double d3 = segmentDistanceSquared(p, c, a);
  return std::min({d1, d2, d3});
}

Result<CachedTriangle>
makeCachedTriangle(const FlowSolution& sol, int i0, int i1, int i2)
{
  if (i0 < 0 || i1 < 0 || i2 < 0)
    return std::unexpected(CoreError::GridBuildFailed);
  
  size_t expectedNodes = static_cast<size_t>(sol.grid.nh) * static_cast<size_t>(sol.grid.m);
  if (static_cast<size_t>(i0) >= expectedNodes || 
      static_cast<size_t>(i1) >= expectedNodes || 
      static_cast<size_t>(i2) >= expectedNodes) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  math::Vec2 p0 = sol.grid.nodes[static_cast<size_t>(i0)];
  math::Vec2 p1 = sol.grid.nodes[static_cast<size_t>(i1)];
  math::Vec2 p2 = sol.grid.nodes[static_cast<size_t>(i2)];
  double psi0 = sol.psi[static_cast<size_t>(i0)];
  double psi1 = sol.psi[static_cast<size_t>(i1)];
  double psi2 = sol.psi[static_cast<size_t>(i2)];

  double detJ = (p1.x() - p0.x()) * (p2.y() - p0.y()) - (p2.x() - p0.x()) * (p1.y() - p0.y());
  
  if (std::abs(detJ) < kEps) {
    return std::unexpected(CoreError::SolverFailed);
  }

  math::Vec2 gradPhi0{p1.y() - p2.y(), p2.x() - p1.x()};
  gradPhi0 /= detJ;
  math::Vec2 gradPhi1{p2.y() - p0.y(), p0.x() - p2.x()};
  gradPhi1 /= detJ;
  math::Vec2 gradPhi2{p0.y() - p1.y(), p1.x() - p0.x()};
  gradPhi2 /= detJ;

  math::Vec2 gradPsi = psi0 * gradPhi0 + psi1 * gradPhi1 + psi2 * gradPhi2;
  double area = std::abs(detJ) * 0.5;

  return CachedTriangle{i0, i1, i2, p0, p1, p2, gradPsi, area};
}

Result<std::vector<CachedTriangle>>
buildTriangleCache(const FlowSolution& sol)
{
  std::vector<CachedTriangle> cache;
  cache.reserve(sol.grid.triangles.size());
  for (const auto& tri : sol.grid.triangles) {
    auto res = makeCachedTriangle(sol, tri[0], tri[1], tri[2]);
    if (res) {
      cache.push_back(*res);
    }
  }
  if (cache.empty())
    return std::unexpected(CoreError::SolverFailed);
  return cache;
}

std::vector<math::Vec2>
recoverNodalGradients(const std::vector<CachedTriangle>& cache, size_t numNodes)
{
  std::vector<math::Vec2> nodalGradients(numNodes, {0.0, 0.0});
  std::vector<double> nodalWeights(numNodes, 0.0);

  for (const auto& tri : cache) {
    size_t idx0 = static_cast<size_t>(tri.i0);
    size_t idx1 = static_cast<size_t>(tri.i1);
    size_t idx2 = static_cast<size_t>(tri.i2);
    nodalGradients[idx0] += tri.elementGradPsi * tri.area;
    nodalWeights[idx0] += tri.area;
    nodalGradients[idx1] += tri.elementGradPsi * tri.area;
    nodalWeights[idx1] += tri.area;
    nodalGradients[idx2] += tri.elementGradPsi * tri.area;
    nodalWeights[idx2] += tri.area;
  }

  for (size_t i = 0; i < numNodes; ++i) {
    if (nodalWeights[i] > kEps) {
      nodalGradients[i] /= nodalWeights[i];
    }
  }

  return nodalGradients;
}

math::Vec2
gradientAtPoint(const math::Vec2& p,
                const std::vector<CachedTriangle>& cache,
                const std::vector<math::Vec2>& nodalGradients)
{
  const CachedTriangle* closestTri = nullptr;
  double minDistSq = std::numeric_limits<double>::max();

  for (const auto& tri : cache) {
    Barycentric b = barycentricCoordinates(p, tri.p0, tri.p1, tri.p2);
    if (isInsideTriangle(b)) {
      return b.u * nodalGradients[static_cast<size_t>(tri.i0)] +
             b.v * nodalGradients[static_cast<size_t>(tri.i1)] +
             b.w * nodalGradients[static_cast<size_t>(tri.i2)];
    }
  }

  for (const auto& tri : cache) {
    double distSq = pointTriangleDistanceSquared(p, tri.p0, tri.p1, tri.p2);
    if (distSq < minDistSq) {
      minDistSq = distSq;
      closestTri = &tri;
    }
  }

  if (closestTri != nullptr) {
    return closestTri->elementGradPsi;
  }
  return {0.0, 0.0};
}

Result<math::Vec2>
velocityFromGradient(const math::Vec2& p,
                     const math::Vec2& gradPsi,
                     double flowRateM3s)
{
  double r = p.y();
  if (r <= 1e-9) {
    return std::unexpected(CoreError::SolverFailed);
  }
  double scale = flowRateM3s / (2.0 * std::numbers::pi);
  return math::Vec2{scale * gradPsi.y() / r, -scale * gradPsi.x() / r};
}

} // namespace

Result<std::vector<StreamlineVelocity>>
computeStreamlineVelocities(const FlowSolution& sol,
                            std::span<const Streamline> streamlines,
                            double flowRateM3s)
{
  if (sol.grid.nh < 2 || sol.grid.m < 2) {
    return std::unexpected(CoreError::GridBuildFailed);
  }
  
  size_t expectedNodes = static_cast<size_t>(sol.grid.nh) * static_cast<size_t>(sol.grid.m);
  
  if (sol.grid.nodes.size() < expectedNodes) {
    return std::unexpected(CoreError::GridBuildFailed);
  }
  if (sol.psi.size() < expectedNodes) {
    return std::unexpected(CoreError::GridBuildFailed);
  }
  if (sol.grid.triangles.empty()) {
    return std::unexpected(CoreError::GridBuildFailed);
  }
  if (!std::isfinite(flowRateM3s) || std::abs(flowRateM3s) < 1e-9) {
    return std::unexpected(CoreError::InvalidParameter);
  }

  auto cacheRes = buildTriangleCache(sol);
  if (!cacheRes) {
    return std::unexpected(cacheRes.error());
  }

  auto cache = *cacheRes;
  auto nodalGradients = recoverNodalGradients(cache, expectedNodes);

  std::vector<StreamlineVelocity> results;
  results.reserve(streamlines.size());

  for (const auto& sl : streamlines) {
    StreamlineVelocity sv;
    sv.psiLevel = sl.psiLevel;
    sv.samples.reserve(sl.points.size());

    double currentArcLength = 0.0;

    for (size_t i = 0; i < sl.points.size(); ++i) {
      const auto& p = sl.points[i];

      if (i > 0) {
        currentArcLength += (p - sl.points[i - 1]).norm();
      }

      math::Vec2 gradPsi = gradientAtPoint(p, cache, nodalGradients);
      auto velRes = velocityFromGradient(p, gradPsi, flowRateM3s);
      
      if (!velRes) {
        return std::unexpected(velRes.error());
      }

      math::Vec2 vel = *velRes;
      double speed = vel.norm();

      sv.samples.push_back({p,
                            vel,
                            speed,
                            p.y(),
                            currentArcLength});
    }

    results.push_back(std::move(sv));
  }

  return results;
}

} // namespace ggm::core
