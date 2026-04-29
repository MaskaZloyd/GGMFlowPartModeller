#pragma once

#include "core/blade_results.hpp"

#include <vector>

namespace ggm::gui {

struct BladePlanViewport
{
  double minX = 0.0;
  double maxX = 0.0;
  double minY = 0.0;
  double maxY = 0.0;
  int widthPx = 0;
  int heightPx = 0;
};

class BladePlanRenderer
{
public:
  BladePlanRenderer() = default;
  ~BladePlanRenderer();
  BladePlanRenderer(const BladePlanRenderer&) = delete;
  BladePlanRenderer& operator=(const BladePlanRenderer&) = delete;
  BladePlanRenderer(BladePlanRenderer&& other) noexcept;
  BladePlanRenderer& operator=(BladePlanRenderer&& other) noexcept;

  [[nodiscard]] BladePlanViewport
  render(const core::BladeDesignResults& results, int viewportWidth, int viewportHeight) noexcept;

private:
  [[nodiscard]] bool isReady() const noexcept;
  void initGl() noexcept;
  void destroy() noexcept;

  unsigned int vao_ = 0;
  unsigned int vbo_ = 0;
  unsigned int shaderProgram_ = 0;
  int viewportUniformLocation_ = -1;
  int colorUniformLocation_ = -1;
  int useVertexColorUniformLocation_ = -1;
  int alphaUniformLocation_ = -1;
  float maxLineWidth_ = 1.0F;
  std::vector<float> scratchVertices_;
};

}
