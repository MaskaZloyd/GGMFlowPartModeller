#include "gui/renderer/opengl/gl_shader_program.hpp"

#include "core/logging.hpp"
#include "renderer/gl_headers.hpp"

#include <string>
#include <utility>

namespace ggm::gui {
namespace {

void
logShaderError(std::string_view debugName, std::string_view stage, const std::string& message)
{
  if (auto logger = logging::gui()) {
    logger->error("OpenGL shader {} {} failed: {}", debugName, stage, message);
  }
}

[[nodiscard]] std::string
shaderInfoLog(GLuint shader)
{
  GLint length = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
  if (length <= 1) {
    return {};
  }

  std::string log(static_cast<std::size_t>(length), '\0');
  glGetShaderInfoLog(shader, length, nullptr, log.data());
  return log;
}

[[nodiscard]] std::string
programInfoLog(GLuint program)
{
  GLint length = 0;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
  if (length <= 1) {
    return {};
  }

  std::string log(static_cast<std::size_t>(length), '\0');
  glGetProgramInfoLog(program, length, nullptr, log.data());
  return log;
}

[[nodiscard]] GLuint
compileShader(GLenum type,
              std::string_view source,
              std::string_view debugName,
              std::string_view stage)
{
  const auto shader = glCreateShader(type);
  if (shader == 0U) {
    logShaderError(debugName, stage, "glCreateShader returned 0");
    return 0U;
  }

  const auto* sourcePtr = source.data();
  const auto sourceLength = static_cast<GLint>(source.size());
  glShaderSource(shader, 1, &sourcePtr, &sourceLength);
  glCompileShader(shader);

  GLint success = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (success == GL_FALSE) {
    logShaderError(debugName, stage, shaderInfoLog(shader));
    glDeleteShader(shader);
    return 0U;
  }
  return shader;
}

} // namespace

GlShaderProgram::~GlShaderProgram()
{
  destroy();
}

GlShaderProgram::GlShaderProgram(GlShaderProgram&& other) noexcept
  : id_(std::exchange(other.id_, 0U))
{
}

GlShaderProgram&
GlShaderProgram::operator=(GlShaderProgram&& other) noexcept
{
  if (this != &other) {
    destroy();
    id_ = std::exchange(other.id_, 0U);
  }
  return *this;
}

bool
GlShaderProgram::create(std::string_view vertexSource,
                        std::string_view fragmentSource,
                        std::string_view debugName) noexcept
{
  destroy();

  const auto vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, debugName, "vertex");
  if (vertexShader == 0U) {
    return false;
  }

  const auto fragmentShader =
    compileShader(GL_FRAGMENT_SHADER, fragmentSource, debugName, "fragment");
  if (fragmentShader == 0U) {
    glDeleteShader(vertexShader);
    return false;
  }

  const auto program = glCreateProgram();
  if (program == 0U) {
    logShaderError(debugName, "program", "glCreateProgram returned 0");
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return false;
  }

  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);

  GLint success = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &success);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  if (success == GL_FALSE) {
    logShaderError(debugName, "link", programInfoLog(program));
    glDeleteProgram(program);
    return false;
  }

  id_ = program;
  return true;
}

void
GlShaderProgram::destroy() noexcept
{
  if (id_ != 0U) {
    glDeleteProgram(id_);
    id_ = 0U;
  }
}

void
GlShaderProgram::use() const noexcept
{
  glUseProgram(id_);
}

void
GlShaderProgram::unuse() noexcept
{
  glUseProgram(0);
}

int
GlShaderProgram::uniformLocation(const char* name) const noexcept
{
  return isValid() ? glGetUniformLocation(id_, name) : -1;
}

void
GlShaderProgram::setUniform2f(int location, float x, float y) const noexcept
{
  if (location >= 0) {
    glUniform2f(location, x, y);
  }
}

void
GlShaderProgram::setUniform3f(int location, float x, float y, float z) const noexcept
{
  if (location >= 0) {
    glUniform3f(location, x, y, z);
  }
}

void
GlShaderProgram::setUniform4f(int location, float x, float y, float z, float w) const noexcept
{
  if (location >= 0) {
    glUniform4f(location, x, y, z, w);
  }
}

void
GlShaderProgram::setUniform1f(int location, float value) const noexcept
{
  if (location >= 0) {
    glUniform1f(location, value);
  }
}

void
GlShaderProgram::setUniform1i(int location, int value) const noexcept
{
  if (location >= 0) {
    glUniform1i(location, value);
  }
}

} // namespace ggm::gui
