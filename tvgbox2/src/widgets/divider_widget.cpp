/*
 * tvgbox2 - DividerWidget Implementation
 */

#include <tvgbox2/widgets/divider_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/renderer.h>

namespace tvgbox2 {

DividerWidget::DividerWidget(Orientation orientation) : orientation_(orientation) {}

void DividerWidget::render(const Element& elem, Renderer& renderer) {
  auto* style = elem.computed_style;
  auto& r = renderer.flex();

  Color color = {0.78f, 0.78f, 0.78f, 1.0f};  // 200/255
  float thickness = 1.0f;

  if (style) {
    color = style->get_variable_color("--divider-color", color);
    thickness = style->get_variable_float("--divider-thickness", thickness);

    std::string orient_str = style->get_variable("--divider-orientation", "");
    if (orient_str == "vertical") orientation_ = Orientation::Vertical;
    else if (orient_str == "horizontal") orientation_ = Orientation::Horizontal;

    std::string style_str = style->get_variable("--divider-style", "solid");
    if (style_str == "dashed") style_ = Style::Dashed;
    else if (style_str == "dotted") style_ = Style::Dotted;
    else style_ = Style::Solid;
  }

  Paint stroke = Paint::solid(color);

  if (orientation_ == Orientation::Horizontal) {
    float y = elem.height() / 2;

    if (label_.empty()) {
      // Simple horizontal line
      char path[128];
      snprintf(path, sizeof(path), "M 0 %.4g L %.4g %.4g", y, elem.width(), y);
      r.stroke_path(path, stroke, thickness);
    } else {
      // Line with gap for label
      float label_width = label_.size() * 8.0f;  // Approximate
      float gap = 8.0f;
      float center = elem.width() / 2;

      // Left segment
      char path1[128];
      snprintf(path1, sizeof(path1), "M 0 %.4g L %.4g %.4g",
               y, center - label_width / 2 - gap, y);
      r.stroke_path(path1, stroke, thickness);

      // Right segment
      char path2[128];
      snprintf(path2, sizeof(path2), "M %.4g %.4g L %.4g %.4g",
               center + label_width / 2 + gap, y, elem.width(), y);
      r.stroke_path(path2, stroke, thickness);

      // Render label
      float font_size = style ? style->font_size : 12.0f;
      std::string font_family = style ? style->font_family : "Arial";
      r.draw_text(label_, center - label_width / 2, y + 4, font_family, font_size, false, color);
    }
  } else {
    // Vertical line
    float x = elem.width() / 2;
    char path[128];
    snprintf(path, sizeof(path), "M %.4g 0 L %.4g %.4g", x, x, elem.height());
    r.stroke_path(path, stroke, thickness);
  }

  // Note: dash pattern would require custom stroke_path extension
  // For now, solid lines only. Dashed/dotted styles could be implemented
  // by adding dash support to flex::Renderer::stroke_path
}

bool DividerWidget::handle_event(const Event& event, Element& elem) {
  return false;
}

void DividerWidget::update(float delta_ms, Element& elem) {
  // No animation
}

} // namespace tvgbox2
