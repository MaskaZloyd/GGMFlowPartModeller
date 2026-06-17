#pragma once

namespace ggm::gui {

/// Saves the OpenGL state touched by the 2D renderer and restores it at scope exit.
class ScopedGlState2D
{
public:
  ScopedGlState2D() noexcept;
  ~ScopedGlState2D();
  ScopedGlState2D(const ScopedGlState2D&) = delete;
  ScopedGlState2D& operator=(const ScopedGlState2D&) = delete;
  ScopedGlState2D(ScopedGlState2D&&) = delete;
  ScopedGlState2D& operator=(ScopedGlState2D&&) = delete;

  void beginFrame(int width, int height, float r, float g, float b, float a) const noexcept;

private:
  unsigned char blendEnabled_ = 0;
  unsigned char depthTestEnabled_ = 0;
  unsigned char scissorTestEnabled_ = 0;
  unsigned char multisampleEnabled_ = 0;
  int blendSrcRgb_ = 0;
  int blendDstRgb_ = 0;
  int blendSrcAlpha_ = 0;
  int blendDstAlpha_ = 0;
  int viewport_[4] = {0, 0, 0, 0};
};

} // namespace ggm::gui
