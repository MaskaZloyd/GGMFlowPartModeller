#include "gui/panels/params_panel.hpp"

#include "gui/ui/panel_style.hpp"
#include "layout/dock_utils.hpp"

#include <algorithm>
#include <cmath>

#include <imgui.h>

namespace ggm::gui {

namespace {

// Thin wrapper around style::fixedDragDouble that pipes widget activation into
// ParamsPanelState so we can batch the edit into a single undoable command.
bool
paramDrag(const char* id,
          const char* labelText,
          double* value,
          const double* minVal,
          const double* maxVal,
          ParamsPanelState& state,
          const char* tooltip = nullptr)
{
  const auto result = style::fixedDragDouble(id, labelText, value, minVal, maxVal,
                                          style::PANEL_DRAG_FORMAT_DOUBLE,
                                          style::PANEL_DRAG_SPEED_DEFAULT,
                                          tooltip);
  if (result.activated && !state.active) {
    state.active = true;
  }
  return result.changed;
}

} // namespace

ParamsPanelResult
drawParamsPanel(const core::PumpParams& current,
                ParamsPanelState& state,
                ImGuiID dockspaceId) noexcept
{
  ParamsPanelResult result;
  result.liveParams = current;

  prepareDockedWindow("Параметры", dockspaceId);
  style::pushPanelStyle();
  ImGui::Begin("Параметры");

  if (!state.active) {
    state.beforeEdit = current;
  }

  auto& params = result.liveParams;

  static constexpr double MIN_POSITIVE = 0.001;
  static constexpr double MIN_ZERO = 0.0;
  static constexpr double MAX_ZERO = 0.0;
  static constexpr double MIN_ANGLE_NEG = -89.0;
  static constexpr double MAX_ANGLE_POS = 89.0;
  static constexpr double MAX_AL02 = -0.001;

  if (ImGui::CollapsingHeader("Общие размеры", ImGuiTreeNodeFlags_DefaultOpen)) {
    paramDrag("xa",
              "xa — осевой отступ, мм",
              &params.xa,
              nullptr,
              nullptr,
              state,
              "Осевой отступ входного участка");
    paramDrag("dvt",
              "dvt — диаметр втулки, мм",
              &params.dvt,
              &MIN_POSITIVE,
              nullptr,
              state,
              "Диаметр втулки (вала)");
    paramDrag("d2",
              "d2 — диаметр выхода, мм",
              &params.d2,
              &MIN_POSITIVE,
              nullptr,
              state,
              "Диаметр выхода рабочего колеса");
    paramDrag("b2",
              "b2 — ширина выхода, мм",
              &params.b2,
              &MIN_POSITIVE,
              nullptr,
              state,
              "Осевая ширина канала на выходе");
    paramDrag("din",
              "din — диаметр входа, мм",
              &params.din,
              &MIN_POSITIVE,
              nullptr,
              state,
              "Диаметр входного патрубка");
  }

  if (ImGui::CollapsingHeader("Дуги втулки", ImGuiTreeNodeFlags_DefaultOpen)) {
    paramDrag("r1",
              "r1 — радиус дуги 1, мм",
              &params.r1,
              &MIN_POSITIVE,
              nullptr,
              state,
              "Радиус первой дуги втулки");
    paramDrag("r2",
              "r2 — радиус дуги 2, мм",
              &params.r2,
              &MIN_POSITIVE,
              nullptr,
              state,
              "Радиус второй дуги втулки");
    paramDrag("al1",
              "al1 — наклон выхода, °",
              &params.al1Deg,
              &MIN_ZERO,
              &MAX_ANGLE_POS,
              state,
              "Угол наклона выходного участка втулки (≥ 0)");
    paramDrag("be1",
              "be1 — охват дуги 1, °",
              &params.be1Deg,
              &MIN_POSITIVE,
              &MAX_ANGLE_POS,
              state,
              "Угловой охват первой дуги втулки");

    double be2Deg = 90.0 - params.be1Deg + params.al1Deg;
    ImGui::TextDisabled("be2 = %.2f° (вычисленный)", be2Deg);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("be2 = 90 − be1 + al1. Охват второй дуги для G1-непрерывности");
    }
  }

  if (ImGui::CollapsingHeader("Дуги покрывного диска", ImGuiTreeNodeFlags_DefaultOpen)) {
    paramDrag("r3",
              "r3 — радиус дуги 3, мм",
              &params.r3,
              &MIN_POSITIVE,
              nullptr,
              state,
              "Радиус первой дуги покрывного диска");
    paramDrag("r4",
              "r4 — радиус дуги 4, мм",
              &params.r4,
              &MIN_POSITIVE,
              nullptr,
              state,
              "Радиус второй дуги покрывного диска");
    paramDrag("al2",
              "al2 — наклон выхода, °",
              &params.al2Deg,
              &MIN_ANGLE_NEG,
              &MAX_ZERO,
              state,
              "Угол наклона выходного участка покр. диска (≤ 0)");
    paramDrag("al02",
              "al02 — наклон горла, °",
              &params.al02Deg,
              &MIN_ANGLE_NEG,
              &MAX_AL02,
              state,
              "Угол наклона входного участка покр. диска (< 0)");
    paramDrag("be3",
              "be3 — охват дуги 3, °",
              &params.be3RawDeg,
              &MIN_POSITIVE,
              &MAX_ANGLE_POS,
              state,
              "Угловой охват первой дуги покр. диска (до корр.)");

    double be3Eff = params.be3RawDeg - std::abs(params.al02Deg);
    double be4Deg = 90.0 - be3Eff + params.al2Deg;
    ImGui::TextDisabled("be3_eff = %.2f°, be4 = %.2f° (вычисленный)", be3Eff, be4Deg);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("be3_eff = be3 − |al02|. be4 = 90 − be3_eff + al2");
    }
  }

  bool editFinished = state.active && !ImGui::IsAnyItemActive();
  if (editFinished && !(params == current)) {
    result.finishedEdit = EditCommand{
      .before = state.beforeEdit,
      .after = params,
      .label = "Изменение параметра",
    };
    state.active = false;
  } else if (editFinished) {
    state.active = false;
  }

  ImGui::End();
  style::popPanelStyle();

  return result;
}

} // namespace ggm::gui
