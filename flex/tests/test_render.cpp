#include "flex/render.h"
#include "tinytest.hpp"

using namespace flex;

namespace {

std::unique_ptr<Renderer> null_renderer_factory(CanvasHandle) { return nullptr; }

} // namespace

suite("flex::render") {
  it("distinguishes platform and Tango presentation backends") {
    check_equal(renderer_backend_name(RendererBackend::OpenGL), "OpenGL");
    check_equal(renderer_backend_name(RendererBackend::Vulkan), "Vulkan");
    check_equal(renderer_backend_name(RendererBackend::TUI), "TUI");
    check_equal(renderer_backend_name(RendererBackend::TangoOpenGL),
                "TangoOpenGL");
    check_equal(renderer_backend_name(RendererBackend::Direct2D), "Direct2D");
  }

  it("registers backend factories without a window or GPU") {
    const auto previous = register_renderer_backend(RendererBackend::Custom, null_renderer_factory);

    check_true(is_renderer_backend_available(RendererBackend::Custom));
    check_true(renderer_backend_factory(RendererBackend::Custom) == null_renderer_factory);

    register_renderer_backend(RendererBackend::Custom, previous);
  }

  it("does not redirect an unavailable Tango OpenGL presenter to TTY or generic GL") {
    const auto previous =
        register_renderer_backend(RendererBackend::TangoOpenGL, nullptr);
    check_false(is_renderer_backend_available(RendererBackend::TangoOpenGL));
    check_null(create_renderer(RendererBackend::TangoOpenGL, nullptr).get());
    check_false(set_default_renderer_backend(RendererBackend::TangoOpenGL));
    register_renderer_backend(RendererBackend::TangoOpenGL, previous);
  }

  it("returns null when a backend is unavailable") {
    const auto previous = register_renderer_backend(RendererBackend::Custom, nullptr);

    check_false(is_renderer_backend_available(RendererBackend::Custom));
    check_null(create_renderer(RendererBackend::Custom, nullptr).get());

    register_renderer_backend(RendererBackend::Custom, previous);
  }

}
