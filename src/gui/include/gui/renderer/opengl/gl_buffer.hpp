#pragma once

#include <cstddef>
#include <span>

namespace ggm::gui {

class GlBuffer
{
public:
  GlBuffer() = default;
  explicit GlBuffer(unsigned int target) noexcept;
  ~GlBuffer();
  GlBuffer(const GlBuffer&) = delete;
  GlBuffer& operator=(const GlBuffer&) = delete;
  GlBuffer(GlBuffer&& other) noexcept;
  GlBuffer& operator=(GlBuffer&& other) noexcept;

  [[nodiscard]] bool create(unsigned int target) noexcept;
  void destroy() noexcept;

  void bind() const noexcept;
  static void unbind(unsigned int target) noexcept;

  void uploadBytes(const void* data, std::size_t byteCount, unsigned int usage) const noexcept;

  template <typename T>
  void upload(std::span<const T> values, unsigned int usage) const noexcept
  {
    uploadBytes(values.data(), values.size_bytes(), usage);
  }

  [[nodiscard]] bool isValid() const noexcept { return id_ != 0U; }
  [[nodiscard]] unsigned int id() const noexcept { return id_; }
  [[nodiscard]] unsigned int target() const noexcept { return target_; }

private:
  unsigned int id_ = 0;
  unsigned int target_ = 0;
};

} // namespace ggm::gui
