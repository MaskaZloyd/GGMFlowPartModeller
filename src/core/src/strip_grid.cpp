#include "core/strip_grid.hpp"

#include <vector>

namespace ggm::core {

namespace {

// Run a few sweeps of Laplace smoothing on interior grid nodes: each node
// is pulled toward the average of its four neighbours. Hub/shroud walls
// and inlet/outlet rows stay pinned so boundary conditions remain valid.
// This gives the strip grid a more orthogonal look through curved sections
// without changing its topology.
void smoothInterior(std::vector<math::Vec2>& nodes, int nh, int mm,
                    int iterations, double relax) {
  if (nh < 3 || mm < 3 || iterations <= 0) {
    return;
  }
  std::vector<math::Vec2> prev(nodes.size());
  for (int iter = 0; iter < iterations; ++iter) {
    prev = nodes;
    for (int i = 1; i < nh - 1; ++i) {
      for (int j = 1; j < mm - 1; ++j) {
        auto idx = static_cast<std::size_t>(i * mm + j);
        auto n = prev[static_cast<std::size_t>((i - 1) * mm + j)];
        auto s = prev[static_cast<std::size_t>((i + 1) * mm + j)];
        auto w = prev[static_cast<std::size_t>(i * mm + (j - 1))];
        auto e = prev[static_cast<std::size_t>(i * mm + (j + 1))];
        math::Vec2 avg = 0.25 * (n + s + w + e);
        nodes[idx] = (1.0 - relax) * prev[idx] + relax * avg;
      }
    }
  }
}

} // namespace

Result<StripGrid> buildStripGrid(std::span<const math::Vec2> hub,
                                 std::span<const math::Vec2> shroud,
                                 int m) noexcept {
  auto nh = static_cast<int>(hub.size());
  if (hub.size() != shroud.size() || nh < 2 || m < 3) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  StripGrid grid;
  grid.nh = nh;
  grid.m = m;

  // Build nodes: linear interpolation between hub[i] and shroud[i]
  int totalNodes = nh * m;
  grid.nodes.resize(static_cast<std::size_t>(totalNodes));
  grid.hubNodes.reserve(static_cast<std::size_t>(nh));
  grid.shroudNodes.reserve(static_cast<std::size_t>(nh));

  for (int i = 0; i < nh; ++i) {
    auto ui = static_cast<std::size_t>(i);
    for (int j = 0; j < m; ++j) {
      double t = static_cast<double>(j) / static_cast<double>(m - 1);
      grid.nodes[static_cast<std::size_t>(i * m + j)] =
          (1.0 - t) * hub[ui] + t * shroud[ui];
    }
    grid.hubNodes.push_back(i * m);
    grid.shroudNodes.push_back(i * m + (m - 1));
  }

  // Smooth interior nodes (Laplace, 20 sweeps at 0.5 relaxation).
  smoothInterior(grid.nodes, nh, m, /*iterations=*/20, /*relax=*/0.5);

  // Build triangles: 2 per quad cell
  int numQuads = (nh - 1) * (m - 1);
  grid.triangles.reserve(static_cast<std::size_t>(numQuads * 2));

  for (int i = 0; i < nh - 1; ++i) {
    for (int j = 0; j < m - 1; ++j) {
      int n00 = i * m + j;
      int n10 = (i + 1) * m + j;
      int n11 = (i + 1) * m + (j + 1);
      int n01 = i * m + (j + 1);
      grid.triangles.push_back({n00, n10, n11});
      grid.triangles.push_back({n00, n11, n01});
    }
  }

  return grid;
}

} // namespace ggm::core
