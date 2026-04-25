#include "core/logging.hpp"

#include <array>
#include <mutex>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace ggm::logging {

namespace {

constexpr const char* LOG_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v";
constexpr std::size_t GUI_RING_CAPACITY = 500;

std::once_flag g_initFlag;
std::shared_ptr<GuiRingSink> g_guiSink;

}

void
init()
{
  std::call_once(g_initFlag, [] {
    auto console = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    auto file = std::make_shared<spdlog::sinks::basic_file_sink_mt>("ggm.log", true);
    g_guiSink = std::make_shared<GuiRingSink>(GUI_RING_CAPACITY);

    for (auto& sink : std::array<spdlog::sink_ptr, 3>{console, file, g_guiSink}) {
      sink->set_pattern(LOG_PATTERN);
    }

    spdlog::sinks_init_list sinks{console, file, g_guiSink};
    for (const char* name : {"core", "solver", "gui"}) {
      auto logger = std::make_shared<spdlog::logger>(name, sinks);
      logger->set_level(spdlog::level::debug);
      logger->flush_on(spdlog::level::warn);
      spdlog::register_logger(logger);
    }
    spdlog::set_default_logger(spdlog::get("core"));
  });
}

std::shared_ptr<GuiRingSink>
guiSink()
{
  return g_guiSink;
}

}
