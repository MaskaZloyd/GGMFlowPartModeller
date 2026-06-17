#include "gui/renderer/fbo.hpp"

#include <utility>

namespace ggm::gui {

Fbo::~Fbo() = default;

Fbo::Fbo(Fbo&& other) noexcept = default;

Fbo&
Fbo::operator=(Fbo&& other) noexcept = default;

core::Result<void>
Fbo::resize(int width, int height) noexcept
{
  return target_.resize(width, height);
}

void
Fbo::bind() noexcept
{
  target_.bind();
}

void
Fbo::unbind() noexcept
{
  target_.resolveAndUnbind();
}

bool
Fbo::readRgbPixels(std::vector<unsigned char>& pixels) const noexcept
{
  return target_.readRgbPixels(pixels);
}

} // namespace ggm::gui
