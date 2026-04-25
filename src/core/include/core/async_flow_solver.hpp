#pragma once

#include "core/computation_settings.hpp"
#include "core/error.hpp"
#include "core/flow_solver.hpp"
#include "core/flow_solver_types.hpp"
#include "core/geometry.hpp"
#include "core/pump_params.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

#include <tbb/task_arena.h>
#include <tbb/task_group.h>

namespace ggm::core {

enum class SolverStatus
{
  Idle,
  Running,
  Success,
  Failed,
  Cancelled,
};

/// Asynchronous wrapper around FlowSolver using TBB task_arena + task_group.
///
/// Usage pattern (from UI thread):
///   - submit(geometry, params, settings) — cancels any in-flight job, queues new one.
///   - poll() — call every frame; returns true if a new result landed.
///   - lastResult() — const access to the most recent successful result.
///   - status() — Idle / Running / Success / Failed / Cancelled.
///
/// Cancellation: a pending job is cancelled cooperatively at pipeline stage
/// boundaries via a generation-counter based predicate.
class AsyncFlowSolver
{
public:
  AsyncFlowSolver();
  ~AsyncFlowSolver() noexcept;
  AsyncFlowSolver(const AsyncFlowSolver&) = delete;
  AsyncFlowSolver& operator=(const AsyncFlowSolver&) = delete;
  AsyncFlowSolver(AsyncFlowSolver&&) = delete;
  AsyncFlowSolver& operator=(AsyncFlowSolver&&) = delete;

  /// Submit a new job. Cancels any in-flight job.
  void submit(MeridionalGeometry geom, PumpParams params, ComputationSettings settings) noexcept;

  /// Returns true if a new successful result was applied this call.
  /// Call from UI thread every frame.
  bool poll() noexcept;

  /// Cancel and wait for running job to finish. Safe to call from UI thread.
  void cancelAndWait() noexcept;

  [[nodiscard]] SolverStatus status() const noexcept;

  /// Snapshot the current result. Returns a shared_ptr whose lifetime
  /// extends as long as the caller holds it — safe to use across frame work
  /// even if the worker publishes a new result mid-frame.
  [[nodiscard]] std::shared_ptr<const FlowResults> snapshot() const noexcept;

  [[nodiscard]] std::chrono::milliseconds lastDuration() const noexcept;

private:
  void workerRun(std::uint64_t jobGen,
                 const MeridionalGeometry& geom,
                 const PumpParams& params,
                 const ComputationSettings& settings) noexcept;

  tbb::task_arena arena_;
  tbb::task_group group_;

  /// Generation counter: monotonic. submit() increments; workers compare
  /// against currentGen_ to detect cancellation.
  std::atomic<std::uint64_t> currentGen_{0};

  /// Pending result from a completed worker, guarded by mutex_.
  std::mutex mutex_;
  std::shared_ptr<FlowResults> pendingResult_;

  /// Last successful result, guarded by resultMutex_. Callers get a
  /// shared_ptr snapshot so they can hold it across frame work safely.
  mutable std::mutex resultMutex_;
  std::shared_ptr<FlowResults> result_;
  /// Status and last-duration are protected by stateMutex_ instead of
  /// being atomics — this sidesteps any ABI trouble and guarantees gui
  /// and core translation units see the same field layout.
  mutable std::mutex stateMutex_;
  SolverStatus status_{SolverStatus::Idle};
  std::int64_t lastDurationMs_{0};
};

}
