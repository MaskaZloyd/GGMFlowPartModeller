#include "core/async_geometry_optimizer.hpp"

#include "core/logging.hpp"

#include <utility>

namespace ggm::core {

AsyncGeometryOptimizer::AsyncGeometryOptimizer() = default;

AsyncGeometryOptimizer::~AsyncGeometryOptimizer() noexcept
{
  cancelAndWait();
}

void
AsyncGeometryOptimizer::submit(PumpParams initialParams,
                               TargetAreaCurve target,
                               GeometryDesignBounds bounds,
                               GeometryOptimizationSettings settings) noexcept
{
  std::uint64_t newGen = currentGen_.fetch_add(1, std::memory_order_release) + 1;

  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    status_ = SolverStatus::Running;
    lastError_.reset();
  }
  logging::solver()->debug("geom-opt submit gen={}", newGen);

  auto initialPtr = std::make_shared<PumpParams>(initialParams);
  auto targetPtr = std::make_shared<TargetAreaCurve>(std::move(target));
  auto boundsPtr = std::make_shared<GeometryDesignBounds>(bounds);
  auto settingsPtr = std::make_shared<GeometryOptimizationSettings>(settings);

  arena_.execute([this, newGen, initialPtr, targetPtr, boundsPtr, settingsPtr]() {
    group_.run([this, newGen, initialPtr, targetPtr, boundsPtr, settingsPtr]() {
      workerRun(newGen, *initialPtr, *targetPtr, *boundsPtr, *settingsPtr);
    });
  });
}

void
AsyncGeometryOptimizer::workerRun(std::uint64_t jobGen,
                                  const PumpParams& initialParams,
                                  const TargetAreaCurve& target,
                                  const GeometryDesignBounds& bounds,
                                  const GeometryOptimizationSettings& settings) noexcept
{
  logging::solver()->debug("geom-opt worker start gen={}", jobGen);
  auto start = std::chrono::steady_clock::now();

  auto isCancelled = [this, jobGen]() -> bool {
    return currentGen_.load(std::memory_order_acquire) != jobGen;
  };

  if (isCancelled()) {
    return;
  }

  auto result = optimizeGeometryForTargetArea(initialParams, target, bounds, settings, isCancelled);

  if (isCancelled()) {
    return;
  }

  auto elapsed =
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

  if (result) {
    auto pending = std::make_shared<GeometryOptimizationResult>(std::move(*result));
    {
      std::lock_guard<std::mutex> lock(mutex_);
      pendingResult_ = std::move(pending);
    }
    {
      std::lock_guard<std::mutex> lock(stateMutex_);
      lastDurationMs_ = elapsed.count();
      lastError_.reset();
      status_ = SolverStatus::Success;
    }
    logging::solver()->info("geom-opt gen={} готово за {} мс", jobGen, elapsed.count());
  } else {
    auto err = result.error();
    {
      std::lock_guard<std::mutex> lock(stateMutex_);
      lastDurationMs_ = elapsed.count();
      lastError_ = err;
      status_ = (err == CoreError::Cancelled) ? SolverStatus::Cancelled : SolverStatus::Failed;
    }
    logging::solver()->error("geom-opt gen={} ошибка: {}", jobGen, toString(err));
  }
}

bool
AsyncGeometryOptimizer::poll(GeometryOptimizationResult& out) noexcept
{
  std::shared_ptr<GeometryOptimizationResult> fresh;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pendingResult_) {
      fresh = std::move(pendingResult_);
      pendingResult_.reset();
    }
  }
  if (!fresh) {
    return false;
  }
  out = std::move(*fresh);
  return true;
}

SolverStatus
AsyncGeometryOptimizer::status() const noexcept
{
  std::lock_guard<std::mutex> lock(stateMutex_);
  return status_;
}

std::chrono::milliseconds
AsyncGeometryOptimizer::lastDuration() const noexcept
{
  std::lock_guard<std::mutex> lock(stateMutex_);
  return std::chrono::milliseconds(lastDurationMs_);
}

std::optional<CoreError>
AsyncGeometryOptimizer::lastError() const noexcept
{
  std::lock_guard<std::mutex> lock(stateMutex_);
  return lastError_;
}

void
AsyncGeometryOptimizer::cancelAndWait() noexcept
{
  currentGen_.fetch_add(1, std::memory_order_release);
  arena_.execute([this]() { group_.wait(); });
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pendingResult_.reset();
  }
  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (status_ == SolverStatus::Running) {
      status_ = SolverStatus::Cancelled;
    }
  }
}

} // namespace ggm::core
