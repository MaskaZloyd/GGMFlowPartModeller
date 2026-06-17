#include "core/geometry.hpp"
#include "gui/renderer/geometry/plot_viewport.hpp"
#include "gui/renderer/geometry/sdf_line_renderer_2d.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

namespace {

ggm::core::MeridionalGeometry
makeSimpleGeometry()
{
  auto geom = ggm::core::MeridionalGeometry{};
  geom.hubCurve = {{0.0, 0.0}, {10.0, 0.0}};
  geom.shroudCurve = {{0.0, 5.0}, {10.0, 5.0}};
  return geom;
}

} // namespace

TEST_CASE("PlotViewport preserves data aspect ratio inside framebuffer", "[gui][rendering]")
{
  const auto geom = makeSimpleGeometry();
  const auto viewport = ggm::gui::PlotViewport::fromGeometry(geom, 400, 200);

  REQUIRE(viewport.hasData());
  REQUIRE(viewport.widthPx() == 400);
  REQUIRE(viewport.heightPx() == 200);
  REQUIRE_THAT(viewport.rangeZ() / viewport.rangeR(), WithinAbs(2.0, 1.0e-12));
}

TEST_CASE("PlotViewport maps data corners to OpenGL pixel coordinates", "[gui][rendering]")
{
  const auto geom = makeSimpleGeometry();
  const auto viewport = ggm::gui::PlotViewport::fromGeometry(geom, 400, 200);
  const auto map = viewport.toMap();

  const auto bottomLeft = viewport.dataToPixel({map.minZ, map.minR});
  const auto topRight = viewport.dataToPixel({map.maxZ, map.maxR});

  REQUIRE_THAT(bottomLeft.x, WithinAbs(0.0, 1.0e-9));
  REQUIRE_THAT(bottomLeft.y, WithinAbs(0.0, 1.0e-9));
  REQUIRE_THAT(topRight.x, WithinAbs(400.0, 1.0e-9));
  REQUIRE_THAT(topRight.y, WithinAbs(200.0, 1.0e-9));
}

TEST_CASE("clipSegmentToRect clips a crossing segment in screen space", "[gui][rendering]")
{
  auto a = ggm::gui::PixelPoint{-100.0, 50.0};
  auto b = ggm::gui::PixelPoint{300.0, 50.0};
  const auto rect = ggm::gui::PixelRect{0.0, 0.0, 200.0, 100.0};

  const bool visible = ggm::gui::clipSegmentToRect(a, b, rect);

  REQUIRE(visible);
  REQUIRE_THAT(a.x, WithinAbs(0.0, 1.0e-9));
  REQUIRE_THAT(a.y, WithinAbs(50.0, 1.0e-9));
  REQUIRE_THAT(b.x, WithinAbs(200.0, 1.0e-9));
  REQUIRE_THAT(b.y, WithinAbs(50.0, 1.0e-9));
}

TEST_CASE("clipSegmentToRect rejects a segment outside one side", "[gui][rendering]")
{
  auto a = ggm::gui::PixelPoint{-100.0, 20.0};
  auto b = ggm::gui::PixelPoint{-10.0, 80.0};
  const auto rect = ggm::gui::PixelRect{0.0, 0.0, 200.0, 100.0};

  REQUIRE(!ggm::gui::clipSegmentToRect(a, b, rect));
}
