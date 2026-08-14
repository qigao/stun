/* Flex OpenGL backend. The selected drawing engine is an internal detail. */
#pragma once

#include "flex/bridge/renderer.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace flex {

namespace opengl_backend {

using OpenGLProcAddress = void (*)();
using OpenGLProcLoader = OpenGLProcAddress (*)(void *user_data, const char *name);

/**
 * Creation options for the current OpenGL context.
 *
 * The host must make a compatible OpenGL context current before renderer creation.
 * The renderer owns its internal drawing engine but
 * never owns the host OpenGL context. The same context must be current while
 * rendering and while destroying the renderer.
 *
 * The gCanvas engine additionally requires get_proc_address. NanoVG retains the
 * legacy antialias/stencil/debug creation flags. The host always owns presentation.
 */
struct OpenGLCanvas {
  bool antialias = true;
  bool stencil_strokes = true;
  bool debug = false;
  void *proc_loader_user_data = nullptr;
  OpenGLProcLoader get_proc_address = nullptr;
};

inline void init() {}
inline void shutdown() {}

inline std::unordered_map<std::string, std::string> &font_registry() {
  static std::unordered_map<std::string, std::string> fonts;
  return fonts;
}

inline std::mutex &font_registry_mutex() {
  static std::mutex mutex;
  return mutex;
}

inline std::string infer_font_name(const char *path) {
  if (!path || !*path) {
    return {};
  }
  const std::string value(path);
  const std::size_t slash = value.find_last_of("/\\");
  const std::size_t start = slash == std::string::npos ? 0 : slash + 1;
  std::size_t dot = value.find_last_of('.');
  if (dot == std::string::npos || dot < start) {
    dot = value.size();
  }
  return value.substr(start, dot - start);
}

inline bool load_font(const char *name, const char *path) {
  if (!name || !*name || !path || !*path) {
    return false;
  }
  std::lock_guard<std::mutex> lock(font_registry_mutex());
  font_registry()[name] = path;
  return true;
}

inline bool load_font(const char *path) {
  const std::string name = infer_font_name(path);
  return !name.empty() && load_font(name.c_str(), path);
}

inline void unload_font(const char *name) {
  if (!name || !*name) {
    return;
  }
  std::lock_guard<std::mutex> lock(font_registry_mutex());
  font_registry().erase(name);
}

inline bool register_backend();
std::unique_ptr<Renderer> create_renderer(const OpenGLCanvas *canvas);

} // namespace opengl_backend

std::unique_ptr<Renderer> create_opengl_renderer(CanvasHandle canvas);

namespace opengl_backend {

inline bool register_backend() {
  register_renderer_backend(RendererBackend::OpenGL,
                            static_cast<RendererFactory>(&create_opengl_renderer));
  if (!default_renderer_factory()) {
    return set_default_renderer_backend(RendererBackend::OpenGL);
  }
  return true;
}

inline std::unique_ptr<Renderer> create_renderer(const OpenGLCanvas *canvas) {
  return create_opengl_renderer(const_cast<OpenGLCanvas *>(canvas));
}

} // namespace opengl_backend
} // namespace flex
