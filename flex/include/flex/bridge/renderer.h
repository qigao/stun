/*
 * Flex Engine - ThorVG Renderer Factory
 *
 * Factory function to create ThorVG backend renderer.
 * This is in the bridge layer as it connects runtime (Renderer interface)
 * with specific backend implementation (ThorVG).
 */

#pragma once

#include "flex/runtime/renderer.h"
#include <memory>

namespace flex {

// Factory function to create ThorVG renderer from canvas handle
std::unique_ptr<Renderer> create_thorvg_renderer(CanvasHandle canvas);

} // namespace flex
