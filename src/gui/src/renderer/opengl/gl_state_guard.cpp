#include "gui/renderer/opengl/gl_state_guard.hpp"

#include "renderer/gl_headers.hpp"

namespace ggm::gui {

ScopedGlState2D::ScopedGlState2D() noexcept
{
  glGetBooleanv(GL_BLEND, &blendEnabled_);
  glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled_);
  glGetBooleanv(GL_SCISSOR_TEST, &scissorTestEnabled_);
  glGetBooleanv(GL_MULTISAMPLE, &multisampleEnabled_);
  glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb_);
  glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb_);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha_);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha_);
  glGetIntegerv(GL_VIEWPORT, viewport_);
}

ScopedGlState2D::~ScopedGlState2D()
{
  if (blendEnabled_ == GL_TRUE) {
    glEnable(GL_BLEND);
  } else {
    glDisable(GL_BLEND);
  }
  if (depthTestEnabled_ == GL_TRUE) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
  if (scissorTestEnabled_ == GL_TRUE) {
    glEnable(GL_SCISSOR_TEST);
  } else {
    glDisable(GL_SCISSOR_TEST);
  }
  if (multisampleEnabled_ == GL_TRUE) {
    glEnable(GL_MULTISAMPLE);
  } else {
    glDisable(GL_MULTISAMPLE);
  }
  glBlendFuncSeparate(blendSrcRgb_, blendDstRgb_, blendSrcAlpha_, blendDstAlpha_);
  glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
}

void
ScopedGlState2D::beginFrame(int width, int height, float r, float g, float b, float a)
  const noexcept
{
  glViewport(0, 0, width, height);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_SCISSOR_TEST);
  glEnable(GL_MULTISAMPLE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(r, g, b, a);
  glClear(GL_COLOR_BUFFER_BIT);
}

} // namespace ggm::gui
