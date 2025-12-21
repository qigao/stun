/*
 * tvgbox2 - TooltipWidget Implementation
 */

#include <tvgbox2/widgets/tooltip_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <thorvg.h>
#include <algorithm>

namespace tvgbox2 {

TooltipWidget::TooltipWidget(const std::string& text) : text_(text) {}

void TooltipWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  if (!visible_ || opacity_ <= 0) return;

  render_background(scene, elem);
  render_text(scene, elem);
}

void TooltipWidget::render_background(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;

  Color bg_color = {30, 30, 30, 230};  // Dark background
  float radius = 4.0f;

  if (style) {
    bg_color = style->get_variable_color("--tooltip-bg", bg_color);
    radius = style->border_radius[0];
  }

  // Apply opacity
  uint8_t alpha = static_cast<uint8_t>(bg_color.a * opacity_);

  auto bg = tvg::Shape::gen();
  bg->appendRect(0, 0, elem.width(), elem.height(), radius, radius);
  bg->fill(bg_color.r, bg_color.g, bg_color.b, alpha);

  scene->push(std::move(bg));

  // Arrow
  if (style && style->get_variable("--tooltip-arrow", "true") == "true") {
    render_arrow(scene, elem);
  }
}

void TooltipWidget::render_arrow(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;

  Color bg_color = {30, 30, 30, 230};
  if (style) {
    bg_color = style->get_variable_color("--tooltip-bg", bg_color);
  }

  uint8_t alpha = static_cast<uint8_t>(bg_color.a * opacity_);
  float arrow_size = 6.0f;

  auto arrow = tvg::Shape::gen();

  float cx = elem.width() / 2;
  float cy = elem.height() / 2;

  switch (position_) {
    case Position::Top:
      // Arrow pointing down at bottom
      arrow->moveTo(cx - arrow_size, elem.height());
      arrow->lineTo(cx, elem.height() + arrow_size);
      arrow->lineTo(cx + arrow_size, elem.height());
      arrow->close();
      break;
    case Position::Bottom:
      // Arrow pointing up at top
      arrow->moveTo(cx - arrow_size, 0);
      arrow->lineTo(cx, -arrow_size);
      arrow->lineTo(cx + arrow_size, 0);
      arrow->close();
      break;
    case Position::Left:
      // Arrow pointing right at right side
      arrow->moveTo(elem.width(), cy - arrow_size);
      arrow->lineTo(elem.width() + arrow_size, cy);
      arrow->lineTo(elem.width(), cy + arrow_size);
      arrow->close();
      break;
    case Position::Right:
      // Arrow pointing left at left side
      arrow->moveTo(0, cy - arrow_size);
      arrow->lineTo(-arrow_size, cy);
      arrow->lineTo(0, cy + arrow_size);
      arrow->close();
      break;
  }

  arrow->fill(bg_color.r, bg_color.g, bg_color.b, alpha);
  scene->push(std::move(arrow));
}

void TooltipWidget::render_text(tvg::Scene* scene, const Element& elem) {
  if (text_.empty()) return;

  auto* style = elem.computed_style;

  Color text_color = {255, 255, 255, 255};
  float font_size = 12.0f;
  std::string font_family = "Arial";

  if (style) {
    text_color = style->get_variable_color("--tooltip-text", text_color);
    font_size = style->font_size > 0 ? style->font_size : font_size;
    if (!style->font_family.empty()) font_family = style->font_family;
  }

  uint8_t alpha = static_cast<uint8_t>(text_color.a * opacity_);

  float padding = 8.0f;
  float text_y = elem.height() / 2 + font_size / 3;

  auto text_shape = tvg::Text::gen();
  text_shape->font(font_family.c_str());
  text_shape->size(font_size);
  text_shape->text(text_.c_str());
  text_shape->fill(text_color.r, text_color.g, text_color.b);
  text_shape->opacity(alpha);
  text_shape->translate(padding, text_y);

  scene->push(std::move(text_shape));
}

bool TooltipWidget::handle_event(const Event& event, Element& elem) {
  // Tooltip shows based on parent hover state, not its own events
  return false;
}

void TooltipWidget::update(float delta_ms, Element& elem) {
  auto* style = elem.computed_style;
  float delay = 500.0f;  // Default delay
  if (style) {
    delay = style->get_variable_float("--tooltip-delay", delay);
  }

  // Check parent hover state
  bool parent_hover = false;
  if (elem.parent) {
    parent_hover = elem.parent->has_state("hover");
  }

  if (parent_hover || target_visible_) {
    delay_timer_ += delta_ms;
    if (delay_timer_ >= delay) {
      visible_ = true;
      // Fade in
      opacity_ = std::min(1.0f, opacity_ + delta_ms / 150.0f);
      elem.mark_paint_dirty();
    }
  } else {
    delay_timer_ = 0;
    // Fade out
    if (opacity_ > 0) {
      opacity_ = std::max(0.0f, opacity_ - delta_ms / 100.0f);
      elem.mark_paint_dirty();
      if (opacity_ <= 0) {
        visible_ = false;
      }
    }
  }
}

} // namespace tvgbox2
