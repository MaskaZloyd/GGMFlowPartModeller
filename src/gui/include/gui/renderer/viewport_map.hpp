#pragma once

namespace ggm::gui {

/// Visible world bounds after the aspect correction. Panels use this to draw
/// overlays (legend, scale bar) whose position depends on the pixel-to-world
/// mapping.
struct ViewportMap
{
  double minZ = 0.0;
  double maxZ = 0.0;
  double minR = 0.0;
  double maxR = 0.0;
  int widthPx = 0;
  int heightPx = 0;
};

} // namespace ggm::gui
