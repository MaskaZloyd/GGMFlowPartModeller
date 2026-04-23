#include "core/streamline_extractor.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ggm::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("equidistantLevels returns exactly the requested number of streamlines",
          "[core][streamline_extractor]")
{
  const auto levels = equidistantLevels(4);

  REQUIRE(levels.size() == 4);
  REQUIRE_THAT(levels[0], WithinAbs(0.2, 1e-12));
  REQUIRE_THAT(levels[1], WithinAbs(0.4, 1e-12));
  REQUIRE_THAT(levels[2], WithinAbs(0.6, 1e-12));
  REQUIRE_THAT(levels[3], WithinAbs(0.8, 1e-12));
}

TEST_CASE("equidistantLevels keeps a single streamline on the midline",
          "[core][streamline_extractor]")
{
  const auto levels = equidistantLevels(1);

  REQUIRE(levels.size() == 1);
  REQUIRE_THAT(levels[0], WithinAbs(0.5, 1e-12));
}
