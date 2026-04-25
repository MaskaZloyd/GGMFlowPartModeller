#pragma once

#include "core/error.hpp"

#include <utility>

namespace ggm::gui {

/// RAII wrapper for an OpenGL framebuffer object with color texture + depth renderbuffer.
class Fbo
{
public:
  Fbo() = default;
  ~Fbo();
  Fbo(const Fbo&) = delete;
  Fbo& operator=(const Fbo&) = delete;
  Fbo(Fbo&& other) noexcept;
  Fbo& operator=(Fbo&& other) noexcept;

  /// (Re)allocate if size changed. No-op if size matches.
  [[nodiscard]] core::Result<void> resize(int width, int height) noexcept;

  /// Bind the multisampled FBO for rendering.
  void bind() noexcept;
  /// Resolve MSAA -> colorTex_ and unbind.
  void unbind() noexcept;

  [[nodiscard]] unsigned int textureId() const noexcept { return colorTex_; }
  [[nodiscard]] std::pair<int, int> size() const noexcept { return {width_, height_}; }

private:
  void destroy() noexcept;

  /// Multisampled target (render here):
  unsigned int msFbo_ = 0;
  unsigned int msColorRbo_ = 0;
  unsigned int msDepthRbo_ = 0;

  /// Resolve target (blitted into and displayed by ImGui):
  unsigned int fbo_ = 0;
  unsigned int colorTex_ = 0;

  int width_ = 0;
  int height_ = 0;
  int samples_ = 4;
};

}
