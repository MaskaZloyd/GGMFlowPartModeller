#include "core/fem_solver.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include <Eigen/Sparse>
#include <Eigen/SparseLU>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>

namespace ggm::core {

namespace {

constexpr double R_CLAMP = 1e-9;
constexpr double DET_TOL = 1e-15;

struct TriLocal
{
  Eigen::Matrix3d stiffness;
  Eigen::Matrix3d firstOrder;
  double area;
  bool degenerate;
};

TriLocal
computeElementMatrices(const math::Vec2& p0, const math::Vec2& p1, const math::Vec2& p2) noexcept
{
  TriLocal result{};
  result.stiffness.setZero();
  result.firstOrder.setZero();
  result.area = 0.0;
  result.degenerate = false;

  const double z0 = p0.x();
  const double r0 = p0.y();
  const double z1 = p1.x();
  const double r1 = p1.y();
  const double z2 = p2.x();
  const double r2 = p2.y();

  const double detJ = (z1 - z0) * (r2 - r0) - (z2 - z0) * (r1 - r0);

  if (std::abs(detJ) < DET_TOL) {
    result.degenerate = true;
    return result;
  }

  result.area = std::abs(detJ) / 2.0;

  Eigen::Vector2d gradPhi0{r1 - r2, z2 - z1};
  Eigen::Vector2d gradPhi1{r2 - r0, z0 - z2};
  Eigen::Vector2d gradPhi2{r0 - r1, z1 - z0};

  gradPhi0 /= detJ;
  gradPhi1 /= detJ;
  gradPhi2 /= detJ;

  const std::array<Eigen::Vector2d, 3> gradPhi = {gradPhi0, gradPhi1, gradPhi2};

  const double rElem = (r0 + r1 + r2) / 3.0;

  if (rElem <= R_CLAMP) {
    result.degenerate = true;
    return result;
  }

  const double invR = 1.0 / rElem;

  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      result.stiffness(i, j) =
        result.area * invR *
        gradPhi[static_cast<std::size_t>(i)].dot(gradPhi[static_cast<std::size_t>(j)]);
    }
  }

  return result;
}

}

Result<FlowSolution>
solveFem(StripGrid grid) noexcept
{
  int totalNodes = grid.nh * grid.m;
  auto numTri = static_cast<int>(grid.triangles.size());

  if (totalNodes < 3 || numTri < 1) {
    return std::unexpected(CoreError::SolverFailed);
  }

  using TripletList = std::vector<Eigen::Triplet<double>>;
  tbb::enumerable_thread_specific<TripletList> threadTriplets;

  tbb::parallel_for(0, numTri, [&](int triIdx) {
    auto triUIdx = static_cast<std::size_t>(triIdx);
    auto tri = grid.triangles[triUIdx];
    const auto& p0 = grid.nodes[static_cast<std::size_t>(tri[0])];
    const auto& p1 = grid.nodes[static_cast<std::size_t>(tri[1])];
    const auto& p2 = grid.nodes[static_cast<std::size_t>(tri[2])];

    double detJ = (p1.x() - p0.x()) * (p2.y() - p0.y()) - (p2.x() - p0.x()) * (p1.y() - p0.y());
    TriLocal local;
    if (detJ < 0.0) {
      local = computeElementMatrices(p0, p2, p1);
      std::swap(tri[1], tri[2]);
    } else {
      local = computeElementMatrices(p0, p1, p2);
    }
    if (local.degenerate) {
      return;
    }

    auto& triplets = threadTriplets.local();
    Eigen::Matrix3d combined = local.stiffness;

    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        double val = combined(i, j);
        if (val != 0.0) {
          triplets.emplace_back(
            tri[static_cast<std::size_t>(i)], tri[static_cast<std::size_t>(j)], val);
        }
      }
    }
  });

  TripletList allTriplets;
  std::size_t totalSize = 0;
  for (const auto& tl : threadTriplets) {
    totalSize += tl.size();
  }
  allTriplets.reserve(totalSize);
  for (const auto& tl : threadTriplets) {
    allTriplets.insert(allTriplets.end(), tl.begin(), tl.end());
  }

  using SparseRowMajor = Eigen::SparseMatrix<double, Eigen::RowMajor>;
  SparseRowMajor matA(totalNodes, totalNodes);
  matA.setFromTriplets(allTriplets.begin(), allTriplets.end());

  Eigen::VectorXd rhs = Eigen::VectorXd::Zero(totalNodes);

  auto applyDirichlet = [&](int node, double value) {
    for (SparseRowMajor::InnerIterator it(matA, node); it; ++it) {
      it.valueRef() = 0.0;
    }
    matA.coeffRef(node, node) = 1.0;
    rhs(node) = value;
  };

  for (int hubNode : grid.hubNodes) {
    applyDirichlet(hubNode, 0.0);
  }
  for (int shroudNode : grid.shroudNodes) {
    applyDirichlet(shroudNode, 1.0);
  }

  matA.makeCompressed();

  Eigen::SparseMatrix<double> matACol = matA;
  Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
  solver.compute(matACol);
  if (solver.info() != Eigen::Success) {
    return std::unexpected(CoreError::SolverFailed);
  }

  Eigen::VectorXd psiVec = solver.solve(rhs);
  if (solver.info() != Eigen::Success) {
    return std::unexpected(CoreError::SolverFailed);
  }

  std::vector<double> psi(static_cast<std::size_t>(totalNodes));
  Eigen::Map<Eigen::VectorXd>(psi.data(), totalNodes) = psiVec;

  return FlowSolution{.grid = std::move(grid), .psi = std::move(psi)};
}

}
