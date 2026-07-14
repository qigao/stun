#include "flex/render.h"
#include "tinytest.h"

using namespace flex;

namespace {

std::unique_ptr<Renderer> null_renderer_factory(CanvasHandle) {
    return nullptr;
}

} // namespace

suite("flex::render") {
    it("registers backend factories without a window or GPU") {
        const auto previous = register_renderer_backend(RendererBackend::Custom,
                                                        null_renderer_factory);

        check(is_renderer_backend_available(RendererBackend::Custom));
        check(renderer_backend_factory(RendererBackend::Custom) ==
              null_renderer_factory);

        register_renderer_backend(RendererBackend::Custom, previous);
    }

    it("returns null when a backend is unavailable") {
        const auto previous = register_renderer_backend(RendererBackend::Custom, nullptr);

        check_false(is_renderer_backend_available(RendererBackend::Custom));
        check(create_renderer(RendererBackend::Custom, nullptr) == nullptr);

        register_renderer_backend(RendererBackend::Custom, previous);
    }
}
