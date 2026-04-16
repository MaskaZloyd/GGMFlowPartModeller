#include "gui/solver_status_display.hpp"

namespace ggm::gui {

StatusDisplay solverStatusBar(core::SolverStatus status) noexcept {
  switch (status) {
    case core::SolverStatus::Idle:
      return {{0.45F, 0.48F, 0.54F, 1.0F}, "ожидание"};
    case core::SolverStatus::Running:
      return {{0.85F, 0.58F, 0.10F, 1.0F}, "расчёт…"};
    case core::SolverStatus::Success:
      return {{0.12F, 0.70F, 0.30F, 1.0F}, "готово"};
    case core::SolverStatus::Failed:
      return {{0.85F, 0.22F, 0.18F, 1.0F}, "ошибка"};
    case core::SolverStatus::Cancelled:
      return {{0.55F, 0.48F, 0.22F, 1.0F}, "отменено"};
  }
  return {{0.2F, 0.2F, 0.2F, 1.0F}, "?"};
}

StatusDisplay solverStatusPanel(core::SolverStatus status) noexcept {
  switch (status) {
    case core::SolverStatus::Idle:
      return {{0.65F, 0.65F, 0.65F, 1.0F}, "ожидание"};
    case core::SolverStatus::Running:
      return {{1.0F, 0.72F, 0.15F, 1.0F}, "расчёт..."};
    case core::SolverStatus::Success:
      return {{0.25F, 0.85F, 0.35F, 1.0F}, "готово"};
    case core::SolverStatus::Failed:
      return {{0.95F, 0.30F, 0.25F, 1.0F}, "ошибка"};
    case core::SolverStatus::Cancelled:
      return {{0.65F, 0.50F, 0.20F, 1.0F}, "отменено"};
  }
  return {{1.0F, 1.0F, 1.0F, 1.0F}, "?"};
}

} // namespace ggm::gui
