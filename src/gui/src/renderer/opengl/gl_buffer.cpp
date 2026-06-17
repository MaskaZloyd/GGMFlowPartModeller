#include "gui/renderer/opengl/gl_buffer.hpp"

#include "renderer/gl_headers.hpp"

#include <utility>

namespace ggm::gui {

GlBuffer::GlBuffer(unsigned int target) noexcept
{
  (void)create(target);
}

GlBuffer::~GlBuffer()
{
  destroy();
}

GlBuffer::GlBuffer(GlBuffer&& other) noexcept
  : id_(std::exchange(other.id_, 0U)),
    target_(std::exchange(other.target_, 0U))
{
}

GlBuffer&
GlBuffer::operator=(GlBuffer&& other) noexcept
{
  if (this != &other) {
    destroy();
    id_ = std::exchange(other.id_, 0U);
    target_ = std::exchange(other.target_, 0U);
  }
  return *this;
}

bool
GlBuffer::create(unsigned int target) noexcept
{
  destroy();
  target_ = target;
  glGenBuffers(1, &id_);
  if (id_ == 0U) {
    target_ = 0U;
    return false;
  }
  return true;
}

void
GlBuffer::destroy() noexcept
{
  if (id_ != 0U) {
    glDeleteBuffers(1, &id_);
    id_ = 0U;
  }
  target_ = 0U;
}

void
GlBuffer::bind() const noexcept
{
  glBindBuffer(target_, id_);
}

void
GlBuffer::unbind(unsigned int target) noexcept
{
  glBindBuffer(target, 0);
}

void
GlBuffer::uploadBytes(const void* data, std::size_t byteCount, unsigned int usage) const noexcept
{
  if (id_ == 0U || target_ == 0U) {
    return;
  }
  glBindBuffer(target_, id_);
  glBufferData(target_, static_cast<GLsizeiptr>(byteCount), data, usage);
}

} // namespace ggm::gui
