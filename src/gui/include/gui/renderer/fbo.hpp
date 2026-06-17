#pragma once

#include "core/error.hpp"
#include "gui/renderer/opengl/render_target_2d.hpp"

#include <utility>
#include <vector>

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

  [[nodiscard]] unsigned int textureId() const noexcept { return target_.textureId(); }
  [[nodiscard]] std::pair<int, int> size() const noexcept { return target_.size(); }
  [[nodiscard]] bool readRgbPixels(std::vector<unsigned char>& pixels) const noexcept;

private:
  RenderTarget2D target_{4};
};

} // namespace ggm::gui
