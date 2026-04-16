#include "core/logging.hpp"
#include "gui/application.hpp"

#include <cstdlib>
#include <print>

int
main()
{
  ggm::logging::init();
  auto app = ggm::gui::Application::create();
  if (!app) {
    std::println(stderr, "Fatal: {}", app.error());
    return EXIT_FAILURE;
  }
  return app->run();
}
