#pragma once

#include <expected>
#include <string>
#include <utility>

struct GLFWwindow;

namespace ggm::gui {

struct WindowConfig
{
  int width = 1600;
  int height = 900;
  const char* title = "GGM Flow Part Modeller";
};

/// RAII wrapper for GLFW window + OpenGL context + ImGui initialization.
/// Destruction reverses init order: ImGui → GL context → GLFW.
class AppWindow
{
public:
  [[nodiscard]] static std::expected<AppWindow, std::string> create(WindowConfig cfg = {}) noexcept;

  ~AppWindow();
  AppWindow(const AppWindow&) = delete;
  AppWindow& operator=(const AppWindow&) = delete;
  AppWindow(AppWindow&& other) noexcept;
  AppWindow& operator=(AppWindow&& other) noexcept;

  [[nodiscard]] bool shouldClose() const noexcept;
  void beginFrame() noexcept;
  void endFrame() noexcept;
  [[nodiscard]] std::pair<int, int> framebufferSize() const noexcept;
  [[nodiscard]] GLFWwindow* handle() noexcept { return window_; }

private:
  AppWindow() = default;
  void destroy() noexcept;

  GLFWwindow* window_ = nullptr;
  bool imguiInitialized_ = false;
};

}
