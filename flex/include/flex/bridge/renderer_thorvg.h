/*
 * Flex Engine - ThorVG Renderer Factory
 */

#pragma once

#include "flex/core/renderer.h"

#include <memory>

namespace flex {

std::unique_ptr<Renderer> create_thorvg_renderer(CanvasHandle canvas);

} // namespace flex
