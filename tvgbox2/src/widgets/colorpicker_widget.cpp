/*
 * tvgbox2 - ColorPickerWidget Implementation
 */

#include <tvgbox2/widgets/colorpicker_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/renderer.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tvgbox2 {

ColorPickerWidget::ColorPickerWidget() {
  hsv_to_rgb(hue_, sat_, val_, color_.r, color_.g, color_.b);
}

void ColorPickerWidget::set_color(const Color& c) {
  color_ = c;
  rgb_to_hsv(c.r, c.g, c.b, hue_, sat_, val_);
  dirty_ = true;
}

void ColorPickerWidget::set_hsv(float h, float s, float v) {
  hue_ = std::clamp(h, 0.0f, 360.0f);
  sat_ = std::clamp(s, 0.0f, 1.0f);
  val_ = std::clamp(v, 0.0f, 1.0f);
  hsv_to_rgb(hue_, sat_, val_, color_.r, color_.g, color_.b);
  dirty_ = true;
}

void ColorPickerWidget::hsv_to_rgb(float h, float s, float v, float& r, float& g, float& b) {
  float c = v * s;
  float x = c * (1 - std::abs(std::fmod(h / 60.0f, 2.0f) - 1));
  float m = v - c;

  if (h < 60)       { r = c + m; g = x + m; b = m; }
  else if (h < 120) { r = x + m; g = c + m; b = m; }
  else if (h < 180) { r = m;     g = c + m; b = x + m; }
  else if (h < 240) { r = m;     g = x + m; b = c + m; }
  else if (h < 300) { r = x + m; g = m;     b = c + m; }
  else              { r = c + m; g = m;     b = x + m; }
}

void ColorPickerWidget::rgb_to_hsv(float r, float g, float b, float& h, float& s, float& v) {
  float cmax = std::max({r, g, b});
  float cmin = std::min({r, g, b});
  float delta = cmax - cmin;

  if (delta == 0) h = 0;
  else if (cmax == r) h = 60 * std::fmod((g - b) / delta + 6, 6.0f);
  else if (cmax == g) h = 60 * ((b - r) / delta + 2);
  else h = 60 * ((r - g) / delta + 4);

  s = (cmax == 0) ? 0 : delta / cmax;
  v = cmax;
}

void ColorPickerWidget::render(const Element& elem, Renderer& renderer) {
  auto& r = renderer.flex();
  auto* style = elem.computed_style;
  Color bg_color{1.0f, 1.0f, 1.0f, 1.0f};
  if (style) bg_color = style->get_variable_color("--picker-bg", bg_color);

  r.draw_rect(0, 0, elem.width(), elem.height(), 8, Paint::solid(bg_color), Paint::none(), 0);

  render_gradient(r, elem);
  render_hue_bar(r, elem);
  render_preview(r, elem);
  render_cursor(r, elem);
}

void ColorPickerWidget::render_gradient(flex::Renderer& r, const Element& elem) {
  float padding = 12.0f;
  float hue_bar_w = 24.0f;
  float preview_h = 40.0f;
  float grad_w = elem.width() - padding * 3 - hue_bar_w;
  float grad_h = elem.height() - padding * 2 - preview_h - 8;

  float hr, hg, hb;
  hsv_to_rgb(hue_, 1.0f, 1.0f, hr, hg, hb);

  // Simulate gradient with grid of colored rectangles
  // HSV gradient: x = saturation (0 to 1), y = value (1 to 0)
  const int cols = 16;
  const int rows = 16;
  float cell_w = grad_w / cols;
  float cell_h = grad_h / rows;

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      float s = (col + 0.5f) / cols;  // saturation
      float v = 1.0f - (row + 0.5f) / rows;  // value (top = 1, bottom = 0)

      float cr, cg, cb;
      hsv_to_rgb(hue_, s, v, cr, cg, cb);

      float x = padding + col * cell_w;
      float y = padding + row * cell_h;

      r.draw_rect(x, y, cell_w + 0.5f, cell_h + 0.5f, 0,
                  Paint::solid(Color{cr, cg, cb, 1.0f}), Paint::none(), 0);
    }
  }

  // Border around gradient area
  r.draw_rect(padding, padding, grad_w, grad_h, 4,
              Paint::none(), Paint::solid(Color{0.78f, 0.78f, 0.78f, 1.0f}), 1);
}

void ColorPickerWidget::render_hue_bar(flex::Renderer& r, const Element& elem) {
  float padding = 12.0f;
  float hue_bar_w = 24.0f;
  float preview_h = 40.0f;
  float bar_h = elem.height() - padding * 2 - preview_h - 8;
  float bar_x = elem.width() - padding - hue_bar_w;

  // Simulate hue gradient with segments
  const int segments = 36;  // 10 degrees each
  float seg_h = bar_h / segments;

  for (int i = 0; i < segments; i++) {
    float hue = i * (360.0f / segments);
    float cr, cg, cb;
    hsv_to_rgb(hue, 1.0f, 1.0f, cr, cg, cb);

    float y = padding + i * seg_h;
    r.draw_rect(bar_x, y, hue_bar_w, seg_h + 0.5f, 0,
                Paint::solid(Color{cr, cg, cb, 1.0f}), Paint::none(), 0);
  }

  // Border around hue bar
  r.draw_rect(bar_x, padding, hue_bar_w, bar_h, 2,
              Paint::none(), Paint::solid(Color{0.78f, 0.78f, 0.78f, 1.0f}), 1);

  // Hue indicator
  float indicator_y = padding + (hue_ / 360.0f) * bar_h;
  r.draw_rect(bar_x - 2, indicator_y - 3, hue_bar_w + 4, 6, 2,
              Paint::none(), Paint::solid(Color{1.0f, 1.0f, 1.0f, 1.0f}), 2);
}

void ColorPickerWidget::render_preview(flex::Renderer& r, const Element& elem) {
  float padding = 12.0f;
  float preview_h = 40.0f;
  float preview_y = elem.height() - padding - preview_h;
  float preview_w = elem.width() - padding * 2;

  // Preview rectangle with current color
  r.draw_rect(padding, preview_y, preview_w, preview_h, 4,
              Paint::solid(color_), Paint::none(), 0);

  // Border
  r.draw_rect(padding, preview_y, preview_w, preview_h, 4,
              Paint::none(), Paint::solid(Color{0.78f, 0.78f, 0.78f, 1.0f}), 1);
}

void ColorPickerWidget::render_cursor(flex::Renderer& r, const Element& elem) {
  float padding = 12.0f;
  float hue_bar_w = 24.0f;
  float preview_h = 40.0f;
  float grad_w = elem.width() - padding * 3 - hue_bar_w;
  float grad_h = elem.height() - padding * 2 - preview_h - 8;

  float cx = padding + sat_ * grad_w;
  float cy = padding + (1 - val_) * grad_h;

  // Outer circle (white)
  r.draw_circle(cx, cy, 8, Paint::none(), Paint::solid(Color{1.0f, 1.0f, 1.0f, 1.0f}), 2);

  // Inner circle (black)
  r.draw_circle(cx, cy, 6, Paint::none(), Paint::solid(Color{0.0f, 0.0f, 0.0f, 1.0f}), 1);
}

bool ColorPickerWidget::handle_event(const Event& event, Element& elem) {
  float padding = 12.0f;
  float hue_bar_w = 24.0f;
  float preview_h = 40.0f;
  float grad_w = elem.width() - padding * 3 - hue_bar_w;
  float grad_h = elem.height() - padding * 2 - preview_h - 8;
  float bar_x = elem.width() - padding - hue_bar_w;

  float local_x = event.x - elem.absolute_x();
  float local_y = event.y - elem.absolute_y();

  if (event.type == EventType::MouseDown) {
    if (local_x >= bar_x && local_x <= bar_x + hue_bar_w &&
        local_y >= padding && local_y <= padding + grad_h) {
      dragging_hue_ = true;
      hue_ = std::clamp((local_y - padding) / grad_h * 360.0f, 0.0f, 360.0f);
      hsv_to_rgb(hue_, sat_, val_, color_.r, color_.g, color_.b);
      if (on_change_) on_change_(color_);
      elem.mark_paint_dirty();
      return true;
    }

    if (local_x >= padding && local_x <= padding + grad_w &&
        local_y >= padding && local_y <= padding + grad_h) {
      dragging_gradient_ = true;
      sat_ = std::clamp((local_x - padding) / grad_w, 0.0f, 1.0f);
      val_ = std::clamp(1.0f - (local_y - padding) / grad_h, 0.0f, 1.0f);
      hsv_to_rgb(hue_, sat_, val_, color_.r, color_.g, color_.b);
      if (on_change_) on_change_(color_);
      elem.mark_paint_dirty();
      return true;
    }
  }

  if (event.type == EventType::MouseMove) {
    if (dragging_hue_) {
      hue_ = std::clamp((local_y - padding) / grad_h * 360.0f, 0.0f, 360.0f);
      hsv_to_rgb(hue_, sat_, val_, color_.r, color_.g, color_.b);
      if (on_change_) on_change_(color_);
      elem.mark_paint_dirty();
      return true;
    }
    if (dragging_gradient_) {
      sat_ = std::clamp((local_x - padding) / grad_w, 0.0f, 1.0f);
      val_ = std::clamp(1.0f - (local_y - padding) / grad_h, 0.0f, 1.0f);
      hsv_to_rgb(hue_, sat_, val_, color_.r, color_.g, color_.b);
      if (on_change_) on_change_(color_);
      elem.mark_paint_dirty();
      return true;
    }
  }

  if (event.type == EventType::MouseUp) {
    dragging_hue_ = false;
    dragging_gradient_ = false;
  }

  return false;
}

void ColorPickerWidget::update(float delta_ms, Element& elem) {}

} // namespace tvgbox2
