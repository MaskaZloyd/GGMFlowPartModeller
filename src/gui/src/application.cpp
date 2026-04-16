#include "gui/application.hpp"

#include "gui/layout/dockspace.hpp"
#include "gui/panels/charts_panel.hpp"
#include "gui/panels/geometry_panel.hpp"
#include "gui/panels/log_panel.hpp"
#include "gui/panels/params_panel.hpp"
#include "gui/panels/settings_panel.hpp"

#include "core/logging.hpp"
#include "core/serialization.hpp"

#include <GLFW/glfw3.h>

#include <memory>

namespace ggm::gui {

Application::Application(AppWindow window)
    : window_(std::move(window))
    , asyncSolver_(std::make_unique<core::AsyncFlowSolver>()) {}

std::expected<Application, std::string> Application::create() noexcept {
  auto window = AppWindow::create();
  if (!window) {
    return std::unexpected(window.error());
  }

  Application app(std::move(*window));

  if (auto rebuilt = app.model_.rebuildGeometry(); !rebuilt) {
    logging::gui()->warn("Ошибка построения геометрии: {}",
                         core::toString(rebuilt.error()));
  }
  logging::gui()->info("Приложение запущено");

  if (app.model_.geometryValid()) {
    logging::gui()->info("Построена начальная геометрия");
    app.asyncSolver_->submit(app.model_.geometry(), app.model_.params(),
                             app.model_.compSettings());
  } else {
    logging::gui()->warn("Не удалось построить начальную геометрию");
  }

  return app;
}

int Application::run() noexcept {
  while (!window_.shouldClose()) {
    window_.beginFrame();

    if (asyncSolver_->poll()) {
      logging::gui()->info("Расчёт завершён за {} мс",
                           asyncSolver_->lastDuration().count());
    }

    auto actions = buildDockspace(undoStack_.canUndo(), undoStack_.canRedo());
    drawStatusBar(currentPath_.filename().string(), asyncSolver_->status(),
                  asyncSolver_->lastDuration());

    if (actions.requestNew) {
      handleNew();
    }
    if (actions.requestOpen) {
      handleOpen();
    }
    if (actions.requestSave) {
      handleSave();
    }
    if (actions.requestSaveAs) {
      handleSaveAs();
    }
    if (actions.requestUndo) {
      handleUndo();
    }
    if (actions.requestRedo) {
      handleRedo();
    }
    if (actions.requestQuit) {
      break;
    }

    // Parameters panel: fast geometry-only update during drag.
    auto panelResult = drawParamsPanel(model_.params());

    bool liveChanged = !(panelResult.liveParams == model_.params());
    if (liveChanged) {
      model_.setParams(panelResult.liveParams);
      if (auto rebuilt = model_.rebuildGeometry(); !rebuilt) {
        logging::gui()->warn("Ошибка построения геометрии: {}",
                             core::toString(rebuilt.error()));
      }
    }

    if (panelResult.finishedEdit) {
      undoStack_.push(std::move(*panelResult.finishedEdit));
      if (!model_.geometryValid()) {
        logging::gui()->warn("Геометрия не построена с текущими параметрами");
      } else {
        // Auto-resubmit so status/time reflects the current parameters.
        asyncSolver_->submit(model_.geometry(), model_.params(), model_.compSettings());
      }
    }

    // Take a single snapshot for this frame — keeps result alive even if
    // the worker publishes a new one mid-frame.
    auto flowSnapshot = asyncSolver_->snapshot();
    const core::FlowResults* flowPtr = flowSnapshot.get();
    bool flowValid = flowSnapshot != nullptr;

    drawGeometryPanel(geometryFbo_, geometryRenderer_, model_.geometry(),
                      flowPtr, renderSettings_, model_.geometryValid());
    drawChartsPanel(flowPtr, flowValid);

    auto settingsResult = drawSettingsPanel(model_.compSettings(), renderSettings_,
                                            asyncSolver_->status(),
                                            asyncSolver_->lastDuration());
    if (settingsResult.renderSettingsChanged) {
      renderSettings_ = settingsResult.renderSettings;
    }
    if (settingsResult.compSettingsChanged) {
      model_.setCompSettings(settingsResult.compSettings);
    }
    if (settingsResult.recomputeRequested) {
      if (model_.geometryValid()) {
        asyncSolver_->submit(model_.geometry(), model_.params(), model_.compSettings());
        logging::gui()->info("Запущен расчёт потока");
      }
    }
    if (settingsResult.cancelRequested) {
      asyncSolver_->cancelAndWait();
      logging::gui()->info("Расчёт отменён");
    }

    drawLogPanel();

    window_.endFrame();
  }

  return 0;
}

void Application::handleUndo() noexcept {
  if (!undoStack_.canUndo()) {
    return;
  }
  const auto& params = undoStack_.undoParams();
  model_.setParams(params);
  if (auto rebuilt = model_.rebuildGeometry(); !rebuilt) {
    logging::gui()->warn("Ошибка построения геометрии: {}",
                         core::toString(rebuilt.error()));
  }
  undoStack_.undo();
  logging::gui()->info("Undo");
}

void Application::handleRedo() noexcept {
  if (!undoStack_.canRedo()) {
    return;
  }
  const auto& params = undoStack_.redoParams();
  model_.setParams(params);
  if (auto rebuilt = model_.rebuildGeometry(); !rebuilt) {
    logging::gui()->warn("Ошибка построения геометрии: {}",
                         core::toString(rebuilt.error()));
  }
  undoStack_.redo();
  logging::gui()->info("Redo");
}

void Application::handleSave() noexcept {
  if (currentPath_.empty()) {
    handleSaveAs();
    return;
  }
  auto result = core::saveParams(model_.params(), currentPath_);
  if (result) {
    logging::gui()->info("Сохранено в {}", currentPath_.string());
  } else {
    logging::gui()->error("Ошибка сохранения: {}", core::toString(result.error()));
  }
}

void Application::handleSaveAs() noexcept {
  currentPath_ = "pump_project.ggm";
  handleSave();
}

void Application::handleOpen() noexcept {
  std::filesystem::path path = "pump_project.ggm";
  auto result = core::loadParams(path);
  if (result) {
    auto oldParams = model_.params();
    model_.setParams(*result);
    if (auto rebuilt = model_.rebuildGeometry(); !rebuilt) {
      logging::gui()->warn("Ошибка построения геометрии: {}",
                           core::toString(rebuilt.error()));
    }
    undoStack_.push(EditCommand{
        .before = oldParams,
        .after = *result,
        .label = "Open file",
    });
    currentPath_ = path;
    logging::gui()->info("Загружено из {}", path.string());
  } else {
    logging::gui()->error("Ошибка открытия: {}", core::toString(result.error()));
  }
}

void Application::handleNew() noexcept {
  auto oldParams = model_.params();
  core::PumpParams defaultParams;
  model_.setParams(defaultParams);
  if (auto rebuilt = model_.rebuildGeometry(); !rebuilt) {
    logging::gui()->warn("Ошибка построения геометрии: {}",
                         core::toString(rebuilt.error()));
  }
  undoStack_.push(EditCommand{
      .before = oldParams,
      .after = defaultParams,
      .label = "New project",
  });
  currentPath_.clear();
  logging::gui()->info("Новый проект");
}

} // namespace ggm::gui
