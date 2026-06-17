#pragma once

#include <string_view>

namespace ggm::gui {

class GlShaderProgram
{
public:
  GlShaderProgram() = default;
  ~GlShaderProgram();
  GlShaderProgram(const GlShaderProgram&) = delete;
  GlShaderProgram& operator=(const GlShaderProgram&) = delete;
  GlShaderProgram(GlShaderProgram&& other) noexcept;
  GlShaderProgram& operator=(GlShaderProgram&& other) noexcept;

  [[nodiscard]] bool create(std::string_view vertexSource,
                            std::string_view fragmentSource,
                            std::string_view debugName) noexcept;
  void destroy() noexcept;

  void use() const noexcept;
  static void unuse() noexcept;

  [[nodiscard]] bool isValid() const noexcept { return id_ != 0U; }
  [[nodiscard]] unsigned int id() const noexcept { return id_; }
  [[nodiscard]] int uniformLocation(const char* name) const noexcept;

  void setUniform2f(int location, float x, float y) const noexcept;
  void setUniform3f(int location, float x, float y, float z) const noexcept;
  void setUniform4f(int location, float x, float y, float z, float w) const noexcept;
  void setUniform1f(int location, float value) const noexcept;
  void setUniform1i(int location, int value) const noexcept;

private:
  unsigned int id_ = 0;
};

} // namespace ggm::gui
