#include "backends/opengl/init.h"
#include "backends/tui/init.h"
#include "backends/vulkan/init.h"
#include "flex/render.h"
#include "tinytest.h"

#ifdef _WIN32
  #include "backends/d2d/init.h"
#endif

using namespace flex;

namespace {

struct RegistryGuard {
  RendererFactory default_factory = default_renderer_factory();
  RendererFactory opengl = renderer_backend_factory(RendererBackend::OpenGL);
  RendererFactory tui = renderer_backend_factory(RendererBackend::TUI);
  RendererFactory vulkan = renderer_backend_factory(RendererBackend::Vulkan);
#ifdef _WIN32
  RendererFactory d2d = renderer_backend_factory(RendererBackend::Direct2D);
#endif

  ~RegistryGuard() {
    register_renderer_backend(RendererBackend::OpenGL, opengl);
    register_renderer_backend(RendererBackend::TUI, tui);
    register_renderer_backend(RendererBackend::Vulkan, vulkan);
#ifdef _WIN32
    register_renderer_backend(RendererBackend::Direct2D, d2d);
#endif
    set_default_renderer_factory(default_factory);
  }
};

} // namespace

suite("flex backend modules") {
  it("exposes canonical module entry points") {
    RegistryGuard guard;

    check_true(opengl_backend::register_backend());
    check_true(tui_backend::register_backend());
    check_true(vulkan_backend::register_backend());
#ifdef _WIN32
    check_true(d2d_backend::register_backend());
#endif

    check_true(is_renderer_backend_available(RendererBackend::OpenGL));
    check_true(is_renderer_backend_available(RendererBackend::TUI));
    check_true(is_renderer_backend_available(RendererBackend::Vulkan));
#ifdef _WIN32
    check_true(is_renderer_backend_available(RendererBackend::Direct2D));
#endif
  }
}
