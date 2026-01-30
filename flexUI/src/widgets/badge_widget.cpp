/*
 * flexUI - BadgeWidget Implementation
 */
#include <flexUI/widgets/badge_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <algorithm>

namespace flexUI {

BadgeWidget::BadgeWidget(const std::string& text) : text_(text) {}

void BadgeWidget::set_count(int count) {
  count_ = count;
  if (count > 99) {
    text_ = "99+";
  } else if (count > 0) {
    text_ = std::to_string(count);
  } else {
    text_ = "";
  }
  dirty_ = true;
}

void BadgeWidget::render(const Element& elem, Renderer& renderer) {
  if (!visible_) return;

  auto& r = renderer.flex();
  render_background(r, elem);
  if (!dot_) {
    render_text(r, elem);
  }
}

void BadgeWidget::render_background(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;

  // Badge background color (normalized 0-1)
  Color bg_color = {0.94f, 0.27f, 0.27f, 1.0f};  // Default: red (239/255, 68/255, 68/255)
  if (style) {
    bg_color = style->get_variable_color("--badge-bg", bg_color);
  }

  float w = elem.width();
  float h = elem.height();

  // Badge is always pill-shaped (full radius)
  float radius = std::min(w, h) / 2;

  r.draw_rect(0, 0, w, h, radius, Paint::solid(bg_color), Paint::none(), 0);
}

void BadgeWidget::render_text(flex::Renderer& r, const Element& elem) {
  if (text_.empty()) return;

  auto* style = elem.computed_style;

  Color text_color = {1.0f, 1.0f, 1.0f, 1.0f};  // Default: white
  float font_size = 12.0f;
  std::string font_family = "Arial";

  if (style) {
    text_color = style->get_variable_color("--badge-text", text_color);
    font_size = style->font_size > 0 ? style->font_size : font_size;
    if (!style->font_family.empty()) font_family = style->font_family;
  }

  // Center text
  float text_width = text_.size() * font_size * 0.6f;
  float text_x = (elem.width() - text_width) / 2;
  float text_y = elem.height() / 2 - font_size / 2;

  r.draw_text(text_, text_x, text_y, font_family, font_size, false, text_color);
}

bool BadgeWidget::handle_event(const Event& event, Element& elem) {
  return false;  // Badges don't handle events
}

void BadgeWidget::update(float delta_ms, Element& elem) {
  // No animation
}

} // namespace flexUI
