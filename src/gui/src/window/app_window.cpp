#include "gui/window/app_window.hpp"

#include "gui/theme.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

namespace ggm::gui {

std::expected<AppWindow, std::string>
AppWindow::create(WindowConfig cfg) noexcept
{
  if (glfwInit() == GLFW_FALSE) {
    return std::unexpected(std::string("Failed to initialize GLFW"));
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_SAMPLES, 4);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

  GLFWwindow* win = glfwCreateWindow(cfg.width, cfg.height, cfg.title, nullptr, nullptr);
  if (win == nullptr) {
    glfwTerminate();
    return std::unexpected(std::string("Failed to create GLFW window"));
  }

  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();

  ImGuiIO& imguiIo = ImGui::GetIO();
  imguiIo.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  imguiIo.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // Load a font with Cyrillic glyphs. Try common system paths.
  constexpr const char* FONT_CANDIDATES[] = {
    // NOLINT
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/Library/Fonts/DejaVuSans.ttf",
    "C:/Windows/Fonts/DejaVuSans.ttf",
  };
  const ImWchar* cyrillicRanges = imguiIo.Fonts->GetGlyphRangesCyrillic();
  bool fontLoaded = false;
  for (const char* path : FONT_CANDIDATES) {
    if (imguiIo.Fonts->AddFontFromFileTTF(path, 17.0F, nullptr, cyrillicRanges) != nullptr) {
      fontLoaded = true;
      break;
    }
  }
  if (!fontLoaded) {
    imguiIo.Fonts->AddFontDefault();
  }

  applyLightTheme();

  ImGui_ImplGlfw_InitForOpenGL(win, true);
  ImGui_ImplOpenGL3_Init("#version 330 core");

  AppWindow result;
  result.window_ = win;
  result.imguiInitialized_ = true;
  return result;
}

AppWindow::~AppWindow()
{
  destroy();
}

AppWindow::AppWindow(AppWindow&& other) noexcept
  : window_(other.window_),
    imguiInitialized_(other.imguiInitialized_)
{
  other.window_ = nullptr;
  other.imguiInitialized_ = false;
}

AppWindow&
AppWindow::operator=(AppWindow&& other) noexcept
{
  if (this != &other) {
    destroy();
    window_ = other.window_;
    imguiInitialized_ = other.imguiInitialized_;
    other.window_ = nullptr;
    other.imguiInitialized_ = false;
  }
  return *this;
}

bool
AppWindow::shouldClose() const noexcept
{
  if (window_ == nullptr) {
    return true;
  }
  return glfwWindowShouldClose(window_) != 0;
}

void
AppWindow::beginFrame() noexcept
{
  glfwPollEvents();
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void
AppWindow::endFrame() noexcept
{
  ImGui::Render();
  auto [width, height] = framebufferSize();
  glViewport(0, 0, width, height);
  glClearColor(0.975F, 0.977F, 0.980F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glfwSwapBuffers(window_);
}

std::pair<int, int>
AppWindow::framebufferSize() const noexcept
{
  int width = 0;
  int height = 0;
  if (window_ != nullptr) {
    glfwGetFramebufferSize(window_, &width, &height);
  }
  return {width, height};
}

void
AppWindow::destroy() noexcept
{
  if (imguiInitialized_) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    imguiInitialized_ = false;
  }
  if (window_ != nullptr) {
    glfwDestroyWindow(window_);
    glfwTerminate();
    window_ = nullptr;
  }
}

} // namespace ggm::gui
