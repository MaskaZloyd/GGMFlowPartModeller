#pragma once

#include <cstddef>

namespace ggm::gui {

class GlVertexArray
{
public:
  GlVertexArray() = default;
  ~GlVertexArray();
  GlVertexArray(const GlVertexArray&) = delete;
  GlVertexArray& operator=(const GlVertexArray&) = delete;
  GlVertexArray(GlVertexArray&& other) noexcept;
  GlVertexArray& operator=(GlVertexArray&& other) noexcept;

  [[nodiscard]] bool create() noexcept;
  void destroy() noexcept;

  void bind() const noexcept;
  static void unbind() noexcept;

  void enableAttribute(unsigned int index,
                       int componentCount,
                       unsigned int componentType,
                       bool normalized,
                       int strideBytes,
                       std::size_t offsetBytes) const noexcept;
  void disableAttribute(unsigned int index) const noexcept;

  [[nodiscard]] bool isValid() const noexcept { return id_ != 0U; }
  [[nodiscard]] unsigned int id() const noexcept { return id_; }

private:
  unsigned int id_ = 0;
};

} // namespace ggm::gui
