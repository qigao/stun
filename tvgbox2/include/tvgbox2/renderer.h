/*
 * tvgbox2 - Renderer
 *
 * Wraps flex::Renderer for backend-agnostic rendering.
 * The actual backend (ThorVG, NanoVG, etc.) is determined by the
 * flex::Renderer implementation passed to Box.
 */

#ifndef TVGBOX2_RENDERER_H
#define TVGBOX2_RENDERER_H

#include "flex/runtime/renderer.h"

namespace tvgbox2 {

// Re-export flex types for convenience
using flex::Paint;
using flex::Transform;
using flex::Bounds;

/**
 * Renderer - Wrapper around flex::Renderer
 *
 * Provides access to flex::Renderer for backend-agnostic rendering.
 * The backend is determined by the flex::Renderer implementation.
 */
class Renderer {
public:
  explicit Renderer(flex::Renderer* renderer)
    : flex_renderer_(renderer) {}

  // Access flex::Renderer
  flex::Renderer& flex() { return *flex_renderer_; }
  const flex::Renderer& flex() const { return *flex_renderer_; }

private:
  flex::Renderer* flex_renderer_;
};

} // namespace tvgbox2

#endif // TVGBOX2_RENDERER_H
