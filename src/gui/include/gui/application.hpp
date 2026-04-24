#pragma once

#include "core/async_flow_solver.hpp"
#include "core/pump_model.hpp"
#include "gui/commands/edit_command.hpp"
#include "gui/commands/undo_stack.hpp"
#include "gui/panels/geometry_panel.hpp"
#include "gui/panels/params_panel.hpp"
#include "gui/panels/reverse_design_panel.hpp"
#include "gui/renderer/fbo.hpp"
#include "gui/renderer/geometry_renderer.hpp"
#include "gui/renderer/render_settings.hpp"
#include "gui/window/app_window.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace ggm::gui {

class Application
{
public:
  [[nodiscard]] static std::expected<Application, std::string> create() noexcept;

  // Main loop. Returns exit code (0 on success).
  int run() noexcept;

  ~Application() = default;
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) noexcept = default;
  Application& operator=(Application&&) noexcept = default;

private:
  explicit Application(AppWindow window);

  void handleUndo() noexcept;
  void handleRedo() noexcept;
  void handleSave() noexcept;
  void handleSaveAs() noexcept;
  void handleOpen() noexcept;
  void handleNew() noexcept;

  AppWindow window_;
  core::PumpModel model_;
  UndoStack undoStack_;
  Fbo geometryFbo_;
  GeometryRenderer geometryRenderer_;
  GeometryPanelState geometryPanelState_;
  std::filesystem::path currentPath_;
  RenderSettings renderSettings_;
  std::unique_ptr<core::AsyncFlowSolver> asyncSolver_;
  ParamsPanelState paramsPanelState_;
  ReverseDesignPanelState reverseDesignPanelState_;
};

} // namespace ggm::gui
