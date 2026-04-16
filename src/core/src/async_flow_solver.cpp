#include "core/async_flow_solver.hpp"

#include "core/logging.hpp"

namespace ggm::core {

AsyncFlowSolver::AsyncFlowSolver() = default;

AsyncFlowSolver::~AsyncFlowSolver() noexcept
{
  cancelAndWait();
}

void
AsyncFlowSolver::submit(MeridionalGeometry geom,
                        PumpParams params,
                        ComputationSettings settings) noexcept
{
  // Increment generation to signal cancellation for any in-flight job.
  std::uint64_t newGen = currentGen_.fetch_add(1, std::memory_order_release) + 1;

  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    status_ = SolverStatus::Running;
  }
  logging::solver()->debug("submit gen={}", newGen);

  // Wrap geometry in shared_ptr so the lambda stays const-callable (TBB requires).
  auto geomPtr = std::make_shared<MeridionalGeometry>(std::move(geom));

  // Run inside the arena so all workers use the arena's thread pool, and
  // associate with group_ so cancelAndWait() can block properly.
  arena_.execute([this, newGen, geomPtr, params, settings]() {
    group_.run([this, newGen, geomPtr, params, settings]() {
      workerRun(newGen, *geomPtr, params, settings);
    });
  });
}

void
AsyncFlowSolver::workerRun(std::uint64_t jobGen,
                           const MeridionalGeometry& geom,
                           const PumpParams& params,
                           const ComputationSettings& settings) noexcept
{
  logging::solver()->debug("worker start gen={}", jobGen);
  auto start = std::chrono::steady_clock::now();

  auto isCancelled = [this, jobGen]() -> bool {
    return currentGen_.load(std::memory_order_acquire) != jobGen;
  };

  // Early exit if already superseded before we even started.
  if (isCancelled()) {
    logging::solver()->debug("early cancel gen={}", jobGen);
    return;
  }

  FlowSolver solver;
  solver.setConfig(settings);
  auto result = solver.solve(geom, params, isCancelled);

  // Final cancellation check before publishing result.
  if (isCancelled()) {
    return;
  }

  auto elapsed =
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    lastDurationMs_ = elapsed.count();
  }

  if (result) {
    auto pending = std::make_shared<FlowResults>(std::move(*result));
    {
      std::lock_guard<std::mutex> lock(mutex_);
      pendingResult_ = std::move(pending);
    }
    if (!isCancelled()) {
      std::lock_guard<std::mutex> lock(stateMutex_);
      status_ = SolverStatus::Success;
    }
    logging::solver()->info("gen={} готово за {} мс", jobGen, elapsed.count());
  } else {
    auto err = result.error();
    if (!isCancelled()) {
      std::lock_guard<std::mutex> lock(stateMutex_);
      status_ = (err == CoreError::Cancelled) ? SolverStatus::Cancelled : SolverStatus::Failed;
    }
    logging::solver()->error("gen={} ошибка: {}", jobGen, toString(err));
  }
}

SolverStatus
AsyncFlowSolver::status() const noexcept
{
  std::lock_guard<std::mutex> lock(stateMutex_);
  return status_;
}

std::chrono::milliseconds
AsyncFlowSolver::lastDuration() const noexcept
{
  std::lock_guard<std::mutex> lock(stateMutex_);
  return std::chrono::milliseconds(lastDurationMs_);
}

bool
AsyncFlowSolver::poll() noexcept
{
  std::shared_ptr<FlowResults> fresh;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pendingResult_) {
      fresh = std::move(pendingResult_);
      pendingResult_.reset();
    }
  }
  if (fresh) {
    std::lock_guard<std::mutex> lock(resultMutex_);
    result_ = std::move(fresh);
    return true;
  }
  return false;
}

std::shared_ptr<const FlowResults>
AsyncFlowSolver::snapshot() const noexcept
{
  std::lock_guard<std::mutex> lock(resultMutex_);
  return result_;
}

void
AsyncFlowSolver::cancelAndWait() noexcept
{
  // Increment gen → in-flight workers will see mismatch at next check.
  currentGen_.fetch_add(1, std::memory_order_release);
  // Wait for any enqueued work in the arena to finish.
  arena_.execute([this]() { group_.wait(); });
  // Discard any pending result from a cancelled job.
  std::lock_guard<std::mutex> lock(mutex_);
  pendingResult_.reset();
  {
    std::lock_guard<std::mutex> stateLock(stateMutex_);
    if (status_ == SolverStatus::Running) {
      status_ = SolverStatus::Cancelled;
    }
  }
}

} // namespace ggm::core
