/*
 * Flex Engine - ThorVG Backend Initialization
 *
 * ThorVG-specific initialization and font management.
 * Only include this if you're using ThorVG backend.
 *
 * Usage:
 *   #include "flex/backends/thorvg/init.h"
 *   flex::init();
 *   // ... use flex engine with ThorVG renderer
 *   flex::shutdown();
 *
 * Note: This is backend-specific. If using NanoVG or other backends,
 *       include the corresponding init header instead.
 */

#pragma once

#include <fmtlog.h>
#include <fstream>
#include <thread>
#include <vector>
#include <thorvg.h>

namespace flex {

// ============================================================================
// Engine Initialization
// ============================================================================

// Initialize the Flex engine (call once at startup)
inline void init() {
  // Initialize ThorVG with auto-detected threads
  tvg::Initializer::init(std::thread::hardware_concurrency());

  // Initialize fmtlog
  fmtlog::setLogLevel(fmtlog::DBG); // Enable all log levels (DBG, INF, WRN, ERR)
  fmtlog::setThreadName("main");

  // Optional: Output to file for debugging
  // fmtlog::setLogFile("flex_engine.log", true);
}

// Shutdown the Flex engine (call at exit)
inline void shutdown() {
  // Flush and shutdown fmtlog
  fmtlog::shutdown();

  // Terminate ThorVG
  tvg::Initializer::term();
}

// ============================================================================
// Font Management
// ============================================================================

// Load a font from file (TTF, OTF)
// Returns true on success
inline bool load_font(const char *path) {
  if (!path)
    return false;
  return tvg::Text::load(path) == tvg::Result::Success;
}

// Load a font from file with a custom name
inline bool load_font(const char *name, const char *path) {
  // ThorVG's Text::load(filename) uses filename as the font name
  // For custom name, we need to load from memory
  if (!name || !path)
    return false;

  // Read file into memory
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open())
    return false;

  auto size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> buffer(size);
  if (!file.read(buffer.data(), size))
    return false;

  return tvg::Text::load(name, buffer.data(), static_cast<uint32_t>(size), "ttf", true) ==
         tvg::Result::Success;
}

// Load a font from memory
inline bool load_font_data(const char *name, const char *data, uint32_t size) {
  if (!name || !data || size == 0)
    return false;
  return tvg::Text::load(name, data, size, "ttf", true) == tvg::Result::Success;
}

// Unload a previously loaded font
inline void unload_font(const char *name) {
  if (name) {
    tvg::Text::unload(name);
  }
}

} // namespace flex
