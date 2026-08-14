#include "backends/opengl/init.h"
#if defined(FLEX_OPENGL_ENGINE_GCANVAS)
#include "flex/render/engines/gcanvas.h"
#else
#include "flex/render/engines/nanovg.h"
#endif

#include <utility>
#include <vector>

namespace flex {

std::unique_ptr<Renderer> create_opengl_renderer(CanvasHandle canvas) {
  const auto *options = static_cast<const opengl_backend::OpenGLCanvas *>(canvas);
  if (!options) {
    return nullptr;
  }
#if defined(FLEX_OPENGL_ENGINE_GCANVAS)
  if (!options->get_proc_address) {
    return nullptr;
  }
  auto renderer = render::engines::gcanvas::create_external_opengl_renderer(
      options->get_proc_address, options->proc_loader_user_data);
  std::vector<std::pair<std::string, std::string>> fonts;
  {
    std::lock_guard<std::mutex> lock(opengl_backend::font_registry_mutex());
    fonts.reserve(opengl_backend::font_registry().size());
    for (const auto &font : opengl_backend::font_registry()) {
      fonts.push_back(font);
    }
  }
  for (const auto &font : fonts) {
    renderer->register_font(font.first, font.second);
  }
  return renderer;
#else
  return render::engines::nanovg::create_renderer(
      options->antialias, options->stencil_strokes, options->debug);
#endif
}

} // namespace flex
