/*
 * flexUI - Renderer
 *
 * Adapts backend-neutral commands to the runtime renderer.
 */

#ifndef FLEXUI_RENDERER_H
#define FLEXUI_RENDERER_H

#include "flex/runtime/renderer.h"
#include <string>

namespace flexUI {

class RenderCommandList;
class RenderCommandCache;

// Re-export flex types for convenience
using flex::Paint;
using flex::Transform;
using flex::Bounds;

/**
 * Renderer - backend adapter
 *
 * The backend is determined by the flex::Renderer implementation passed to Box.
 * Callers emit RenderCommandList instances; replay is the only drawing path.
 */
class Renderer {
public:
  explicit Renderer(flex::Renderer* renderer)
    : flex_renderer_(renderer) {}

  flex::Bounds viewport() const { return flex_renderer_->viewport(); }
  flex::RendererCapabilities capabilities() const { return flex_renderer_->capabilities(); }
  bool requires_surface_recreation() const { return flex_renderer_->requires_surface_recreation(); }

  bool supports_rotation() const { return capabilities().rotation; }
  bool supports_scaling() const { return capabilities().scaling; }
  bool supports_raster_images() const { return capabilities().raster_images; }
  bool supports_svg_images() const { return capabilities().svg_images; }
  bool supports_shadow() const { return capabilities().shadow; }

  bool measure_text(const std::string& text, const std::string& font,
                    float size, bool bold, flex::TextMetrics& out_metrics) {
    return flex_renderer_->measure_text(
        text, font.empty() ? "Arial" : font, size, bold, out_metrics);
  }

  bool register_font(const std::string& family, const std::string& path) {
    return flex_renderer_->register_font(family, path);
  }
  void unregister_font(const std::string& family) {
    flex_renderer_->unregister_font(family);
  }

private:
  friend class RenderCommandList;
  friend class RenderCommandCache;

  void begin_frame(float width, float height, float pixel_ratio) {
    flex_renderer_->begin_frame(width, height, pixel_ratio);
  }

  void end_frame() { flex_renderer_->end_frame(); }
  void clear(const flex::Color& color) { flex_renderer_->clear(color); }
  void set_retained_mode(bool enabled) { flex_renderer_->set_retained_mode(enabled); }
  bool supports_retained_mode() const { return flex_renderer_->supports_retained_mode(); }
  void set_retained_insertion_anchor(flex::PaintHandle before) {
    flex_renderer_->set_retained_insertion_anchor(before);
  }

  void save() { flex_renderer_->save(); }
  void restore() { flex_renderer_->restore(); }
  void set_transform(const flex::Transform& transform) {
    flex_renderer_->set_transform(transform);
  }
  
  void translate(float x, float y) { flex_renderer_->translate(x, y); }
  void rotate(float degrees) { flex_renderer_->rotate(degrees); }
  void scale(float sx, float sy) { flex_renderer_->scale(sx, sy); }
  
  void set_global_alpha(float alpha) { flex_renderer_->set_global_alpha(alpha); }
  void set_shadow(const flex::Shadow& shadow) { flex_renderer_->set_shadow(shadow); }
  void clear_shadow() { flex_renderer_->clear_shadow(); }
  void set_blur(const flex::BlurFilter& blur) { flex_renderer_->set_blur(blur); }
  void clear_blur() { flex_renderer_->clear_blur(); }

  void fill_path(const std::string& d, const flex::Paint& paint) {
    flex_renderer_->fill_path(d, paint);
  }

  void stroke_path(const std::string& d, const flex::Paint& paint, float width) {
    flex_renderer_->stroke_path(d, paint, width);
  }
  
  void draw_rect(float x, float y, float w, float h, float r,
                 const flex::Paint& fill, const flex::Paint& stroke, float stroke_width) {
    flex_renderer_->draw_rect(x, y, w, h, r, fill, stroke, stroke_width);
  }

  void draw_line(float x1, float y1, float x2, float y2,
                 const flex::Paint& paint, float width) {
    flex_renderer_->draw_line(x1, y1, x2, y2, paint, width);
  }

  void draw_circle(float cx, float cy, float radius,
                   const flex::Paint& fill, const flex::Paint& stroke,
                   float stroke_width) {
    flex_renderer_->draw_circle(cx, cy, radius, fill, stroke, stroke_width);
  }

  void draw_ellipse(float cx, float cy, float rx, float ry,
                    const flex::Paint& fill, const flex::Paint& stroke,
                    float stroke_width) {
    flex_renderer_->draw_ellipse(cx, cy, rx, ry, fill, stroke, stroke_width);
  }
  
  void draw_text(const std::string& text, float x, float y,
                 const std::string& font, float size, bool bold, const flex::Color& col) {
    flex_renderer_->draw_text(text, x, y, font.empty() ? "Arial" : font, size, bold, col);
  }

  void draw_image(const std::string& src, float x, float y, float w, float h) {
    flex_renderer_->draw_image(src, x, y, w, h);
  }

  void draw_svg(const std::string& src, float x, float y, float w, float h) {
    flex_renderer_->draw_svg(src, x, y, w, h);
  }

  void draw_svg_data(const std::string& data, float x, float y, float w,
                     float h) {
    flex_renderer_->draw_svg_data(data, x, y, w, h);
  }

  void clip_rect(float x, float y, float w, float h) {
    flex_renderer_->clip_rect(x, y, w, h);
  }

  void reset_clip() { flex_renderer_->reset_clip(); }

  void remove_cached(flex::PaintHandle paint) {
    flex_renderer_->remove_cached(paint);
  }

  flex::PaintHandle push_rect(float x, float y, float w, float h, float r,
                              const flex::Paint& fill,
                              const flex::Paint& stroke, float stroke_width,
                              const flex::Transform& transform, float alpha) {
    return flex_renderer_->push_rect(x, y, w, h, r, fill, stroke,
                                     stroke_width, transform, alpha);
  }

  flex::PaintHandle push_circle(float cx, float cy, float r,
                                const flex::Paint& fill,
                                const flex::Paint& stroke, float stroke_width,
                                const flex::Transform& transform, float alpha) {
    return flex_renderer_->push_circle(cx, cy, r, fill, stroke, stroke_width,
                                       transform, alpha);
  }

  flex::PaintHandle push_ellipse(float cx, float cy, float rx, float ry,
                                 const flex::Paint& fill,
                                 const flex::Paint& stroke, float stroke_width,
                                 const flex::Transform& transform, float alpha) {
    return flex_renderer_->push_ellipse(cx, cy, rx, ry, fill, stroke,
                                        stroke_width, transform, alpha);
  }

  flex::PaintHandle push_path(const std::string& d, const flex::Paint& fill,
                              const flex::Paint& stroke, float stroke_width,
                              const flex::Transform& transform, float alpha) {
    return flex_renderer_->push_path(d, fill, stroke, stroke_width, transform,
                                     alpha);
  }

  flex::PaintHandle push_text(const std::string& text,
                              const std::string& font, float size, bool bold,
                              const flex::Color& color,
                              const flex::Transform& transform, float alpha) {
    return flex_renderer_->push_text(text, font.empty() ? "Arial" : font, size,
                                     bold, color, transform, alpha);
  }

  flex::PaintHandle push_image(const std::string& src, float w, float h,
                               const flex::Transform& transform, float alpha) {
    return flex_renderer_->push_image(src, w, h, transform, alpha);
  }

  flex::PaintHandle push_svg(const std::string& src, float w, float h,
                             const flex::Transform& transform, float alpha) {
    return flex_renderer_->push_svg(src, w, h, transform, alpha);
  }

  flex::PaintHandle push_svg_data(const std::string& data, float w, float h,
                                  const flex::Transform& transform,
                                  float alpha) {
    return flex_renderer_->push_svg_data(data, w, h, transform, alpha);
  }

  void update_transform(flex::PaintHandle paint,
                        const flex::Transform& transform) {
    flex_renderer_->update_transform(paint, transform);
  }

private:
  flex::Renderer* flex_renderer_;
};

} // namespace flexUI

#endif // FLEXUI_RENDERER_H
