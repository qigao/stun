/*
 * Flex Engine - ThorVG Backend Initialization
 *
 * ThorVG-specific initialization and font management.
 * Only include this if you're using ThorVG backend.
 *
 * Usage:
 *   #include "backends/thorvg/init.h"
 *   flex::thorvg_backend::init();
 *   // ... use flex engine with ThorVG renderer
 *   flex::thorvg_backend::shutdown();
 *
 * Note: This is backend-specific. If using NanoVG or other backends,
 *       include the corresponding init header instead.
 */

#pragma once

#include "backends/renderer.h"
#include "flex/bridge/renderer_thorvg.h"

#include <tlog.h>
#include <fstream>
#include <thread>
#include <vector>
#include <thorvg.h>

namespace flex {
namespace thorvg_backend {

// Initialize the ThorVG backend (call once at startup)
inline void init() {
  unsigned int threads = std::thread::hardware_concurrency();
  if (threads < 4)
    threads = 4;
  auto result = tvg::Initializer::init(threads);
  TLOG_INFO("[flex::thorvg_backend::init] ThorVG initialized with {} threads (result={})",
            threads, (int) result);
}

// Shutdown the ThorVG backend (call at exit)
inline void shutdown() { tvg::Initializer::term(); }

// Load a font from file (TTF, OTF)
// Returns true on success
inline bool load_font(const char *path) {
  if (!path)
    return false;
  return tvg::Text::load(path) == tvg::Result::Success;
}

// Load a font from file with a custom name
inline bool load_font(const char *name, const char *path) {
  if (!name || !path)
    return false;

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

inline bool register_backend() {
  flex::register_renderer_backend(
      RendererBackend::ThorVG,
      static_cast<RendererFactory>(&flex::create_thorvg_renderer));
  if (!flex::default_renderer_factory()) {
    flex::set_default_renderer_factory(static_cast<RendererFactory>(&flex::create_thorvg_renderer));
  }
  return true;
}

inline std::unique_ptr<Renderer> create_renderer(tvg::Canvas* canvas) {
  return flex::create_thorvg_renderer(static_cast<CanvasHandle>(canvas));
}

} // namespace thorvg_backend

// Legacy ThorVG-only aliases. Prefer flex::thorvg_backend::* in new code.
inline void init() { thorvg_backend::init(); }
inline void shutdown() { thorvg_backend::shutdown(); }
inline bool load_font(const char *path) { return thorvg_backend::load_font(path); }
inline bool load_font(const char *name, const char *path) {
  return thorvg_backend::load_font(name, path);
}
inline bool load_font_data(const char *name, const char *data, uint32_t size) {
  return thorvg_backend::load_font_data(name, data, size);
}
inline void unload_font(const char *name) { thorvg_backend::unload_font(name); }

} // namespace flex
