/*
 * tvgbox2 - BadgeWidget Implementation
 */

#include <tvgbox2/widgets/badge_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <thorvg.h>
#include <algorithm>

namespace tvgbox2 {

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

void BadgeWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  if (!visible_) return;

  render_background(scene, elem);
  if (!dot_) {
    render_text(scene, elem);
  }
}

void BadgeWidget::render_background(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;

  // Badge background color
  Color bg_color = {239, 68, 68, 255};  // Default: red
  if (style) {
    bg_color = style->get_variable_color("--badge-bg", bg_color);
  }

  float w = elem.width();
  float h = elem.height();

  // Badge is always pill-shaped (full radius)
  float radius = std::min(w, h) / 2;

  auto bg = tvg::Shape::gen();
  bg->appendRect(0, 0, w, h, radius, radius);
  bg->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);

  scene->push(std::move(bg));
}

void BadgeWidget::render_text(tvg::Scene* scene, const Element& elem) {
  if (text_.empty()) return;

  auto* style = elem.computed_style;

  Color text_color = {255, 255, 255, 255};  // Default: white
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
  float text_y = elem.height() / 2 + font_size / 3;

  auto text_shape = tvg::Text::gen();
  text_shape->font(font_family.c_str());
  text_shape->size(font_size);
  text_shape->text(text_.c_str());
  text_shape->fill(text_color.r, text_color.g, text_color.b);
  text_shape->opacity(text_color.a);
  text_shape->translate(text_x, text_y);

  scene->push(std::move(text_shape));
}

bool BadgeWidget::handle_event(const Event& event, Element& elem) {
  return false;  // Badges don't handle events
}

void BadgeWidget::update(float delta_ms, Element& elem) {
  // No animation
}

} // namespace tvgbox2
