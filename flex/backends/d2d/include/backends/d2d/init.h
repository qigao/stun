/*
 * Flex Engine - Direct2D Backend Initialization
 *
 * Initialize Flex with Direct2D rendering backend (Windows only).
 */

#pragma once

#ifdef _WIN32

#include "flex/bridge/renderer.h"
#include "flex/bridge/renderer_d2d.h"

#include <d2d1.h>
#include <dwrite.h>
#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <mutex>
#include <string>
#include <unordered_map>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace flex {
namespace d2d_backend {

inline Microsoft::WRL::ComPtr<ID2D1Factory>& d2d_factory() {
    static Microsoft::WRL::ComPtr<ID2D1Factory> factory;
    return factory;
}

inline Microsoft::WRL::ComPtr<IDWriteFactory>& dwrite_factory() {
    static Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    return factory;
}

inline Microsoft::WRL::ComPtr<IWICImagingFactory>& wic_factory() {
    static Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    return factory;
}

inline Microsoft::WRL::ComPtr<ID2D1Factory>& g_d2d_factory = d2d_factory();
inline Microsoft::WRL::ComPtr<IDWriteFactory>& g_dwrite_factory = dwrite_factory();
inline Microsoft::WRL::ComPtr<IWICImagingFactory>& g_wic_factory = wic_factory();

inline HRESULT& com_init_result() {
    static HRESULT hr = E_FAIL;
    return hr;
}

inline std::unordered_map<std::string, std::string>& font_registry() {
    static std::unordered_map<std::string, std::string> fonts;
    return fonts;
}

inline std::mutex& font_registry_mutex() {
    static std::mutex mtx;
    return mtx;
}

inline std::string infer_font_family(const char* path) {
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

    std::string stem = value.substr(start, dot - start);
    std::string lower = stem;
    for (char& ch : lower) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }

    if (lower == "segoeui" || lower == "segoeuib") {
        return "Segoe UI";
    }
    if (lower == "arial" || lower == "arialbd") {
        return "Arial";
    }
    if (lower == "consola" || lower == "consolab") {
        return "Consolas";
    }

    return stem;
}

inline void init() {
    if (FAILED(com_init_result())) {
        com_init_result() = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (com_init_result() == RPC_E_CHANGED_MODE) {
            com_init_result() = S_FALSE;
        }
    }

    if (!d2d_factory()) {
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_factory().GetAddressOf());
    }

    if (!dwrite_factory()) {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown**>(dwrite_factory().GetAddressOf()));
    }

    if (!wic_factory()) {
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(wic_factory().GetAddressOf()));
    }
}

inline void shutdown() {
    d2d_factory().Reset();
    dwrite_factory().Reset();
    wic_factory().Reset();
    if (SUCCEEDED(com_init_result())) {
        CoUninitialize();
    }
    com_init_result() = E_FAIL;
}

inline bool load_font(const char* path) {
    if (!path || !*path) {
        return false;
    }

    const std::string family = infer_font_family(path);
    if (family.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(font_registry_mutex());
    font_registry()[family] = family;
    return true;
}

inline bool load_font(const char* name, const char* path) {
    if (!name || !*name) {
        return false;
    }

    std::string family = infer_font_family(path);
    if (family.empty()) {
        family = name;
    }

    std::lock_guard<std::mutex> lock(font_registry_mutex());
    font_registry()[name] = family;
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
        RendererBackend::Direct2D,
        static_cast<RendererFactory>(&flex::create_d2d_renderer));
    flex::set_default_renderer_backend(RendererBackend::Direct2D);
    return true;
}

inline std::unique_ptr<Renderer> create_renderer(ID2D1RenderTarget* render_target) {
    return flex::create_d2d_renderer(static_cast<CanvasHandle>(render_target));
}

} // namespace d2d_backend

} // namespace flex

#endif // _WIN32
