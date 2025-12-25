/*
 * Flex Engine - Skia Renderer Factory
 *
 * Factory function to create Skia backend renderer.
 */

#pragma once

#include "flex/runtime/renderer.h"
#include <memory>

namespace flex {

// Factory function to create Skia renderer from SkCanvas*
// canvas = SkCanvas* cast to void*
std::unique_ptr<Renderer> create_skia_renderer(CanvasHandle canvas);

} // namespace flex
