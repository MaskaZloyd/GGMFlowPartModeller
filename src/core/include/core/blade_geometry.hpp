#pragma once

#include "core/blade_params.hpp"
#include "core/blade_results.hpp"
#include "core/error.hpp"

#include <span>

namespace ggm::core {

[[nodiscard]] double
evaluateBladeAngleDeg(const BladeDesignParams& params, double t) noexcept;

[[nodiscard]] double
evaluateBladeThicknessMm(const BladeDesignParams& params, double t) noexcept;

[[nodiscard]] Result<BladeContour>
buildCylindricalBladeContour(std::span<const BladeSectionSample> sections) noexcept;

[[nodiscard]] BladeContour
rotateBladeContour(const BladeContour& contour, double angleRad);

}
