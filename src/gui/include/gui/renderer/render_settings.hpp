#pragma once

namespace ggm::gui {

struct RenderSettings
{
  /// Base overlays.
  bool showCoordGrid = true;
  bool showComputationalGrid = false;

  /// Velocity-driven visualization.
  bool showVelocityHeatmap = false;    ///< fill FEM triangles by |V|
  bool colorStreamlinesBySpeed = true; ///< per-vertex viridis color along each streamline

  /// Diagnostic overlays.
  bool showCriticalMarkers = true; ///< inlet / outlet circles
  bool showInvalidChords = true;   ///< red highlight for degenerate chords
  bool showAngleTangents = false;  ///< short arrows at al1, al2, al02 positions

  /// Hover inspector (tooltip with local ξ / ψ / |V|).
  bool showHoverInspector = true;

  /// Snapshot overlay: ghost-render a saved geometry on top for comparison.
  bool showSnapshotOverlay = false;

  /// Line widths for the base drawing pass.
  float hubLineWidth = 5.0F;
  float shroudLineWidth = 5.0F;
  float meanLineWidth = 3.0F;
  float streamlineWidth = 2.0F;

  /// Number of streamlines to render when the solver produced them.
  /// 0 = use all streamlines as-is.
  int streamlineCountHint = 0;
};

}
