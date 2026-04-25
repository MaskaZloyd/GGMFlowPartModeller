#pragma once

#include <expected>
#include <string>

namespace ggm::core {

enum class CoreError
{
  InvalidParameter,
  GeometryBuildFailed,
  SerializationFailed,
  FileNotFound,
  FileWriteFailed,
  ParseError,
  GridBuildFailed,
  SolverFailed,
  Cancelled,
  RenderFailed,
};

/// Human-readable error description.
[[nodiscard]] constexpr const char*
toString(CoreError err) noexcept
{
  switch (err) {
    case CoreError::InvalidParameter:
      return "Invalid parameter value";
    case CoreError::GeometryBuildFailed:
      return "Geometry build failed";
    case CoreError::SerializationFailed:
      return "Serialization failed";
    case CoreError::FileNotFound:
      return "File not found";
    case CoreError::FileWriteFailed:
      return "File write failed";
    case CoreError::ParseError:
      return "JSON parse error";
    case CoreError::GridBuildFailed:
      return "Grid build failed";
    case CoreError::SolverFailed:
      return "FEM solver failed";
    case CoreError::Cancelled:
      return "Cancelled";
    case CoreError::RenderFailed:
      return "Render target creation failed";
  }
  return "Unknown error";
}

template <typename T>
using Result = std::expected<T, CoreError>;

}
