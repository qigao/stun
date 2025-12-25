/*
 * Flex Engine - Direct2D Renderer Factory
 *
 * Factory function to create Direct2D backend renderer (Windows only).
 */

#pragma once

#include "flex/runtime/renderer.h"
#include <memory>

namespace flex {

#ifdef _WIN32

// Factory function to create Direct2D renderer from ID2D1RenderTarget*
// canvas = ID2D1RenderTarget* cast to void*
std::unique_ptr<Renderer> create_d2d_renderer(CanvasHandle canvas);

#endif // _WIN32

} // namespace flex
