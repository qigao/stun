/*
 * tvgbox2 - SpinnerWidget Implementation
 */

#include <tvgbox2/widgets/spinner_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <thorvg.h>
#include <cmath>

namespace tvgbox2 {

constexpr float PI = 3.14159265358979f;

SpinnerWidget::SpinnerWidget(Variant variant) : variant_(variant) {}

void SpinnerWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  if (!spinning_) return;

  switch (variant_) {
    case Variant::Ring: render_ring(scene, elem); break;
    case Variant::Dots: render_dots(scene, elem); break;
    case Variant::Bars: render_bars(scene, elem); break;
  }
}

void SpinnerWidget::render_ring(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;

  Color color = {59, 130, 246, 255};  // Default: blue
  Color track = {229, 231, 235, 255}; // Default: light gray
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
  auto track_shape = tvg::Shape::gen();
  track_shape->appendCircle(cx, cy, radius, radius);
  track_shape->strokeFill(track.r, track.g, track.b, track.a);
  track_shape->strokeWidth(stroke_width);
  scene->push(std::move(track_shape));

  // Draw arc (270 degrees)
  auto arc = tvg::Shape::gen();

  // Create arc using bezier approximation
  float start_angle = rotation_ * PI / 180.0f;
  float arc_length = 270.0f * PI / 180.0f;

  // Draw arc as series of small segments
  const int segments = 32;
  float step = arc_length / segments;
  for (int i = 0; i < segments; i++) {
    float a1 = start_angle + i * step;
    float a2 = start_angle + (i + 1) * step;
    float x1 = cx + radius * std::cos(a1);
    float y1 = cy + radius * std::sin(a1);
    float x2 = cx + radius * std::cos(a2);
    float y2 = cy + radius * std::sin(a2);

    if (i == 0) {
      arc->moveTo(x1, y1);
    }
    arc->lineTo(x2, y2);
  }

  arc->strokeFill(color.r, color.g, color.b, color.a);
  arc->strokeWidth(stroke_width);
  arc->strokeCap(tvg::StrokeCap::Round);
  scene->push(std::move(arc));
}

void SpinnerWidget::render_dots(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;

  Color color = {59, 130, 246, 255};
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
    uint8_t alpha = static_cast<uint8_t>(alpha_factor * color.a);

    auto dot = tvg::Shape::gen();
    dot->appendCircle(x, y, dot_radius, dot_radius);
    dot->fill(color.r, color.g, color.b, alpha);
    scene->push(std::move(dot));
  }
}

void SpinnerWidget::render_bars(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;

  Color color = {59, 130, 246, 255};
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

    auto bar = tvg::Shape::gen();
    bar->appendRect(x, y, bar_width, h, bar_width / 2, bar_width / 2);
    bar->fill(color.r, color.g, color.b, color.a);
    scene->push(std::move(bar));
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
