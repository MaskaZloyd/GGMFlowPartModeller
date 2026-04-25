#include "gui/renderer/fbo.hpp"

#include "gl_headers.hpp"

namespace ggm::gui {

Fbo::~Fbo()
{
  destroy();
}

Fbo::Fbo(Fbo&& other) noexcept
  : msFbo_(std::exchange(other.msFbo_, 0)),
    msColorRbo_(std::exchange(other.msColorRbo_, 0)),
    msDepthRbo_(std::exchange(other.msDepthRbo_, 0)),
    fbo_(std::exchange(other.fbo_, 0)),
    colorTex_(std::exchange(other.colorTex_, 0)),
    width_(std::exchange(other.width_, 0)),
    height_(std::exchange(other.height_, 0)),
    samples_(std::exchange(other.samples_, 0))
{
}

Fbo&
Fbo::operator=(Fbo&& other) noexcept
{
  if (this != &other) {
    destroy();
    msFbo_ = std::exchange(other.msFbo_, 0);
    msColorRbo_ = std::exchange(other.msColorRbo_, 0);
    msDepthRbo_ = std::exchange(other.msDepthRbo_, 0);
    fbo_ = std::exchange(other.fbo_, 0);
    colorTex_ = std::exchange(other.colorTex_, 0);
    width_ = std::exchange(other.width_, 0);
    height_ = std::exchange(other.height_, 0);
    samples_ = std::exchange(other.samples_, 0);
  }
  return *this;
}

core::Result<void>
Fbo::resize(int width, int height) noexcept
{
  if (width == width_ && height == height_ && msFbo_ != 0) {
    return {};
  }
  if (width <= 0 || height <= 0) {
    return {};
  }

  destroy();

  width_ = width;
  height_ = height;

  GLint maxSamples = 1;
  glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
  int samples = samples_;
  if (samples > maxSamples) {
    samples = maxSamples;
  }
  if (samples < 1) {
    samples = 1;
  }

  glGenFramebuffers(1, &msFbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);

  glGenRenderbuffers(1, &msColorRbo_);
  glBindRenderbuffer(GL_RENDERBUFFER, msColorRbo_);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColorRbo_);

  glGenRenderbuffers(1, &msDepthRbo_);
  glBindRenderbuffer(GL_RENDERBUFFER, msDepthRbo_);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height);
  glFramebufferRenderbuffer(
    GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, msDepthRbo_);

  auto msStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (msStatus != GL_FRAMEBUFFER_COMPLETE) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    destroy();
    return std::unexpected(core::CoreError::RenderFailed);
  }

  glGenFramebuffers(1, &fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

  glGenTextures(1, &colorTex_);
  glBindTexture(GL_TEXTURE_2D, colorTex_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex_, 0);

  auto resolveStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  if (resolveStatus != GL_FRAMEBUFFER_COMPLETE) {
    destroy();
    return std::unexpected(core::CoreError::RenderFailed);
  }

  return {};
}

void
Fbo::bind() noexcept
{
  glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
  glViewport(0, 0, width_, height_);
}

void
Fbo::unbind() noexcept
{

  glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_);
  glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void
Fbo::destroy() noexcept
{
  if (colorTex_ != 0) {
    glDeleteTextures(1, &colorTex_);
    colorTex_ = 0;
  }
  if (fbo_ != 0) {
    glDeleteFramebuffers(1, &fbo_);
    fbo_ = 0;
  }
  if (msColorRbo_ != 0) {
    glDeleteRenderbuffers(1, &msColorRbo_);
    msColorRbo_ = 0;
  }
  if (msDepthRbo_ != 0) {
    glDeleteRenderbuffers(1, &msDepthRbo_);
    msDepthRbo_ = 0;
  }
  if (msFbo_ != 0) {
    glDeleteFramebuffers(1, &msFbo_);
    msFbo_ = 0;
  }
  width_ = 0;
  height_ = 0;
}

}
