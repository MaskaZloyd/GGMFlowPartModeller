#pragma once

#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>

#include <memory>

namespace ggm::logging {

// Initialize spdlog: three sinks (stderr color, rotating file, in-memory
// ring buffer for the GUI panel) and three named loggers: "core", "solver",
// "gui". Pattern is identical across sinks:
//     [YYYY-MM-DD HH:MM:SS.mmm] [level] [logger] message
// Safe to call multiple times; subsequent calls are no-ops.
void init();

// Ring-buffer sink that backs the GUI log panel. Holds the last ~500
// formatted messages. Thread-safe.
using GuiRingSink = spdlog::sinks::ringbuffer_sink_mt;
std::shared_ptr<GuiRingSink> guiSink();

// Convenience accessors (equivalent to spdlog::get("<name>")).
inline std::shared_ptr<spdlog::logger> core() { return spdlog::get("core"); }
inline std::shared_ptr<spdlog::logger> solver() { return spdlog::get("solver"); }
inline std::shared_ptr<spdlog::logger> gui() { return spdlog::get("gui"); }

} // namespace ggm::logging
