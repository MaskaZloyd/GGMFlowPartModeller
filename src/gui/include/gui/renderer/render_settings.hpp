#pragma once

namespace ggm::gui {

struct RenderSettings
{
  bool showCoordGrid = true;
  bool showComputationalGrid = false;
  float hubLineWidth = 2.5F;
  float shroudLineWidth = 2.5F;
  float meanLineWidth = 1.5F;
  float streamlineWidth = 1.0F;
};

} // namespace ggm::gui
