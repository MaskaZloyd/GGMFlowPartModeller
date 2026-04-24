#pragma once

#include "core/async_flow_solver.hpp"
#include "core/error.hpp"
#include "core/geometry_optimizer.hpp"
#include "core/pump_params.hpp"
#include "core/target_area_curve.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

#include <tbb/task_arena.h>
#include <tbb/task_group.h>

namespace ggm::core {

// Asynchronous wrapper around optimizeGeometryForTargetArea using TBB
// task_arena + task_group. Mirrors AsyncFlowSolver: UI submits, polls once
// per frame, cancels cooperatively via a generation counter.
class AsyncGeometryOptimizer
{
public:
  AsyncGeometryOptimizer();
  ~AsyncGeometryOptimizer() noexcept;
  AsyncGeometryOptimizer(const AsyncGeometryOptimizer&) = delete;
  AsyncGeometryOptimizer& operator=(const AsyncGeometryOptimizer&) = delete;
  AsyncGeometryOptimizer(AsyncGeometryOptimizer&&) = delete;
  AsyncGeometryOptimizer& operator=(AsyncGeometryOptimizer&&) = delete;

  void submit(PumpParams initialParams,
              TargetAreaCurve target,
              GeometryDesignBounds bounds,
              GeometryOptimizationSettings settings) noexcept;

  // Consumes the pending result (if any). Returns true once per successful
  // run; caller takes ownership of the result via `out`.
  bool poll(GeometryOptimizationResult& out) noexcept;

  void cancelAndWait() noexcept;

  [[nodiscard]] SolverStatus status() const noexcept;
  [[nodiscard]] std::chrono::milliseconds lastDuration() const noexcept;
  [[nodiscard]] std::optional<CoreError> lastError() const noexcept;

private:
  void workerRun(std::uint64_t jobGen,
                 const PumpParams& initialParams,
                 const TargetAreaCurve& target,
                 const GeometryDesignBounds& bounds,
                 const GeometryOptimizationSettings& settings) noexcept;

  tbb::task_arena arena_;
  tbb::task_group group_;

  std::atomic<std::uint64_t> currentGen_{0};

  std::mutex mutex_;
  std::shared_ptr<GeometryOptimizationResult> pendingResult_;

  mutable std::mutex stateMutex_;
  SolverStatus status_{SolverStatus::Idle};
  std::int64_t lastDurationMs_{0};
  std::optional<CoreError> lastError_;
};

} // namespace ggm::core
