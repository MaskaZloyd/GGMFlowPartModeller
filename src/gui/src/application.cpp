#include "gui/application.hpp"

#include "core/logging.hpp"
#include "core/serialization.hpp"
#include "gui/layout/dockspace.hpp"
#include "gui/panels/charts_panel.hpp"
#include "gui/panels/blade_design_panel.hpp"
#include "gui/panels/geometry_panel.hpp"
#include "gui/panels/log_panel.hpp"
#include "gui/panels/params_panel.hpp"
#include "gui/panels/reverse_design_panel.hpp"
#include "gui/panels/settings_panel.hpp"

#include <memory>

#include <GLFW/glfw3.h>

namespace ggm::gui {

Application::Application(AppWindow window)
  : window_(std::move(window)),
    asyncSolver_(std::make_unique<core::AsyncFlowSolver>())
{
}

std::expected<Application, std::string>
Application::create() noexcept
{
  auto window = AppWindow::create();
  if (!window) {
    return std::unexpected(window.error());
  }

  Application app(std::move(*window));

  if (auto rebuilt = app.model_.rebuildGeometry(); !rebuilt) {
    logging::gui()->warn("Ошибка построения геометрии: {}", core::toString(rebuilt.error()));
  }
  logging::gui()->info("Приложение запущено");

  if (app.model_.geometryValid()) {
    logging::gui()->info("Построена начальная геометрия");
    app.asyncSolver_->submit(app.model_.geometry(), app.model_.params(), app.model_.compSettings());
  } else {
    logging::gui()->warn("Не удалось построить начальную геометрию");
  }

  return app;
}

int
Application::run() noexcept
{
  while (!window_.shouldClose()) {
    window_.beginFrame();

    if (asyncSolver_->poll()) {
      logging::gui()->info("Расчёт завершён за {} мс", asyncSolver_->lastDuration().count());
    }

    auto dockLayout = buildDockspace(undoStack_.canUndo(), undoStack_.canRedo());
    drawStatusBar(
      currentPath_.filename().string(), asyncSolver_->status(), asyncSolver_->lastDuration());

    if (dockLayout.actions.requestNew) {
      handleNew();
    }
    if (dockLayout.actions.requestOpen) {
      handleOpen();
    }
    if (dockLayout.actions.requestSave) {
      handleSave();
    }
    if (dockLayout.actions.requestSaveAs) {
      handleSaveAs();
    }
    if (dockLayout.actions.requestUndo) {
      handleUndo();
    }
    if (dockLayout.actions.requestRedo) {
      handleRedo();
    }
    if (dockLayout.actions.requestQuit) {
      break;
    }

    auto flowSnapshot = asyncSolver_->snapshot();
    const core::FlowResults* flowPtr = flowSnapshot.get();
    bool flowValid = flowSnapshot != nullptr;

    if (dockLayout.meridionalDockspaceId != 0) {
      auto panelResult =
        drawParamsPanel(model_.params(), paramsPanelState_, dockLayout.meridionalDockspaceId);

      bool liveChanged = !(panelResult.liveParams == model_.params());
      if (liveChanged) {
        model_.setParams(panelResult.liveParams);
        if (auto rebuilt = model_.rebuildGeometry(); !rebuilt) {
          logging::gui()->warn("Ошибка построения геометрии: {}", core::toString(rebuilt.error()));
        }
      }

      if (panelResult.finishedEdit) {
        undoStack_.push(std::move(*panelResult.finishedEdit));
        if (!model_.geometryValid()) {
          logging::gui()->warn("Геометрия не построена с текущими параметрами");
        } else {
          asyncSolver_->submit(model_.geometry(), model_.params(), model_.compSettings());
        }
      }

      drawGeometryPanel(geometryFbo_,
                        geometryRenderer_,
                        geometryPanelState_,
                        model_.geometry(),
                        flowPtr,
                        renderSettings_,
                        model_.geometryValid(),
                        dockLayout.meridionalDockspaceId);
      drawChartsPanel(flowPtr, flowValid, dockLayout.meridionalDockspaceId);

      auto settingsResult = drawSettingsPanel(model_.compSettings(),
                                              renderSettings_,
                                              asyncSolver_->status(),
                                              asyncSolver_->lastDuration(),
                                              dockLayout.meridionalDockspaceId);
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
    }

    if (dockLayout.reverseDesignDockspaceId != 0) {
      auto reverseResult = drawReverseDesignPanel(
        reverseDesignPanelState_, model_.params(), dockLayout.reverseDesignDockspaceId);
      if (reverseResult.paramsForMeridional) {
        applyReverseDesignParams(*reverseResult.paramsForMeridional);
      }
    }

    if (dockLayout.bladeDesignDockspaceId != 0) {
      auto bladeResult = drawBladeDesignPanel(bladeDesignPanelState_,
                                              bladePlanFbo_,
                                              bladePlanRenderer_,
                                              model_.bladeDesignParams(),
                                              model_.params(),
                                              model_.geometry(),
                                              flowPtr,
                                              model_.geometryValid(),
                                              dockLayout.bladeDesignDockspaceId);
      if (bladeResult.paramsChanged) {
        model_.setBladeDesignParams(bladeResult.params);
        model_.setFlowRateM3s(bladeResult.params.flowRateM3s);
        if (model_.geometryValid()) {
          asyncSolver_->submit(model_.geometry(), model_.params(), model_.compSettings());
        }
      }
    }

    drawLogPanel(dockLayout.rootDockspaceId);

    window_.endFrame();
  }

  return 0;
}

void
Application::handleUndo() noexcept
{
  if (!undoStack_.canUndo()) {
    return;
  }
  const auto& params = undoStack_.undoParams();
  model_.setParams(params);
  if (auto rebuilt = model_.rebuildGeometry(); !rebuilt) {
    logging::gui()->warn("Ошибка построения геометрии: {}", core::toString(rebuilt.error()));
  }
  undoStack_.undo();
  logging::gui()->info("Undo");
}

void
Application::handleRedo() noexcept
{
  if (!undoStack_.canRedo()) {
    return;
  }
  const auto& params = undoStack_.redoParams();
  model_.setParams(params);
  if (auto rebuilt = model_.rebuildGeometry(); !rebuilt) {
    logging::gui()->warn("Ошибка построения геометрии: {}", core::toString(rebuilt.error()));
  }
  undoStack_.redo();
  logging::gui()->info("Redo");
}

void
Application::handleSave() noexcept
{
  if (currentPath_.empty()) {
    handleSaveAs();
    return;
  }
  core::ProjectData project{
    .pumpParams = model_.params(),
    .bladeDesign = model_.bladeDesignParams(),
  };
  project.pumpParams.qM3s = project.bladeDesign.flowRateM3s;
  auto result = core::saveProject(project, currentPath_);
  if (result) {
    logging::gui()->info("Сохранено в {}", currentPath_.string());
  } else {
    logging::gui()->error("Ошибка сохранения: {}", core::toString(result.error()));
  }
}

void
Application::handleSaveAs() noexcept
{
  currentPath_ = "pump_project.ggm";
  handleSave();
}

void
Application::handleOpen() noexcept
{
  std::filesystem::path path = "pump_project.ggm";
  auto result = core::loadProject(path);
  if (result) {
    auto oldParams = model_.params();
    model_.setBladeDesignParams(result->bladeDesign);
    result->pumpParams.qM3s = result->bladeDesign.flowRateM3s;
    model_.setParams(result->pumpParams);
    if (auto rebuilt = model_.rebuildGeometry(); !rebuilt) {
      logging::gui()->warn("Ошибка построения геометрии: {}", core::toString(rebuilt.error()));
    }
    undoStack_.push(EditCommand{
      .before = oldParams,
      .after = result->pumpParams,
      .label = "Open file",
    });
    currentPath_ = path;
    logging::gui()->info("Загружено из {}", path.string());
  } else {
    logging::gui()->error("Ошибка открытия: {}", core::toString(result.error()));
  }
}

void
Application::handleNew() noexcept
{
  auto oldParams = model_.params();
  core::PumpParams defaultParams;
  model_.setParams(defaultParams);
  model_.setBladeDesignParams(core::BladeDesignParams{});
  if (auto rebuilt = model_.rebuildGeometry(); !rebuilt) {
    logging::gui()->warn("Ошибка построения геометрии: {}", core::toString(rebuilt.error()));
  }
  undoStack_.push(EditCommand{
    .before = oldParams,
    .after = defaultParams,
    .label = "New project",
  });
  currentPath_.clear();
  logging::gui()->info("Новый проект");
}

void
Application::applyReverseDesignParams(const core::PumpParams& params) noexcept
{
  if (params == model_.params()) {
    logging::gui()->info("Геометрия обратного проектирования уже совпадает с меридианной");
    return;
  }

  const auto oldParams = model_.params();
  model_.setParams(params);
  if (auto rebuilt = model_.rebuildGeometry(); !rebuilt) {
    logging::gui()->warn("Ошибка построения геометрии: {}", core::toString(rebuilt.error()));
  }

  undoStack_.push(EditCommand{
    .before = oldParams,
    .after = params,
    .label = "Импорт из обратного проектирования",
  });

  if (model_.geometryValid()) {
    asyncSolver_->submit(model_.geometry(), model_.params(), model_.compSettings());
  }
  logging::gui()->info("Геометрия импортирована из обратного проектирования");
}

}
