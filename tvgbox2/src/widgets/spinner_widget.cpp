/*
 * tvgbox2 - SpinnerWidget Implementation
 */

#include <tvgbox2/widgets/spinner_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/renderer.h>
#include <cmath>
#include <cstdio>

namespace tvgbox2 {

constexpr float PI = 3.14159265358979f;

SpinnerWidget::SpinnerWidget(Variant variant) : variant_(variant) {}

void SpinnerWidget::render(const Element& elem, Renderer& renderer) {
  if (!spinning_) return;

  auto& r = renderer.flex();
  switch (variant_) {
    case Variant::Ring: render_ring(r, elem); break;
    case Variant::Dots: render_dots(r, elem); break;
    case Variant::Bars: render_bars(r, elem); break;
  }
}

void SpinnerWidget::render_ring(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;

  Color color = {0.23f, 0.51f, 0.96f, 1.0f};  // Default: blue
  Color track = {0.90f, 0.91f, 0.92f, 1.0f};  // Default: light gray
  float stroke_width = 3.0f;

  if (style) {
    color = style->get_variable_color("--spinner-color", color);
    track = style->get_variable_color("--spinner-track", track);
    stroke_width = style->get_variable_float("--spinner-stroke", stroke_width);
  }

  float cx = elem.width() / 2;
  float cy = elem.height() / 2;
  float radius = std::min(cx, cy) - stroke_width;

  // Draw track (full circle)
  r.draw_circle(cx, cy, radius, Paint::none(), Paint::solid(track), stroke_width);

  // Draw arc (270 degrees) as path
  float start_angle = rotation_ * PI / 180.0f;
  float arc_length = 270.0f * PI / 180.0f;

  // Build arc path as series of small line segments
  char path[2048];
  int offset = 0;
  const int segments = 32;
  float step = arc_length / segments;

  for (int i = 0; i <= segments; i++) {
    float a = start_angle + i * step;
    float x = cx + radius * std::cos(a);
    float y = cy + radius * std::sin(a);

    if (i == 0) {
      offset += snprintf(path + offset, sizeof(path) - offset, "M %.4g %.4g", x, y);
    } else {
      offset += snprintf(path + offset, sizeof(path) - offset, " L %.4g %.4g", x, y);
    }
  }

  r.stroke_path(path, Paint::solid(color), stroke_width);
}

void SpinnerWidget::render_dots(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;

  Color color = {0.23f, 0.51f, 0.96f, 1.0f};
  if (style) {
    color = style->get_variable_color("--spinner-color", color);
  }

  float cx = elem.width() / 2;
  float cy = elem.height() / 2;
  float radius = std::min(cx, cy) * 0.6f;
  float dot_radius = radius * 0.2f;

  const int dot_count = 8;
  for (int i = 0; i < dot_count; i++) {
    float angle = (i * 360.0f / dot_count + rotation_) * PI / 180.0f;
    float x = cx + radius * std::cos(angle);
    float y = cy + radius * std::sin(angle);

    // Alpha based on position in animation
    float alpha_factor = (float(i) / dot_count);
    Color dot_color = color;
    dot_color.a = alpha_factor * color.a;

    r.draw_circle(x, y, dot_radius, Paint::solid(dot_color), Paint::none(), 0);
  }
}

void SpinnerWidget::render_bars(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;

  Color color = {0.23f, 0.51f, 0.96f, 1.0f};
  if (style) {
    color = style->get_variable_color("--spinner-color", color);
  }

  float bar_width = elem.width() / 5;
  float bar_height = elem.height();
  float spacing = bar_width * 0.5f;

  const int bar_count = 3;
  float total_width = bar_count * bar_width + (bar_count - 1) * spacing;
  float start_x = (elem.width() - total_width) / 2;

  for (int i = 0; i < bar_count; i++) {
    // Phase offset for each bar
    float phase_offset = i * (2 * PI / bar_count);
    float scale = 0.5f + 0.5f * std::sin(phase_ + phase_offset);

    float x = start_x + i * (bar_width + spacing);
    float h = bar_height * (0.3f + 0.7f * scale);
    float y = (elem.height() - h) / 2;

    r.draw_rect(x, y, bar_width, h, bar_width / 2, Paint::solid(color), Paint::none(), 0);
  }
}

bool SpinnerWidget::handle_event(const Event& event, Element& elem) {
  return false;
}

void SpinnerWidget::update(float delta_ms, Element& elem) {
  if (!spinning_) return;

  auto* style = elem.computed_style;
  float speed = 1000.0f;  // ms per revolution
  if (style) {
    speed = style->get_variable_float("--spinner-speed", speed);
  }

  rotation_ += (360.0f / speed) * delta_ms;
  if (rotation_ >= 360.0f) rotation_ -= 360.0f;

  phase_ += (2 * PI / speed) * delta_ms;
  if (phase_ >= 2 * PI) phase_ -= 2 * PI;

  elem.mark_paint_dirty();
}

} // namespace tvgbox2
