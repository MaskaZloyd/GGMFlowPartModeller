#include "core/strip_grid.hpp"

#include <array>
#include <vector>

namespace ggm::core {

namespace {

void
smoothInterior(std::vector<math::Vec2>& nodes, int nh, int mm, int iterations, double relax)
{
  if (nh < 3 || mm < 3 || iterations <= 0) {
    return;
  }
  std::array<std::vector<math::Vec2>, 2> bufs{nodes, nodes};
  int src = 0;
  const auto mmU = static_cast<std::size_t>(mm);
  for (int iter = 0; iter < iterations; ++iter) {
    const int dst = 1 - src;
    const auto& readBuf = bufs[static_cast<std::size_t>(src)];
    auto& writeBuf = bufs[static_cast<std::size_t>(dst)];
    for (int i = 1; i < nh - 1; ++i) {
      const auto rowIdx = static_cast<std::size_t>(i);
      for (int j = 1; j < mm - 1; ++j) {
        const auto colIdx = static_cast<std::size_t>(j);
        const auto idx = (rowIdx * mmU) + colIdx;
        const auto& north = readBuf[((rowIdx - 1) * mmU) + colIdx];
        const auto& south = readBuf[((rowIdx + 1) * mmU) + colIdx];
        const auto& west = readBuf[(rowIdx * mmU) + (colIdx - 1)];
        const auto& east = readBuf[(rowIdx * mmU) + (colIdx + 1)];
        const math::Vec2 avg = 0.25 * (north + south + west + east);
        writeBuf[idx] = ((1.0 - relax) * readBuf[idx]) + (relax * avg);
      }
    }
    src = dst;
  }
  nodes = std::move(bufs[static_cast<std::size_t>(src)]);
}

}

Result<StripGrid>
buildStripGrid(std::span<const math::Vec2> hub, std::span<const math::Vec2> shroud, int m) noexcept
{
  auto nh = static_cast<int>(hub.size());
  if (hub.size() != shroud.size() || nh < 2 || m < 3) {
    return std::unexpected(CoreError::GridBuildFailed);
  }

  StripGrid grid;
  grid.nh = nh;
  grid.m = m;

  int totalNodes = nh * m;
  grid.nodes.resize(static_cast<std::size_t>(totalNodes));
  grid.hubNodes.reserve(static_cast<std::size_t>(nh));
  grid.shroudNodes.reserve(static_cast<std::size_t>(nh));

  for (int i = 0; i < nh; ++i) {
    auto ui = static_cast<std::size_t>(i);
    for (int j = 0; j < m; ++j) {
      double t = static_cast<double>(j) / static_cast<double>(m - 1);
      grid.nodes[static_cast<std::size_t>(i * m + j)] = (1.0 - t) * hub[ui] + t * shroud[ui];
    }
    grid.hubNodes.push_back(i * m);
    grid.shroudNodes.push_back(i * m + (m - 1));
  }

  smoothInterior(grid.nodes, nh, m, 20, 0.5);

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

}
