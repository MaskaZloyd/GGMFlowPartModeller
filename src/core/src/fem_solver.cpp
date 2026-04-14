#include "core/fem_solver.hpp"

#include "core/logging.hpp"

#include <Eigen/Sparse>
#include <Eigen/SparseLU>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace ggm::core {

namespace {

constexpr double R_CLAMP = 1e-9;
constexpr double DET_TOL = 1e-15;

struct TriLocal {
  Eigen::Matrix3d stiffness;
  Eigen::Matrix3d firstOrder;
  double area;
  bool degenerate;
};

// Compute local stiffness and first-order matrices for a P1 triangle element.
// Nodes: (z0,r0), (z1,r1), (z2,r2).
// PDE: div(grad(psi)) - (1/r)*dpsi_dr = 0
TriLocal computeElementMatrices(const math::Vec2& p0,
                                const math::Vec2& p1,
                                const math::Vec2& p2) noexcept {
  TriLocal result{};

  // z = x(), r = y()
  double z0 = p0.x(), r0 = p0.y();
  double z1 = p1.x(), r1 = p1.y();
  double z2 = p2.x(), r2 = p2.y();

  // 2 * area (signed)
  double detJ = (z1 - z0) * (r2 - r0) - (z2 - z0) * (r1 - r0);

  if (std::abs(detJ) < DET_TOL) {
    result.degenerate = true;
    return result;
  }

  result.area = std::abs(detJ) / 2.0;
  result.degenerate = false;

  // Gradient of P1 basis functions (constant per element):
  // grad_phi_i = (1/(2*A)) * [r_j - r_k, z_k - z_j]
  // where (i,j,k) are cyclic permutations of (0,1,2)
  Eigen::Vector2d gradPhi0{r1 - r2, z2 - z1};
  Eigen::Vector2d gradPhi1{r2 - r0, z0 - z2};
  Eigen::Vector2d gradPhi2{r0 - r1, z1 - z0};

  gradPhi0 /= detJ;
  gradPhi1 /= detJ;
  gradPhi2 /= detJ;

  std::array<Eigen::Vector2d, 3> gradPhi = {gradPhi0, gradPhi1, gradPhi2};

  // Stiffness: K_ij = (grad_phi_i . grad_phi_j) * A
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      result.stiffness(i, j) = gradPhi[static_cast<std::size_t>(i)].dot(
                                    gradPhi[static_cast<std::size_t>(j)]) *
                                result.area;
    }
  }

  // First-order term: T_ij = -(A/3) * (dphi_j/dr) / r_elem. Sign kept as in
  // the Python reference implementation while we diagnose the distribution.
  double rElem = std::max((r0 + r1 + r2) / 3.0, R_CLAMP);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      result.firstOrder(i, j) =
          -(result.area / 3.0) * gradPhi[static_cast<std::size_t>(j)].y() / rElem;
    }
  }

  return result;
}

} // namespace

Result<FlowSolution> solveFem(StripGrid grid) noexcept {
  int totalNodes = grid.nh * grid.m;
  auto numTri = static_cast<int>(grid.triangles.size());

  if (totalNodes < 3 || numTri < 1) {
    return std::unexpected(CoreError::SolverFailed);
  }

  // TBB parallel assembly: per-thread triplet lists
  using TripletList = std::vector<Eigen::Triplet<double>>;
  tbb::enumerable_thread_specific<TripletList> threadTriplets;

  tbb::parallel_for(0, numTri, [&](int triIdx) {
    auto triUIdx = static_cast<std::size_t>(triIdx);
    auto tri = grid.triangles[triUIdx];
    const auto& p0 = grid.nodes[static_cast<std::size_t>(tri[0])];
    const auto& p1 = grid.nodes[static_cast<std::size_t>(tri[1])];
    const auto& p2 = grid.nodes[static_cast<std::size_t>(tri[2])];

    // Force CCW orientation: if detJ < 0, swap vertices 1 and 2
    double detJ = (p1.x() - p0.x()) * (p2.y() - p0.y()) -
                  (p2.x() - p0.x()) * (p1.y() - p0.y());
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
    Eigen::Matrix3d combined = local.stiffness + local.firstOrder;

    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        double val = combined(i, j);
        if (val != 0.0) {
          triplets.emplace_back(tri[static_cast<std::size_t>(i)],
                                tri[static_cast<std::size_t>(j)], val);
        }
      }
    }
  });

  // Merge all per-thread triplets
  TripletList allTriplets;
  std::size_t totalSize = 0;
  for (const auto& tl : threadTriplets) {
    totalSize += tl.size();
  }
  allTriplets.reserve(totalSize);
  for (const auto& tl : threadTriplets) {
    allTriplets.insert(allTriplets.end(), tl.begin(), tl.end());
  }

  // Build sparse matrix (ROW-major so that InnerIterator walks rows — the
  // Dirichlet row-replacement below depends on this).
  using SparseRowMajor = Eigen::SparseMatrix<double, Eigen::RowMajor>;
  SparseRowMajor matA(totalNodes, totalNodes);
  matA.setFromTriplets(allTriplets.begin(), allTriplets.end());

  Eigen::VectorXd rhs = Eigen::VectorXd::Zero(totalNodes);

  // Apply Dirichlet BCs: psi=0 on hub, psi=1 on shroud. Row-replacement:
  // zero the row, set diagonal to 1, set rhs to the boundary value.
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

  // Solve — SparseLU needs a col-major operand, so convert once.
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

  // Copy to std::vector
  std::vector<double> psi(static_cast<std::size_t>(totalNodes));
  Eigen::Map<Eigen::VectorXd>(psi.data(), totalNodes) = psiVec;

  // Diagnostic: dump psi along the transverse chord at three rows:
  // inlet area, middle, and outlet area. For a correct Stokes stream
  // function in a locally-annular region this should track
  // (r^2 - r_h^2) / (r_s^2 - r_h^2). If it looks logarithmic (slow near hub,
  // fast near shroud) the PDE being discretised is wrong.
  if (grid.nh > 2 && grid.m >= 5) {
    auto dumpRow = [&](int row) {
      std::ostringstream line;
      line.precision(3);
      line << std::fixed;
      int step = std::max(1, grid.m / 8);
      for (int j = 0; j < grid.m; j += step) {
        auto idx = static_cast<std::size_t>(row * grid.m + j);
        line << " j=" << j << " r=" << grid.nodes[idx].y()
             << " psi=" << psi[idx] << ";";
      }
      auto last = static_cast<std::size_t>(row * grid.m + grid.m - 1);
      line << " j=" << (grid.m - 1) << " r=" << grid.nodes[last].y()
           << " psi=" << psi[last] << ";";
      logging::core()->debug("psi row {}:{}", row, line.str());
    };
    dumpRow(grid.nh / 10);
    dumpRow(grid.nh / 2);
    dumpRow(grid.nh - 1 - grid.nh / 10);
  }

  return FlowSolution{.grid = std::move(grid), .psi = std::move(psi)};
}

} // namespace ggm::core
