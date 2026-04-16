// Stand-alone benchmark binary. Not a CTest test — invoked via
// `perf record -- build/release/tests/bench_hotpaths`.
//
// Exercises the three hot paths identified in the refactoring spec:
//   1. NURBS evaluation (1000 × 1500 points)
//   2. Full solver pipeline (50 × buildStripGrid + solveFem)
//   3. Flat vertex conversion (equivalent to drawPolyline inner loop)

#include "core/fem_solver.hpp"
#include "core/geometry.hpp"
#include "core/logging.hpp"
#include "core/pump_params.hpp"
#include "core/strip_grid.hpp"
#include "math/bezier.hpp"
#include "math/nurbs.hpp"
#include "math/types.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <numbers>
#include <vector>

using ggm::math::ArcBezier;
using ggm::math::Vec2;

namespace {

void benchNurbs() {
  const Vec2 center{0.0, 0.0};
  constexpr double radius = 1.0;
  const std::array<ArcBezier, 2> segs = {
    ggm::math::arcToBezier(center, radius, 0.0, std::numbers::pi / 2.0),
    ggm::math::arcToBezier(center, radius, std::numbers::pi / 2.0, std::numbers::pi),
  };
  const auto curve = ggm::math::buildFromSegments(segs);

  const auto start = std::chrono::steady_clock::now();
  std::size_t checksum = 0;
  for (int iter = 0; iter < 1000; ++iter) {
    const auto pts = ggm::math::evaluate(curve, 1500);
    checksum += static_cast<std::size_t>(pts.back().x() * 1e6) +
                static_cast<std::size_t>(pts.back().y() * 1e6);
  }
  const auto elapsed = std::chrono::steady_clock::now() - start;
  const auto elapsedMs =
    std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  std::printf("nurbs: %ld ms (checksum=%zu)\n", static_cast<long>(elapsedMs), checksum); // NOLINT(cppcoreguidelines-pro-type-vararg)
}

void benchSolver() {
  ggm::core::PumpParams params;
  const auto geom = ggm::core::buildGeometry(params);
  if (!geom) {
    std::printf("solver: geometry build failed\n"); // NOLINT(cppcoreguidelines-pro-type-vararg)
    return;
  }

  const auto start = std::chrono::steady_clock::now();
  std::size_t checksum = 0;
  for (int iter = 0; iter < 50; ++iter) {
    auto grid = ggm::core::buildStripGrid(geom->hubCurve, geom->shroudCurve, 80);
    if (!grid) {
      continue;
    }
    auto sol = ggm::core::solveFem(std::move(*grid));
    if (sol && !sol->psi.empty()) {
      checksum += static_cast<std::size_t>(sol->psi.back() * 1e9) +
                  static_cast<std::size_t>(sol->psi.front() * 1e9);
    }
  }
  const auto elapsed = std::chrono::steady_clock::now() - start;
  const auto elapsedMs =
    std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  std::printf("solver: %ld ms (checksum=%zu)\n", static_cast<long>(elapsedMs), checksum); // NOLINT(cppcoreguidelines-pro-type-vararg)
}

void benchFlatConvert() {
  std::vector<Vec2> points;
  points.reserve(1500);
  for (int i = 0; i < 1500; ++i) {
    const auto paramT = static_cast<double>(i) / 1499.0;
    points.push_back({paramT, paramT * paramT});
  }

  const auto start = std::chrono::steady_clock::now();
  double checksum = 0.0;
  for (int iter = 0; iter < 20000; ++iter) {
    std::vector<float> flat;
    flat.reserve(points.size() * 2U);
    for (const auto& pos : points) {
      flat.push_back(static_cast<float>(pos.x()));
      flat.push_back(static_cast<float>(pos.y()));
    }
    checksum += static_cast<double>(flat.size());
  }
  const auto elapsed = std::chrono::steady_clock::now() - start;
  const auto elapsedMs =
    std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  std::printf("flat:   %ld ms (checksum=%.0f)\n", static_cast<long>(elapsedMs), checksum); // NOLINT(cppcoreguidelines-pro-type-vararg)
}

} // namespace

int main() {
  ggm::logging::init();
  benchNurbs();
  benchSolver();
  benchFlatConvert();
  return 0;
}
