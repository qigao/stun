/*
 * Flex Engine - Renderer Backend Factory
 */

#include "flex/bridge/renderer.h"

#include <array>
#include <cstddef>
#include <mutex>

namespace flex {
namespace {

constexpr std::size_t backend_count =
    static_cast<std::size_t>(RendererBackend::Custom) + 1;

std::size_t backend_index(RendererBackend backend) {
    return static_cast<std::size_t>(backend);
}

std::array<RendererFactory, backend_count>& backend_registry() {
    static std::array<RendererFactory, backend_count> registry{};
    return registry;
}

std::mutex& backend_registry_mutex() {
    static std::mutex mutex;
    return mutex;
}

RendererFactory& default_backend_factory_slot() {
    static RendererFactory factory = nullptr;
    return factory;
}

bool is_valid_backend(RendererBackend backend) {
    return backend_index(backend) < backend_count;
}

} // namespace

const char* renderer_backend_name(RendererBackend backend) {
    switch (backend) {
    case RendererBackend::OpenGL:
        return "OpenGL";
    case RendererBackend::Vulkan:
        return "Vulkan";
    case RendererBackend::TUI:
        return "TUI";
    case RendererBackend::Direct2D:
        return "Direct2D";
    case RendererBackend::Custom:
        return "Custom";
    }
    return "Unknown";
}

RendererFactory renderer_backend_factory(RendererBackend backend) {
    if (!is_valid_backend(backend)) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(backend_registry_mutex());
    return backend_registry()[backend_index(backend)];
}

RendererFactory default_renderer_factory() {
    std::lock_guard<std::mutex> lock(backend_registry_mutex());
    return default_backend_factory_slot();
}

RendererFactory set_default_renderer_factory(RendererFactory factory) {
    std::lock_guard<std::mutex> lock(backend_registry_mutex());
    RendererFactory previous = default_backend_factory_slot();
    default_backend_factory_slot() = factory;
    return previous;
}

bool set_default_renderer_backend(RendererBackend backend) {
    if (!is_valid_backend(backend)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(backend_registry_mutex());
    RendererFactory factory = backend_registry()[backend_index(backend)];
    if (!factory) {
        return false;
    }
    default_backend_factory_slot() = factory;
    return true;
}

RendererFactory register_renderer_backend(RendererBackend backend, RendererFactory factory) {
    if (!is_valid_backend(backend)) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(backend_registry_mutex());
    RendererFactory previous = backend_registry()[backend_index(backend)];
    backend_registry()[backend_index(backend)] = factory;
    return previous;
}

bool is_renderer_backend_available(RendererBackend backend) {
    return renderer_backend_factory(backend) != nullptr;
}

std::unique_ptr<Renderer> create_renderer(CanvasHandle handle) {
    RendererFactory factory = default_renderer_factory();
    if (!factory) {
        return nullptr;
    }
    return factory(handle);
}

std::unique_ptr<Renderer> create_renderer(RendererBackend backend, CanvasHandle handle) {
    RendererFactory factory = renderer_backend_factory(backend);
    if (!factory) {
        return nullptr;
    }
    return factory(handle);
}

std::unique_ptr<Renderer> create_renderer(const RendererCreateInfo& info) {
    if (info.backend == RendererBackend::Custom) {
        return create_renderer(info.handle);
    }
    return create_renderer(info.backend, info.handle);
}

} // namespace flex
