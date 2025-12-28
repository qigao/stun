/*
 * flexUI - TooltipWidget Implementation
 */

#include <flexUI/widgets/tooltip_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <algorithm>
#include <stb_sprintf.h>

namespace flexUI {

TooltipWidget::TooltipWidget(const std::string& text) : text_(text) {}

void TooltipWidget::render(const Element& elem, Renderer& renderer) {
  if (!visible_ || opacity_ <= 0) return;

  auto& r = renderer.flex();
  render_background(r, elem);
  render_text(r, elem);
}

void TooltipWidget::render_background(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;

  Color bg_color = {0.12f, 0.12f, 0.12f, 0.9f};  // Dark background
  float radius = 4.0f;

  if (style) {
    bg_color = style->get_variable_color("--tooltip-bg", bg_color);
    radius = style->border_radius[0];
  }

  // Apply opacity
  Color final_color = {bg_color.r, bg_color.g, bg_color.b, bg_color.a * opacity_};

  r.draw_rect(0, 0, elem.width(), elem.height(), radius,
              Paint::solid(final_color), Paint::none(), 0);

  // Arrow
  if (style && style->get_variable("--tooltip-arrow", "true") == "true") {
    render_arrow(r, elem);
  }
}

void TooltipWidget::render_arrow(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;

  Color bg_color = {0.12f, 0.12f, 0.12f, 0.9f};
  if (style) {
    bg_color = style->get_variable_color("--tooltip-bg", bg_color);
  }

  Color final_color = {bg_color.r, bg_color.g, bg_color.b, bg_color.a * opacity_};
  float arrow_size = 6.0f;

  float cx = elem.width() / 2;
  float cy = elem.height() / 2;

  char path[256];

  switch (position_) {
    case Position::Top:
      // Arrow pointing down at bottom
      stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g Z",
               cx - arrow_size, elem.height(),
               cx, elem.height() + arrow_size,
               cx + arrow_size, elem.height());
      break;
    case Position::Bottom:
      // Arrow pointing up at top
      stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g Z",
               cx - arrow_size, 0.0f,
               cx, -arrow_size,
               cx + arrow_size, 0.0f);
      break;
    case Position::Left:
      // Arrow pointing right at right side
      stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g Z",
               elem.width(), cy - arrow_size,
               elem.width() + arrow_size, cy,
               elem.width(), cy + arrow_size);
      break;
    case Position::Right:
      // Arrow pointing left at left side
      stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g Z",
               0.0f, cy - arrow_size,
               -arrow_size, cy,
               0.0f, cy + arrow_size);
      break;
  }

  r.fill_path(path, Paint::solid(final_color));
}

void TooltipWidget::render_text(flex::Renderer& r, const Element& elem) {
  if (text_.empty()) return;

  auto* style = elem.computed_style;

  Color text_color = {1.0f, 1.0f, 1.0f, 1.0f};
  float font_size = 12.0f;
  std::string font_family = "Arial";

  if (style) {
    text_color = style->get_variable_color("--tooltip-text", text_color);
    font_size = style->font_size > 0 ? style->font_size : font_size;
    if (!style->font_family.empty()) font_family = style->font_family;
  }

  Color final_color = {text_color.r, text_color.g, text_color.b, text_color.a * opacity_};

  float padding = 8.0f;
  float text_y = elem.height() / 2 - font_size / 2;

  r.draw_text(text_, padding, text_y, font_family, font_size, false, final_color);
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
  if (elem.parent_elem()) {
    parent_hover = elem.parent_elem()->has_state("hover");
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

} // namespace flexUI
