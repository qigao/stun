/*
 * tvgbox2 - DividerWidget Implementation
 */

#include <tvgbox2/widgets/divider_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <thorvg.h>

namespace tvgbox2 {

DividerWidget::DividerWidget(Orientation orientation) : orientation_(orientation) {}

void DividerWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  auto* style = elem.computed_style;

  Color color = {200, 200, 200, 255};
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

  auto line = tvg::Shape::gen();

  if (orientation_ == Orientation::Horizontal) {
    float y = elem.height() / 2;

    if (label_.empty()) {
      // Simple line
      line->moveTo(0, y);
      line->lineTo(elem.width(), y);
    } else {
      // Line with gap for label
      float label_width = label_.size() * 8.0f;  // Approximate
      float gap = 8.0f;
      float center = elem.width() / 2;

      line->moveTo(0, y);
      line->lineTo(center - label_width / 2 - gap, y);
      line->moveTo(center + label_width / 2 + gap, y);
      line->lineTo(elem.width(), y);

      // Render label
      auto text_shape = tvg::Text::gen();
      text_shape->font(style ? style->font_family.c_str() : "Arial");
      text_shape->size(style ? style->font_size : 12.0f);
      text_shape->text(label_.c_str());
      text_shape->fill(color.r, color.g, color.b);
      text_shape->opacity(color.a);
      text_shape->translate(center - label_width / 2, y + 4);
      scene->push(std::move(text_shape));
    }
  } else {
    float x = elem.width() / 2;
    line->moveTo(x, 0);
    line->lineTo(x, elem.height());
  }

  line->strokeFill(color.r, color.g, color.b, color.a);
  line->strokeWidth(thickness);

  // Apply dash pattern
  if (style_ == Style::Dashed) {
    float dash[] = {6.0f, 4.0f};
    line->strokeDash(dash, 2);
  } else if (style_ == Style::Dotted) {
    float dash[] = {2.0f, 2.0f};
    line->strokeDash(dash, 2);
  }

  scene->push(std::move(line));
}

bool DividerWidget::handle_event(const Event& event, Element& elem) {
  return false;
}

void DividerWidget::update(float delta_ms, Element& elem) {
  // No animation
}

} // namespace tvgbox2
