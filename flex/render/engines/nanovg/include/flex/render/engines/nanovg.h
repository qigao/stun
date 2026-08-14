/* Internal NanoVG engine used by the public OpenGL backend. */
#pragma once

#include "flex/core/renderer.h"

#include <memory>

namespace flex::render::engines::nanovg {

std::unique_ptr<Renderer> create_renderer(bool antialias,
                                          bool stencil_strokes,
                                          bool debug);

} // namespace flex::render::engines::nanovg
