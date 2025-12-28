/*
 * flexUI - Renderer
 *
 * Wraps flex::Renderer for backend-agnostic rendering.
 * The actual backend (ThorVG, NanoVG, etc.) is determined by the
 * flex::Renderer implementation passed to Box.
 */

#ifndef FLEXUI_RENDERER_H
#define FLEXUI_RENDERER_H

#include "flex/runtime/renderer.h"

namespace flexUI {

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

  // Proxy methods for common drawing operations
  void save() { flex_renderer_->save(); }
  void restore() { flex_renderer_->restore(); }
  
  void translate(float x, float y) { flex_renderer_->translate(x, y); }
  void rotate(float degrees) { flex_renderer_->rotate(degrees); }
  void scale(float sx, float sy) { flex_renderer_->scale(sx, sy); }
  
  void set_global_alpha(float alpha) { flex_renderer_->set_global_alpha(alpha); }
  
  void draw_rect(float x, float y, float w, float h, float r,
                 const flex::Paint& fill, const flex::Paint& stroke, float stroke_width) {
    flex_renderer_->draw_rect(x, y, w, h, r, fill, stroke, stroke_width);
  }
  
  void draw_text(const std::string& text, float x, float y,
                 const std::string& font, float size, bool bold, const flex::Color& col) {
    flex_renderer_->draw_text(text, x, y, font, size, bold, col);
  }

  void clip_rect(float x, float y, float w, float h) {
    flex_renderer_->clip_rect(x, y, w, h);
  }

private:
  flex::Renderer* flex_renderer_;
};

} // namespace flexUI

#endif // FLEXUI_RENDERER_H
