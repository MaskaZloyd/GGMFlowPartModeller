#include "gui/renderer/opengl/gl_vertex_array.hpp"

#include "renderer/gl_headers.hpp"

#include <utility>

namespace ggm::gui {

GlVertexArray::~GlVertexArray()
{
  destroy();
}

GlVertexArray::GlVertexArray(GlVertexArray&& other) noexcept : id_(std::exchange(other.id_, 0U)) {}

GlVertexArray&
GlVertexArray::operator=(GlVertexArray&& other) noexcept
{
  if (this != &other) {
    destroy();
    id_ = std::exchange(other.id_, 0U);
  }
  return *this;
}

bool
GlVertexArray::create() noexcept
{
  destroy();
  glGenVertexArrays(1, &id_);
  return id_ != 0U;
}

void
GlVertexArray::destroy() noexcept
{
  if (id_ != 0U) {
    glDeleteVertexArrays(1, &id_);
    id_ = 0U;
  }
}

void
GlVertexArray::bind() const noexcept
{
  glBindVertexArray(id_);
}

void
GlVertexArray::unbind() noexcept
{
  glBindVertexArray(0);
}

void
GlVertexArray::enableAttribute(unsigned int index,
                               int componentCount,
                               unsigned int componentType,
                               bool normalized,
                               int strideBytes,
                               std::size_t offsetBytes) const noexcept
{
  glVertexAttribPointer(index,
                        componentCount,
                        componentType,
                        normalized ? GL_TRUE : GL_FALSE,
                        strideBytes,
                        reinterpret_cast<const void*>(offsetBytes));
  glEnableVertexAttribArray(index);
}

void
GlVertexArray::disableAttribute(unsigned int index) const noexcept
{
  glDisableVertexAttribArray(index);
}

} // namespace ggm::gui
