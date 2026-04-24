#include "gui/panels/reverse_design_panel.hpp"

#include "core/error.hpp"
#include "gui/panels/geometry_panel.hpp"
#include "gui/ui/panel_style.hpp"
#include "layout/dock_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include <imgui.h>
#include <implot.h>

namespace ggm::gui {

namespace {

constexpr const char* kControlsWindowTitle = "Настройки обратного проектирования";
constexpr const char* kCurveWindowTitle = "Целевая кривая площади";
constexpr const char* kComparisonWindowTitle = "Сравнение площади";
constexpr const char* kGeometryWindowTitle = "Геометрия оптимизации";

constexpr float BOUNDS_INPUT_WIDTH = 92.0F;

void
setDefaultCurveIfNeeded(ui::AreaCurveEditorState& editorState)
{
  if (editorState.targetCurve.isValid()) {
    return;
  }

  editorState.targetCurve.setPoints({
    {0.0, 0.55},
    {0.40, 0.78},
    {0.75, 0.92},
    {1.0, 1.00},
  });
}

void
drawStatusLine(const ReverseDesignPanelState& state, const core::SolverStatus optimizerStatus)
{
  ImVec4 color = style::PANEL_COLOR_INFO;
  std::string text;

  switch (optimizerStatus) {
    case core::SolverStatus::Running:
      color = style::PANEL_COLOR_WARN;
      text = "Идёт оптимизация…";
      break;
    case core::SolverStatus::Success:
      color = style::PANEL_COLOR_SUCCESS;
      text = state.statusMessage.empty() ? "Готово" : state.statusMessage;
      break;
    case core::SolverStatus::Failed:
      color = style::PANEL_COLOR_ERROR;
      text = state.statusMessage.empty() ? "Ошибка оптимизации" : state.statusMessage;
      break;
    case core::SolverStatus::Cancelled:
      color = style::PANEL_COLOR_WARN;
      text = state.statusMessage.empty() ? "Отменено" : state.statusMessage;
      break;
    case core::SolverStatus::Idle:
    default:
      if (state.statusMessage.empty()) {
        return;
      }
      text = state.statusMessage;
      break;
  }

  style::drawColoredStatusLine(color, text.c_str());
}

// Local wrappers that hide the DragResult → bool conversion and let call
// sites stay short.
bool
fixedDrag(const char* id,
          const char* label,
          double* value,
          const double* minVal,
          const double* maxVal,
          const char* format = style::PANEL_DRAG_FORMAT_DOUBLE,
          float speed = style::PANEL_DRAG_SPEED_DEFAULT,
          const char* tooltip = nullptr) noexcept
{
  return style::fixedDragDouble(id, label, value, minVal, maxVal, format, speed, tooltip).changed;
}

bool
fixedDragInt(const char* id,
             const char* label,
             int* value,
             int minVal,
             int maxVal,
             const char* tooltip = nullptr) noexcept
{
  return style::fixedDragInt(id, label, value, minVal, maxVal, tooltip).changed;
}

void
labelValueRow(const char* label, const char* valueFmt, ...) noexcept
{
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::TextDisabled("%s", label);
  ImGui::TableSetColumnIndex(1);
  va_list args;
  va_start(args, valueFmt);
  ImGui::TextV(valueFmt, args);
  va_end(args);
}

void
clearPreview(ReverseDesignPanelState& state)
{
  state.hasPreview = false;
  state.hasOptimizationResult = false;
  state.lastOptimizationConverged = false;
  state.lastOptimizationGenerations = 0;
  state.currentObjective = {};
  state.previewGeometry = {};
  state.previewAreaProfile = {};
  state.comparisonReferenceOutletArea = 0.0;
  state.xiSamples.clear();
  state.targetSamples.clear();
  state.currentSamples.clear();
  state.residualSamples.clear();
}

void
updateComparisonSamples(ReverseDesignPanelState& state)
{
  state.xiSamples.clear();
  state.targetSamples.clear();
  state.currentSamples.clear();
  state.residualSamples.clear();

  auto currentSamples =
    core::normalizedAreaSamples(state.previewAreaProfile, state.optimizationSettings.sampleCount);
  if (!currentSamples) {
    state.statusMessage =
      std::string("Ошибка нормализации профиля площади: ") + core::toString(currentSamples.error());
    return;
  }

  const auto sampleCount = static_cast<std::size_t>(state.optimizationSettings.sampleCount);
  state.xiSamples.resize(sampleCount, 0.0);
  state.targetSamples.resize(sampleCount, 0.0);
  state.currentSamples = std::move(*currentSamples);
  state.residualSamples.resize(sampleCount, 0.0);

  const double currentOutletArea =
    state.previewAreaProfile.flowAreas.empty() ? 0.0 : state.previewAreaProfile.flowAreas.back();
  const bool useReference = state.comparisonReferenceOutletArea > 0.0 &&
                            std::isfinite(state.comparisonReferenceOutletArea) &&
                            std::isfinite(currentOutletArea) && currentOutletArea > 0.0;
  const double currentScale =
    useReference ? currentOutletArea / state.comparisonReferenceOutletArea : 1.0;

  for (std::size_t i = 0; i < sampleCount; ++i) {
    const double xi = static_cast<double>(i) / static_cast<double>(sampleCount - 1U);
    const double targetValue = state.editorState.targetCurve.evaluate(xi);
    state.xiSamples[i] = xi;
    state.targetSamples[i] = targetValue;
    state.currentSamples[i] *= currentScale;
    state.residualSamples[i] = state.currentSamples[i] - targetValue;
  }
}

void
applyPreviewResult(ReverseDesignPanelState& state,
                   core::GeometryOptimizationResult result,
                   const bool fromOptimization)
{
  state.currentParams = result.params;
  state.previewGeometry = std::move(result.geometry);
  state.previewAreaProfile = std::move(result.areaProfile);
  state.currentObjective = result.objective;
  state.hasCurrentParams = true;
  state.hasPreview = true;
  state.hasOptimizationResult = fromOptimization;
  state.lastOptimizationConverged = fromOptimization && result.converged;
  state.lastOptimizationGenerations = fromOptimization ? result.generations : 0;
  updateComparisonSamples(state);
}

void
importFromMeridional(ReverseDesignPanelState& state, const core::PumpParams& meridionalParams)
{
  state.currentParams = meridionalParams;
  state.hasCurrentParams = true;
  state.bounds = core::makeBoundsFromValues(meridionalParams);
  clearPreview(state);
  state.statusMessage = "Геометрия импортирована из меридианного модуля.";
}

void
previewCurrentGeometry(ReverseDesignPanelState& state)
{
  if (!state.hasCurrentParams) {
    state.statusMessage = "Сначала импортируйте геометрию из меридианного модуля.";
    return;
  }

  auto previewSettings = state.optimizationSettings;
  previewSettings.referenceOutletArea =
    std::numbers::pi * state.currentParams.d2 * state.currentParams.b2;
  state.comparisonReferenceOutletArea = previewSettings.referenceOutletArea;

  auto previewResult = core::evaluateGeometryCandidate(
    state.currentParams, state.editorState.targetCurve, previewSettings);
  if (!previewResult) {
    clearPreview(state);
    state.statusMessage = std::string("Не удалось построить превью геометрии: ") +
                          core::toString(previewResult.error());
    return;
  }

  applyPreviewResult(state, std::move(*previewResult), false);
  state.statusMessage = "Превью геометрии обновлено.";
}

void
submitOptimization(ReverseDesignPanelState& state)
{
  if (!state.hasCurrentParams) {
    state.statusMessage = "Сначала импортируйте геометрию из меридианного модуля.";
    return;
  }

  // Anchor absolute scale: outlet flow area ≈ π·D₂·b₂ (cylindrical periphery).
  auto optimizationSettings = state.optimizationSettings;
  optimizationSettings.referenceOutletArea =
    std::numbers::pi * state.currentParams.d2 * state.currentParams.b2;
  state.comparisonReferenceOutletArea = optimizationSettings.referenceOutletArea;

  state.asyncOptimizer->submit(
    state.currentParams, state.editorState.targetCurve, state.bounds, optimizationSettings);
  state.statusMessage = "Оптимизация запущена…";
}

void
pollOptimization(ReverseDesignPanelState& state)
{
  core::GeometryOptimizationResult result;
  if (!state.asyncOptimizer->poll(result)) {
    const auto status = state.asyncOptimizer->status();
    if (status == core::SolverStatus::Failed) {
      const auto err = state.asyncOptimizer->lastError();
      if (err) {
        state.statusMessage =
          std::string("Оптимизация завершилась ошибкой: ") + core::toString(*err);
      }
    } else if (status == core::SolverStatus::Cancelled) {
      state.statusMessage = "Оптимизация отменена.";
    }
    return;
  }

  applyPreviewResult(state, std::move(result), true);
  const auto elapsed = state.asyncOptimizer->lastDuration();
  state.statusMessage = std::string("Оптимизация завершена за ") +
                        std::to_string(elapsed.count()) + std::string(" мс.");
}

void
drawBoundsEditor(ReverseDesignPanelState& state, const bool disabled)
{
  const auto variables = core::allDesignVariables();

  ImGui::BeginDisabled(disabled);

  if (ImGui::Button("Сбросить границы (±50 %)", ImVec2(-1, 0.0F))) {
    state.bounds = core::makeBoundsFromValues(state.currentParams);
  }

  ImGui::Spacing();

  const ImGuiTableFlags tableFlags =
    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit;

  if (ImGui::BeginTable("##bounds", 4, tableFlags)) {
    ImGui::TableSetupColumn("opt", ImGuiTableColumnFlags_WidthFixed, 40.0F);
    ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, 56.0F);
    ImGui::TableSetupColumn("min", ImGuiTableColumnFlags_WidthFixed, BOUNDS_INPUT_WIDTH);
    ImGui::TableSetupColumn("max", ImGuiTableColumnFlags_WidthFixed, BOUNDS_INPUT_WIDTH);
    ImGui::TableHeadersRow();

    for (const auto& var : variables) {
      ImGui::TableNextRow();

      ImGui::PushID(var.label);

      ImGui::TableSetColumnIndex(0);
      ImGui::Checkbox("##opt", &(state.optimizationSettings.mask.*(var.enabled)));

      ImGui::TableSetColumnIndex(1);
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(var.label);

      double& minValue = state.bounds.*(var.minBound);
      double& maxValue = state.bounds.*(var.maxBound);

      ImGui::TableSetColumnIndex(2);
      ImGui::SetNextItemWidth(BOUNDS_INPUT_WIDTH);
      if (ImGui::DragScalar(
            "##min", ImGuiDataType_Double, &minValue, 0.25F, nullptr, nullptr, "%.2f")) {
        if (maxValue <= minValue) {
          maxValue = minValue + std::max(std::abs(minValue) * 0.1, 1.0);
        }
      }

      ImGui::TableSetColumnIndex(3);
      ImGui::SetNextItemWidth(BOUNDS_INPUT_WIDTH);
      if (ImGui::DragScalar(
            "##max", ImGuiDataType_Double, &maxValue, 0.25F, nullptr, nullptr, "%.2f")) {
        if (maxValue <= minValue) {
          minValue = maxValue - std::max(std::abs(maxValue) * 0.1, 1.0);
        }
      }

      ImGui::PopID();
    }
    ImGui::EndTable();
  }

  ImGui::EndDisabled();
}

[[nodiscard]] std::optional<core::PumpParams>
drawSourceSection(ReverseDesignPanelState& state,
                  const core::PumpParams& meridionalParams,
                  const bool optimizing)
{
  if (style::accentButton("Импортировать геометрию из меридианного модуля", optimizing)) {
    importFromMeridional(state, meridionalParams);
    previewCurrentGeometry(state);
  }

  std::optional<core::PumpParams> paramsForMeridional;
  const bool canApplyToMeridional = state.hasCurrentParams && !optimizing;
  if (style::accentButton("Передать геометрию в меридианное сечение", !canApplyToMeridional)) {
    paramsForMeridional = state.currentParams;
    state.statusMessage = "Геометрия передана в меридианное сечение.";
  }

  ImGui::Spacing();

  if (!state.hasCurrentParams) {
    ImGui::TextColored(style::PANEL_COLOR_INFO,
                       "Геометрия пока не импортирована. Нажмите кнопку выше, чтобы забрать "
                       "параметры из вкладки \"Меридианное сечение\".");
    return paramsForMeridional;
  }

  const ImGuiTableFlags tableFlags =
    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit;
  if (ImGui::BeginTable("##imported_sizes", 4, tableFlags)) {
    ImGui::TableSetupColumn("D₂", ImGuiTableColumnFlags_WidthFixed, 78.0F);
    ImGui::TableSetupColumn("Dvt", ImGuiTableColumnFlags_WidthFixed, 78.0F);
    ImGui::TableSetupColumn("din", ImGuiTableColumnFlags_WidthFixed, 78.0F);
    ImGui::TableSetupColumn("xa", ImGuiTableColumnFlags_WidthFixed, 78.0F);
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%.2f", state.currentParams.d2);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%.2f", state.currentParams.dvt);
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("%.2f", state.currentParams.din);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%.2f", state.currentParams.xa);
    ImGui::EndTable();
  }
  ImGui::TextDisabled(
    "Размеры (мм). D₂, Dvt, xa фиксируются; din можно включить в оптимизацию (din > Dvt).");

  return paramsForMeridional;
}

void
drawActionButtons(ReverseDesignPanelState& state, const bool optimizing)
{
  const bool canAct = state.hasCurrentParams && !optimizing;

  if (style::accentButton("Запустить оптимизацию", !canAct)) {
    state.editorState.requestOptimization = true;
  }

  ImGui::Spacing();

  if (style::dangerButton("Отменить оптимизацию", !optimizing)) {
    state.asyncOptimizer->cancelAndWait();
    state.statusMessage = "Оптимизация отменена.";
  }

  ImGui::Spacing();

  ImGui::BeginDisabled(!canAct);
  if (ImGui::Button("Обновить превью (FEM)", ImVec2(-1, 0.0F))) {
    previewCurrentGeometry(state);
  }
  ImGui::EndDisabled();
}

void
drawOptimizationSettings(ReverseDesignPanelState& state)
{
  auto& opt = state.optimizationSettings;

  ImGui::TextDisabled("Параметры CMA-ES");
  fixedDragInt(
    "sample", "число sample", &opt.sampleCount, 16, 160, "Число узлов для целевой кривой");
  fixedDragInt("pop",
               "population λ",
               &opt.populationSize,
               0,
               256,
               "0 = авто: 4 + ⌊3·ln N⌋, где N — число активных переменных");
  fixedDragInt(
    "gen", "generations", &opt.maxGenerations, 1, 500, "Число поколений CMA-ES");
  fixedDragInt("polish",
               "LM polish",
               &opt.localPolishIterations,
               0,
               20,
               "Локальные итерации bounded least-squares после CMA-ES");

  constexpr double SIGMA_MIN = 0.01;
  constexpr double SIGMA_MAX = 1.0;
  constexpr double ZERO = 0.0;
  constexpr double ONE = 1.0;
  constexpr double AREA_MAX = 10.0;
  constexpr double SHAPE_MAX = 100.0;
  constexpr double POINT_MAX = 20.0;
  constexpr double SMOOTH_MAX = 0.05;
  constexpr double CONS_MAX = 10000.0;
  constexpr const char* kAreaWeightTooltip =
    "Основной вес среднеквадратичной ошибки площади: F_current - F_target.\n"
    "Увеличивайте, если нужно точнее попасть в целевую кривую площади.";
  constexpr const char* kTargetPointWeightTooltip =
    "Дополнительный вес опорных точек целевой кривой.\n"
    "Увеличивайте, если оптимизация хорошо идет между точками, но промахивается в заданных "
    "точках.";
  constexpr const char* kSlopeWeightTooltip =
    "Вес штрафа за изменение наклона residual между соседними xi.\n"
    "Увеличивайте, чтобы подавить волны ошибки; слишком большой вес может ухудшить попадание "
    "по абсолютной площади.";
  constexpr const char* kMonotonicityWeightTooltip =
    "Вес штрафа за локальное падение текущей площади там, где целевая кривая не убывает.\n"
    "Увеличивайте при провалах и перегибах площади; слишком большой вес делает поиск жестче.";
  constexpr const char* kSmoothnessWeightTooltip =
    "Вес гладкости геометрии: штрафует вторые разности точек hub и shroud.\n"
    "Увеличивайте, если стенки становятся слишком резкими; слишком большой вес сглаживает "
    "ценой точности площади.";
  constexpr const char* kConstraintWeightTooltip =
    "Вес технологических и геометрических ограничений: зазоры, радиальные границы, "
    "положительная площадь, корректные хорды.\n"
    "Увеличивайте, если оптимизатор находит математически хороший, но недопустимый вариант.";
  constexpr const char* kInvalidChordTooltip =
    "Допустимая доля некорректных хорд профиля площади до усиленного штрафа.\n"
    "Меньшее значение строже отбрасывает сомнительную геометрию.";

  fixedDrag("sigma",
            "σ₀ (доля границы)",
            &opt.sigmaInitial,
            &SIGMA_MIN,
            &SIGMA_MAX,
            "%.2f",
            0.01F,
            "Начальный шаг CMA-ES как доля ширины [min, max] каждой переменной");

  ImGui::Spacing();
  ImGui::TextDisabled("Веса в целевой функции");
  fixedDrag("aw",
            "area weight",
            &opt.areaWeight,
            &ZERO,
            &AREA_MAX,
            "%.3f",
            0.01F,
            kAreaWeightTooltip);
  fixedDrag("tpw",
            "target point weight",
            &opt.targetPointWeight,
            &ZERO,
            &POINT_MAX,
            "%.2f",
            0.05F,
            kTargetPointWeightTooltip);
  fixedDrag("slope",
            "slope weight",
            &opt.residualSlopeWeight,
            &ZERO,
            &SHAPE_MAX,
            "%.3f",
            0.01F,
            kSlopeWeightTooltip);
  fixedDrag("mono",
            "monotonicity weight",
            &opt.monotonicityWeight,
            &ZERO,
            &SHAPE_MAX,
            "%.3f",
            0.05F,
            kMonotonicityWeightTooltip);
  fixedDrag("sw",
            "smoothness weight",
            &opt.smoothnessWeight,
            &ZERO,
            &SMOOTH_MAX,
            "%.5f",
            0.0005F,
            kSmoothnessWeightTooltip);
  fixedDrag("cw",
            "constraint weight",
            &opt.constraintWeight,
            &ZERO,
            &CONS_MAX,
            "%.1f",
            5.0F,
            kConstraintWeightTooltip);
  fixedDrag("chord",
            "max invalid chord",
            &opt.maxInvalidChordFraction,
            &ZERO,
            &ONE,
            "%.2f",
            0.01F,
            kInvalidChordTooltip);
}

void
drawModeSection(ReverseDesignPanelState& state)
{
  auto& opt = state.optimizationSettings;

  ImGui::Checkbox("Использовать FEM для валидации", &opt.useFemValidation);
  if (!opt.useFemValidation) {
    ImGui::TextColored(style::PANEL_COLOR_WARN,
                       "⚠ Быстрый профиль пока не реализован — FEM всё равно используется.");
  }

  ImGui::Spacing();

  // Summarize which variables are currently active. CMA-ES only moves the
  // ones ticked in the "Границы поиска" editor.
  std::string activeList;
  int activeCount = 0;
  for (const auto& var : core::allDesignVariables()) {
    if (opt.mask.*(var.enabled)) {
      if (activeCount > 0) {
        activeList += ", ";
      }
      activeList += var.label;
      ++activeCount;
    }
  }

  if (activeCount == 0) {
    ImGui::TextColored(style::PANEL_COLOR_WARN,
                       "⚠ Ни одна переменная не выбрана — CMA-ES не сможет двигаться.");
  } else {
    ImGui::TextColored(style::PANEL_COLOR_INFO,
                       "Активные переменные (%d): %s",
                       activeCount,
                       activeList.c_str());
  }
}

void
drawDiagnostics(const ReverseDesignPanelState& state)
{
  if (!state.hasPreview) {
    ImGui::TextDisabled("Нет превью. Импортируйте геометрию и нажмите \"Обновить превью\".");
    return;
  }

  const ImGuiTableFlags tableFlags =
    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;

  if (ImGui::BeginTable("##objective", 2, tableFlags)) {
    labelValueRow("Objective total", "%.6f", state.currentObjective.total);
    labelValueRow("Area error", "%.6f", state.currentObjective.areaError);
    labelValueRow("Slope", "%.6f", state.currentObjective.residualSlopePenalty);
    labelValueRow("Monotonicity", "%.6f", state.currentObjective.monotonicityPenalty);
    labelValueRow("Smoothness", "%.6f", state.currentObjective.smoothnessPenalty);
    labelValueRow("Constraints", "%.6f", state.currentObjective.constraintPenalty);
    if (state.hasOptimizationResult) {
      labelValueRow(
        "Converged", "%s", state.lastOptimizationConverged ? "да" : "нет");
      labelValueRow("Generations", "%d", state.lastOptimizationGenerations);
    }
    ImGui::EndTable();
  }

  ImGui::Spacing();
  ImGui::TextDisabled("Текущие параметры (мм / °)");

  if (ImGui::BeginTable("##params", 4, tableFlags)) {
    const auto row = [](const char* l1, double v1, const char* l2, double v2) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("%s", l1);
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%.3f", v1);
      ImGui::TableSetColumnIndex(2);
      ImGui::TextDisabled("%s", l2);
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%.3f", v2);
    };
    row("b2", state.currentParams.b2, "din", state.currentParams.din);
    row("r1", state.currentParams.r1, "r2", state.currentParams.r2);
    row("r3", state.currentParams.r3, "r4", state.currentParams.r4);
    row("al1", state.currentParams.al1Deg, "al2", state.currentParams.al2Deg);
    row("al02", state.currentParams.al02Deg, "be1", state.currentParams.be1Deg);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("be3");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%.3f", state.currentParams.be3RawDeg);
    ImGui::EndTable();
  }
}

[[nodiscard]] ReverseDesignPanelResult
drawControlsWindow(ReverseDesignPanelState& state,
                   const core::PumpParams& meridionalParams,
                   const unsigned int dockspaceId)
{
  ReverseDesignPanelResult result;

  prepareDockedWindow(kControlsWindowTitle, dockspaceId);
  style::pushPanelStyle();
  ImGui::Begin(kControlsWindowTitle);

  const auto optimizerStatus = state.asyncOptimizer->status();
  const bool optimizing = optimizerStatus == core::SolverStatus::Running;

  // Persistent status strip at the top for visibility during long runs.
  drawStatusLine(state, optimizerStatus);
  ImGui::Separator();

  if (ImGui::CollapsingHeader("Источник геометрии", ImGuiTreeNodeFlags_DefaultOpen)) {
    result.paramsForMeridional = drawSourceSection(state, meridionalParams, optimizing);
  }

  if (ImGui::CollapsingHeader("Действия", ImGuiTreeNodeFlags_DefaultOpen)) {
    drawActionButtons(state, optimizing);
  }

  if (ImGui::CollapsingHeader("Настройки оптимизации")) {
    drawOptimizationSettings(state);
  }

  if (ImGui::CollapsingHeader("Режим поиска", ImGuiTreeNodeFlags_DefaultOpen)) {
    drawModeSection(state);
  }

  if (ImGui::CollapsingHeader("Границы поиска", ImGuiTreeNodeFlags_DefaultOpen)) {
    drawBoundsEditor(state, optimizing || !state.hasCurrentParams);
  }

  if (ImGui::CollapsingHeader("Диагностика")) {
    drawDiagnostics(state);
  }

  ImGui::End();
  style::popPanelStyle();

  return result;
}

void
drawCurveWindow(ReverseDesignPanelState& state, const unsigned int dockspaceId)
{
  prepareDockedWindow(kCurveWindowTitle, dockspaceId);

  ImGui::Begin(kCurveWindowTitle);
  ui::drawAreaCurveEditor(state.editorState);
  ImGui::End();
}

[[nodiscard]] double
findSeriesMin(std::span<const double> values) noexcept
{
  double minValue = std::numeric_limits<double>::max();
  for (double value : values) {
    if (std::isfinite(value)) {
      minValue = std::min(minValue, value);
    }
  }
  return minValue == std::numeric_limits<double>::max() ? 0.0 : minValue;
}

[[nodiscard]] double
findSeriesMax(std::span<const double> values) noexcept
{
  double maxValue = std::numeric_limits<double>::lowest();
  for (double value : values) {
    if (std::isfinite(value)) {
      maxValue = std::max(maxValue, value);
    }
  }
  return maxValue == std::numeric_limits<double>::lowest() ? 1.0 : maxValue;
}

void
drawComparisonWindow(ReverseDesignPanelState& state, const unsigned int dockspaceId)
{
  prepareDockedWindow(kComparisonWindowTitle, dockspaceId);

  ImGui::Begin(kComparisonWindowTitle);

  if (!state.hasPreview || state.xiSamples.empty() ||
      state.targetSamples.size() != state.currentSamples.size() ||
      state.currentSamples.size() != state.residualSamples.size()) {
    ImGui::TextDisabled("Сначала постройте превью или завершите оптимизацию.");
    ImGui::End();
    return;
  }

  const float availableHeight = ImGui::GetContentRegionAvail().y;
  const float compareHeight = std::max(180.0F, availableHeight * 0.55F);
  const bool useReference =
    state.comparisonReferenceOutletArea > 0.0 && std::isfinite(state.comparisonReferenceOutletArea);
  const char* yAxisLabel = useReference ? "F / F_ref" : "F_norm";
  const char* targetLabel = useReference ? "F_target_ref" : "F_target_norm";
  const char* currentLabel = useReference ? "F_current_ref" : "F_current_norm";

  if (ImPlot::BeginPlot(
        "##TargetVsCurrentArea", ImVec2(-1, compareHeight), ImPlotFlags_Crosshairs)) {
    const double yMax =
      std::max(findSeriesMax(state.targetSamples), findSeriesMax(state.currentSamples)) * 1.1;

    ImPlot::SetupAxes("xi", yAxisLabel, ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
    ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 1.0, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, std::max(yMax, 1.1), ImPlotCond_Always);
    ImPlot::SetupLegend(ImPlotLocation_NorthWest);
    ImPlot::SetupMouseText(ImPlotLocation_SouthEast);

    ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.85F, 0.30F, 0.18F, 1.0F));
    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.4F);
    ImPlot::PlotLine(targetLabel,
                     state.xiSamples.data(),
                     state.targetSamples.data(),
                     static_cast<int>(state.xiSamples.size()));
    ImPlot::PopStyleVar();
    ImPlot::PopStyleColor();

    ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.18F, 0.45F, 0.84F, 1.0F));
    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.4F);
    ImPlot::PlotLine(currentLabel,
                     state.xiSamples.data(),
                     state.currentSamples.data(),
                     static_cast<int>(state.xiSamples.size()));
    ImPlot::PopStyleVar();
    ImPlot::PopStyleColor();

    ImPlot::EndPlot();
  }

  if (ImPlot::BeginPlot("##AreaResidual", ImVec2(-1, -1), ImPlotFlags_Crosshairs)) {
    const double residualMin = findSeriesMin(state.residualSamples);
    const double residualMax = findSeriesMax(state.residualSamples);
    const double residualExtent = std::max(std::abs(residualMin), std::abs(residualMax));

    ImPlot::SetupAxes("xi", "Residual", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
    ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 1.0, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(
      ImAxis_Y1, -1.1 * residualExtent, 1.1 * residualExtent + 1e-6, ImPlotCond_Always);
    ImPlot::SetupMouseText(ImPlotLocation_SouthEast);

    double zero = 0.0;
    ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.65F, 0.68F, 0.72F, 0.9F));
    ImPlot::PlotInfLines("Нулевая ошибка", &zero, 1, ImPlotInfLinesFlags_Horizontal);
    ImPlot::PopStyleColor();

    ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.20F, 0.62F, 0.40F, 1.0F));
    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.2F);
    ImPlot::PlotLine("Residual",
                     state.xiSamples.data(),
                     state.residualSamples.data(),
                     static_cast<int>(state.xiSamples.size()));
    ImPlot::PopStyleVar();
    ImPlot::PopStyleColor();

    ImPlot::EndPlot();
  }

  ImGui::End();
}

} // namespace

ReverseDesignPanelResult
drawReverseDesignPanel(ReverseDesignPanelState& state,
                       const core::PumpParams& meridionalParams,
                       const unsigned int dockspaceId)
{
  setDefaultCurveIfNeeded(state.editorState);
  pollOptimization(state);

  auto result = drawControlsWindow(state, meridionalParams, dockspaceId);
  drawCurveWindow(state, dockspaceId);

  if (state.editorState.requestPreview) {
    state.editorState.requestPreview = false;
    if (state.hasCurrentParams) {
      previewCurrentGeometry(state);
    }
  }

  if (state.editorState.requestOptimization) {
    state.editorState.requestOptimization = false;
    submitOptimization(state);
  }

  drawGeometryPanelWithTitle(kGeometryWindowTitle,
                             state.previewFbo,
                             state.previewRenderer,
                             state.previewPanelState,
                             state.previewGeometry,
                             nullptr,
                             state.previewRenderSettings,
                             state.hasPreview,
                             dockspaceId);
  drawComparisonWindow(state, dockspaceId);

  return result;
}

} // namespace ggm::gui
