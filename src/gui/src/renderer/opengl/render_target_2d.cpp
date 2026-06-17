#include "gui/renderer/opengl/render_target_2d.hpp"

#include "renderer/gl_headers.hpp"

#include <algorithm>
#include <utility>

namespace ggm::gui {

RenderTarget2D::RenderTarget2D(int samples) noexcept : samples_(std::max(samples, 1)) {}

RenderTarget2D::~RenderTarget2D()
{
  destroy();
}

RenderTarget2D::RenderTarget2D(RenderTarget2D&& other) noexcept
  : samples_(std::exchange(other.samples_, 4)),
    msFbo_(std::exchange(other.msFbo_, 0U)),
    msColorRbo_(std::exchange(other.msColorRbo_, 0U)),
    msDepthRbo_(std::exchange(other.msDepthRbo_, 0U)),
    resolveFbo_(std::exchange(other.resolveFbo_, 0U)),
    colorTex_(std::exchange(other.colorTex_, 0U)),
    width_(std::exchange(other.width_, 0)),
    height_(std::exchange(other.height_, 0))
{
}

RenderTarget2D&
RenderTarget2D::operator=(RenderTarget2D&& other) noexcept
{
  if (this != &other) {
    destroy();
    samples_ = std::exchange(other.samples_, 4);
    msFbo_ = std::exchange(other.msFbo_, 0U);
    msColorRbo_ = std::exchange(other.msColorRbo_, 0U);
    msDepthRbo_ = std::exchange(other.msDepthRbo_, 0U);
    resolveFbo_ = std::exchange(other.resolveFbo_, 0U);
    colorTex_ = std::exchange(other.colorTex_, 0U);
    width_ = std::exchange(other.width_, 0);
    height_ = std::exchange(other.height_, 0);
  }
  return *this;
}

core::Result<void>
RenderTarget2D::resize(int width, int height) noexcept
{
  if (width == width_ && height == height_ && msFbo_ != 0U) {
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
  const int samples = std::clamp(samples_, 1, static_cast<int>(maxSamples));

  glGenFramebuffers(1, &msFbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);

  glGenRenderbuffers(1, &msColorRbo_);
  glBindRenderbuffer(GL_RENDERBUFFER, msColorRbo_);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, width_, height_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColorRbo_);

  glGenRenderbuffers(1, &msDepthRbo_);
  glBindRenderbuffer(GL_RENDERBUFFER, msDepthRbo_);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width_, height_);
  glFramebufferRenderbuffer(
    GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, msDepthRbo_);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    destroy();
    return std::unexpected(core::CoreError::RenderFailed);
  }

  glGenFramebuffers(1, &resolveFbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, resolveFbo_);

  glGenTextures(1, &colorTex_);
  glBindTexture(GL_TEXTURE_2D, colorTex_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex_, 0);

  const auto resolveStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  if (resolveStatus != GL_FRAMEBUFFER_COMPLETE) {
    destroy();
    return std::unexpected(core::CoreError::RenderFailed);
  }

  return {};
}

void
RenderTarget2D::bind() const noexcept
{
  glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
  glViewport(0, 0, width_, height_);
}

void
RenderTarget2D::resolveAndUnbind() const noexcept
{
  glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo_);
  glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool
RenderTarget2D::readRgbPixels(std::vector<unsigned char>& pixels) const noexcept
{
  if (width_ <= 0 || height_ <= 0 || colorTex_ == 0U) {
    return false;
  }

  const auto requiredSize =
    static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 3U;
  if (pixels.size() != requiredSize) {
    pixels.resize(requiredSize);
  }

  glBindTexture(GL_TEXTURE_2D, colorTex_);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
  glBindTexture(GL_TEXTURE_2D, 0);
  return true;
}

void
RenderTarget2D::destroy() noexcept
{
  if (colorTex_ != 0U) {
    glDeleteTextures(1, &colorTex_);
    colorTex_ = 0U;
  }
  if (resolveFbo_ != 0U) {
    glDeleteFramebuffers(1, &resolveFbo_);
    resolveFbo_ = 0U;
  }
  if (msColorRbo_ != 0U) {
    glDeleteRenderbuffers(1, &msColorRbo_);
    msColorRbo_ = 0U;
  }
  if (msDepthRbo_ != 0U) {
    glDeleteRenderbuffers(1, &msDepthRbo_);
    msDepthRbo_ = 0U;
  }
  if (msFbo_ != 0U) {
    glDeleteFramebuffers(1, &msFbo_);
    msFbo_ = 0U;
  }
  width_ = 0;
  height_ = 0;
}

} // namespace ggm::gui
