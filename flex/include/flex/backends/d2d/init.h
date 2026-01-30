/*
 * Flex Engine - Direct2D Backend Initialization
 *
 * Initialize Flex with Direct2D rendering backend (Windows only).
 */

#pragma once

#ifdef _WIN32

#include <tlog.h>
#include <d2d1.h>
#include <wrl/client.h>

#pragma comment(lib, "d2d1.lib")

namespace flex {

// Global Direct2D factory (initialized once)
inline Microsoft::WRL::ComPtr<ID2D1Factory> g_d2d_factory;

// Initialize Flex with Direct2D backend
inline void init() {
 
    // Initialize Direct2D factory
    if (!g_d2d_factory) {
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, g_d2d_factory.GetAddressOf());
    }
}

inline void shutdown() {
    g_d2d_factory.Reset();
 }

} // namespace flex

#endif // _WIN32
