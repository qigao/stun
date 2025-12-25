/*
 * Flex Engine - NanoVG Renderer Factory
 *
 * Factory function to create NanoVG backend renderer.
 */

#pragma once

#include "flex/runtime/renderer.h"
#include <memory>

namespace flex {

// Factory function to create NanoVG renderer from NVGcontext*
// canvas = NVGcontext* cast to void*
std::unique_ptr<Renderer> create_nanovg_renderer(CanvasHandle canvas);

} // namespace flex
