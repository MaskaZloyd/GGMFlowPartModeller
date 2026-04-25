#include "core/streamline_extractor.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ggm::core {

namespace {

void
chaikinSmooth(std::vector<math::Vec2>& poly, int iterations)
{
  if (poly.size() < 3 || iterations <= 0) {
    return;
  }
  for (int iter = 0; iter < iterations; ++iter) {
    std::vector<math::Vec2> next;
    next.reserve(poly.size() * 2);
    next.push_back(poly.front());
    for (std::size_t i = 0; i + 1 < poly.size(); ++i) {
      const auto& p0 = poly[i];
      const auto& p1 = poly[i + 1];
      next.push_back(0.75 * p0 + 0.25 * p1);
      next.push_back(0.25 * p0 + 0.75 * p1);
    }
    next.push_back(poly.back());
    poly = std::move(next);
  }
}

constexpr std::uint64_t
edgeIdI(int i, int j, int m) noexcept
{
  return (static_cast<std::uint64_t>(i) * static_cast<std::uint64_t>(m) +
          static_cast<std::uint64_t>(j)) *
           2ULL +
         0ULL;
}
constexpr std::uint64_t
edgeIdJ(int i, int j, int m) noexcept
{
  return (static_cast<std::uint64_t>(i) * static_cast<std::uint64_t>(m) +
          static_cast<std::uint64_t>(j)) *
           2ULL +
         1ULL;
}

math::Vec2
interpolate(const math::Vec2& pa, const math::Vec2& pb, double va, double vb, double level) noexcept
{
  double denom = vb - va;
  double t = 0.5;
  if (std::abs(denom) > 1e-15) {
    t = (level - va) / denom;
  }
  t = std::clamp(t, 0.0, 1.0);
  return (1.0 - t) * pa + t * pb;
}

enum : int
{
  EdgeBottom = 0,
  EdgeRight = 1,
  EdgeTop = 2,
  EdgeLeft = 3
};

struct Segments
{
  int count;
  std::array<std::array<int, 2>, 2> edges;
};

constexpr std::array<Segments, 16> SEGMENT_TABLE = {{
  {0, {{{-1, -1}, {-1, -1}}}},
  {1, {{{EdgeLeft, EdgeBottom}, {-1, -1}}}},
  {1, {{{EdgeBottom, EdgeRight}, {-1, -1}}}},
  {1, {{{EdgeLeft, EdgeRight}, {-1, -1}}}},
  {1, {{{EdgeRight, EdgeTop}, {-1, -1}}}},
  {2, {{{EdgeLeft, EdgeBottom}, {EdgeRight, EdgeTop}}}},
  {1, {{{EdgeBottom, EdgeTop}, {-1, -1}}}},
  {1, {{{EdgeLeft, EdgeTop}, {-1, -1}}}},
  {1, {{{EdgeTop, EdgeLeft}, {-1, -1}}}},
  {1, {{{EdgeTop, EdgeBottom}, {-1, -1}}}},
  {2, {{{EdgeBottom, EdgeRight}, {EdgeTop, EdgeLeft}}}},
  {1, {{{EdgeTop, EdgeRight}, {-1, -1}}}},
  {1, {{{EdgeRight, EdgeLeft}, {-1, -1}}}},
  {1, {{{EdgeBottom, EdgeRight}, {-1, -1}}}},
  {1, {{{EdgeLeft, EdgeBottom}, {-1, -1}}}},
  {0, {{{-1, -1}, {-1, -1}}}},
}};

struct EdgeKey
{
  std::uint64_t id;
};

struct SegmentRecord
{
  std::uint64_t edgeA;
  std::uint64_t edgeB;
};

std::vector<std::vector<math::Vec2>>
stitch(const std::vector<SegmentRecord>& segments,
       const std::unordered_map<std::uint64_t, math::Vec2>& edgePoints)
{

  std::unordered_map<std::uint64_t, std::vector<std::pair<std::uint64_t, std::size_t>>> adjacency;
  adjacency.reserve(segments.size() * 2);
  for (std::size_t s = 0; s < segments.size(); ++s) {
    adjacency[segments[s].edgeA].emplace_back(segments[s].edgeB, s);
    adjacency[segments[s].edgeB].emplace_back(segments[s].edgeA, s);
  }

  std::vector<bool> visited(segments.size(), false);
  std::vector<std::vector<math::Vec2>> polylines;

  auto extendFrom = [&](std::uint64_t start) -> std::vector<math::Vec2> {
    std::vector<math::Vec2> pts;
    auto it = edgePoints.find(start);
    if (it == edgePoints.end()) {
      return pts;
    }
    pts.push_back(it->second);
    std::uint64_t current = start;
    std::uint64_t previous = UINT64_MAX;
    while (true) {
      auto adjIt = adjacency.find(current);
      if (adjIt == adjacency.end()) {
        break;
      }
      std::uint64_t nextVertex = UINT64_MAX;
      std::size_t nextSegment = 0;
      for (auto [other, segIdx] : adjIt->second) {
        if (visited[segIdx]) {
          continue;
        }
        if (other == previous) {
          continue;
        }
        nextVertex = other;
        nextSegment = segIdx;
        break;
      }
      if (nextVertex == UINT64_MAX) {
        break;
      }
      visited[nextSegment] = true;
      auto pIt = edgePoints.find(nextVertex);
      if (pIt == edgePoints.end()) {
        break;
      }
      pts.push_back(pIt->second);
      previous = current;
      current = nextVertex;
    }
    return pts;
  };

  for (const auto& [vertex, neighbors] : adjacency) {
    if (neighbors.size() == 1 && !visited[neighbors.front().second]) {
      auto poly = extendFrom(vertex);
      if (poly.size() >= 2) {
        polylines.push_back(std::move(poly));
      }
    }
  }

  for (std::size_t s = 0; s < segments.size(); ++s) {
    if (visited[s]) {
      continue;
    }
    visited[s] = true;
    auto it0 = edgePoints.find(segments[s].edgeA);
    auto it1 = edgePoints.find(segments[s].edgeB);
    if (it0 == edgePoints.end() || it1 == edgePoints.end()) {
      continue;
    }
    std::vector<math::Vec2> pts;
    pts.push_back(it0->second);
    pts.push_back(it1->second);
    std::uint64_t current = segments[s].edgeB;
    std::uint64_t previous = segments[s].edgeA;
    while (true) {
      auto adjIt = adjacency.find(current);
      if (adjIt == adjacency.end()) {
        break;
      }
      std::uint64_t nextVertex = UINT64_MAX;
      std::size_t nextSegment = 0;
      for (auto [other, segIdx] : adjIt->second) {
        if (visited[segIdx]) {
          continue;
        }
        if (other == previous) {
          continue;
        }
        nextVertex = other;
        nextSegment = segIdx;
        break;
      }
      if (nextVertex == UINT64_MAX) {
        break;
      }
      visited[nextSegment] = true;
      auto pIt = edgePoints.find(nextVertex);
      if (pIt == edgePoints.end()) {
        break;
      }
      pts.push_back(pIt->second);
      previous = current;
      current = nextVertex;
    }
    if (pts.size() >= 2) {
      polylines.push_back(std::move(pts));
    }
  }

  return polylines;
}

}

std::vector<double>
equidistantLevels(int count) noexcept
{
  std::vector<double> levels;

  if (count <= 0) {
    return levels;
  }

  levels.reserve(static_cast<std::size_t>(count));

  const double denom = static_cast<double>(count + 1);
  for (int k = 1; k <= count; ++k) {
    levels.push_back(static_cast<double>(k) / denom);
  }

  return levels;
}

std::vector<Streamline>
extractStreamlines(const FlowSolution& sol, const std::vector<double>& psiLevels) noexcept
{
  const auto& grid = sol.grid;
  std::vector<Streamline> streamlines;
  streamlines.reserve(psiLevels.size());

  const int nh = grid.nh;
  const int mm = grid.m;
  if (nh < 2 || mm < 2) {
    return streamlines;
  }

  auto nodeIdx = [&](int i, int j) -> std::size_t { return static_cast<std::size_t>(i * mm + j); };

  for (double level : psiLevels) {
    std::unordered_map<std::uint64_t, math::Vec2> edgePoints;
    std::vector<SegmentRecord> segments;
    segments.reserve(static_cast<std::size_t>((nh - 1) * (mm - 1)));

    auto getEdgeCrossing = [&](int edgeLabel, int i, int j) -> std::uint64_t {
      std::uint64_t id = 0;
      int ia = 0;
      int ja = 0;
      int ib = 0;
      int jb = 0;
      switch (edgeLabel) {
        case EdgeBottom:
          id = edgeIdI(i, j, mm);
          ia = i;
          ja = j;
          ib = i + 1;
          jb = j;
          break;
        case EdgeRight:
          id = edgeIdJ(i + 1, j, mm);
          ia = i + 1;
          ja = j;
          ib = i + 1;
          jb = j + 1;
          break;
        case EdgeTop:
          id = edgeIdI(i, j + 1, mm);
          ia = i;
          ja = j + 1;
          ib = i + 1;
          jb = j + 1;
          break;
        case EdgeLeft:
        default:
          id = edgeIdJ(i, j, mm);
          ia = i;
          ja = j;
          ib = i;
          jb = j + 1;
          break;
      }
      if (edgePoints.find(id) == edgePoints.end()) {
        edgePoints[id] = interpolate(grid.nodes[nodeIdx(ia, ja)],
                                     grid.nodes[nodeIdx(ib, jb)],
                                     sol.psi[nodeIdx(ia, ja)],
                                     sol.psi[nodeIdx(ib, jb)],
                                     level);
      }
      return id;
    };

    for (int i = 0; i < nh - 1; ++i) {
      for (int j = 0; j < mm - 1; ++j) {
        double v00 = sol.psi[nodeIdx(i, j)];
        double v10 = sol.psi[nodeIdx(i + 1, j)];
        double v11 = sol.psi[nodeIdx(i + 1, j + 1)];
        double v01 = sol.psi[nodeIdx(i, j + 1)];

        int caseBits = 0;
        caseBits |= (v00 >= level ? 1 : 0);
        caseBits |= (v10 >= level ? 2 : 0);
        caseBits |= (v11 >= level ? 4 : 0);
        caseBits |= (v01 >= level ? 8 : 0);

        const auto& entry = SEGMENT_TABLE[static_cast<std::size_t>(caseBits)];
        for (int s = 0; s < entry.count; ++s) {
          auto edgeA = getEdgeCrossing(entry.edges[static_cast<std::size_t>(s)][0], i, j);
          auto edgeB = getEdgeCrossing(entry.edges[static_cast<std::size_t>(s)][1], i, j);
          segments.push_back({edgeA, edgeB});
        }
      }
    }

    auto polylines = stitch(segments, edgePoints);

    Streamline line;
    line.psiLevel = level;
    if (!polylines.empty()) {
      auto longest =
        std::max_element(polylines.begin(), polylines.end(), [](const auto& a, const auto& b) {
          return a.size() < b.size();
        });
      line.points = std::move(*longest);

      chaikinSmooth(line.points, 2);
    }
    streamlines.push_back(std::move(line));
  }

  return streamlines;
}

}
