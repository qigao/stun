/*
 * Flex Engine - NanoVG Backend Initialization
 *
 * Initialize Flex with NanoVG rendering backend.
 */

#pragma once

#include "backends/renderer.h"
#include "flex/bridge/renderer_nanovg.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

struct NVGcontext;

namespace flex {
namespace nanovg_backend {

inline bool load_font(const char* name, const char* path);

inline void init() {
    // NanoVG doesn't require global init.
}

inline void shutdown() {}

inline std::unordered_map<std::string, std::string>& font_registry() {
    static std::unordered_map<std::string, std::string> fonts;
    return fonts;
}

inline std::mutex& font_registry_mutex() {
    static std::mutex mtx;
    return mtx;
}

inline std::string infer_font_name(const char* path) {
    if (!path || !*path) {
        return {};
    }
    std::string value(path);
    size_t slash = value.find_last_of("/\\");
    size_t start = (slash == std::string::npos) ? 0 : slash + 1;
    size_t dot = value.find_last_of('.');
    if (dot == std::string::npos || dot < start) {
        dot = value.size();
    }
    return value.substr(start, dot - start);
}

inline bool load_font(const char* path) {
    if (!path || !*path) {
        return false;
    }
    const std::string name = infer_font_name(path);
    if (name.empty()) {
        return false;
    }
    return load_font(name.c_str(), path);
}

inline bool load_font(const char* name, const char* path) {
    if (!name || !*name || !path || !*path) {
        return false;
    }
    std::lock_guard<std::mutex> lock(font_registry_mutex());
    font_registry()[name] = path;
    return true;
}

inline void unload_font(const char* name) {
    if (!name || !*name) {
        return;
    }
    std::lock_guard<std::mutex> lock(font_registry_mutex());
    font_registry().erase(name);
}

inline bool register_backend() {
    flex::register_renderer_backend(
        RendererBackend::NanoVG,
        static_cast<RendererFactory>(&flex::create_nanovg_renderer));
    if (!flex::default_renderer_factory()) {
        flex::set_default_renderer_factory(static_cast<RendererFactory>(&flex::create_nanovg_renderer));
    }
    return true;
}

inline std::unique_ptr<Renderer> create_renderer(NVGcontext* vg) {
    return flex::create_nanovg_renderer(static_cast<CanvasHandle>(vg));
}

} // namespace nanovg_backend

inline std::unique_ptr<Renderer> create_renderer(RendererBackend backend, NVGcontext* vg) {
    if (backend == RendererBackend::NanoVG) {
        nanovg_backend::register_backend();
        return nanovg_backend::create_renderer(vg);
    }
    return create_renderer(backend, static_cast<CanvasHandle>(vg));
}

} // namespace flex
