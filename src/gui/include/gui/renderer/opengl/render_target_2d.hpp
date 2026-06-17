#pragma once

#include "core/error.hpp"

#include <utility>
#include <vector>

namespace ggm::gui {

/// Offscreen 2D render target with MSAA color/depth-stencil buffers and a
/// resolved texture for ImGui display.
class RenderTarget2D
{
public:
  explicit RenderTarget2D(int samples = 4) noexcept;
  ~RenderTarget2D();
  RenderTarget2D(const RenderTarget2D&) = delete;
  RenderTarget2D& operator=(const RenderTarget2D&) = delete;
  RenderTarget2D(RenderTarget2D&& other) noexcept;
  RenderTarget2D& operator=(RenderTarget2D&& other) noexcept;

  [[nodiscard]] core::Result<void> resize(int width, int height) noexcept;
  void bind() const noexcept;
  void resolveAndUnbind() const noexcept;
  void destroy() noexcept;

  [[nodiscard]] bool readRgbPixels(std::vector<unsigned char>& pixels) const noexcept;

  [[nodiscard]] unsigned int textureId() const noexcept { return colorTex_; }
  [[nodiscard]] std::pair<int, int> size() const noexcept { return {width_, height_}; }

private:
  int samples_ = 4;
  unsigned int msFbo_ = 0;
  unsigned int msColorRbo_ = 0;
  unsigned int msDepthRbo_ = 0;
  unsigned int resolveFbo_ = 0;
  unsigned int colorTex_ = 0;
  int width_ = 0;
  int height_ = 0;
};

} // namespace ggm::gui
