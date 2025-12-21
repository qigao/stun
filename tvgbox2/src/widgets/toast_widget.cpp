/*
 * tvgbox2 - ToastWidget Implementation
 */

#include <tvgbox2/widgets/toast_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <thorvg.h>
#include <algorithm>
#include <cmath>

namespace tvgbox2 {

ToastWidget::ToastWidget(const std::string& message, Type type)
    : message_(message), type_(type) {}

void ToastWidget::show() {
  visible_ = true;
  timer_ = 0;
  opacity_ = 0;
  dirty_ = true;
}

void ToastWidget::hide() {
  visible_ = false;
  opacity_ = 0;
  if (on_dismiss_) on_dismiss_();
  dirty_ = true;
}

Color ToastWidget::get_type_color() const {
  switch (type_) {
    case Type::Success: return {34, 197, 94, 255};    // Green
    case Type::Error:   return {239, 68, 68, 255};    // Red
    case Type::Warning: return {234, 179, 8, 255};    // Yellow
    case Type::Info:    return {59, 130, 246, 255};   // Blue
    default:            return {50, 50, 50, 240};     // Dark gray
  }
}

void ToastWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  if (!visible_ || opacity_ <= 0) return;

  render_background(scene, elem);
  render_icon(scene, elem);
  render_text(scene, elem);
  render_close_button(scene, elem);
}

void ToastWidget::render_background(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;

  Color bg_color = get_type_color();
  float radius = 8.0f;

  if (style) {
    bg_color = style->get_variable_color("--toast-bg", bg_color);
    radius = style->border_radius[0];
  }

  uint8_t alpha = static_cast<uint8_t>(bg_color.a * opacity_);

  auto bg = tvg::Shape::gen();
  bg->appendRect(0, 0, elem.width(), elem.height(), radius, radius);
  bg->fill(bg_color.r, bg_color.g, bg_color.b, alpha);

  // Shadow
  auto shadow = tvg::Shape::gen();
  shadow->appendRect(2, 2, elem.width(), elem.height(), radius, radius);
  shadow->fill(0, 0, 0, static_cast<uint8_t>(30 * opacity_));
  scene->push(std::move(shadow));

  scene->push(std::move(bg));
}

void ToastWidget::render_icon(tvg::Scene* scene, const Element& elem) {
  float icon_size = 20.0f;
  float padding = 12.0f;
  float cx = padding + icon_size / 2;
  float cy = elem.height() / 2;

  uint8_t alpha = static_cast<uint8_t>(255 * opacity_);

  auto icon = tvg::Shape::gen();

  switch (type_) {
    case Type::Success:
      // Checkmark
      icon->moveTo(cx - 6, cy);
      icon->lineTo(cx - 2, cy + 4);
      icon->lineTo(cx + 6, cy - 4);
      icon->strokeFill(255, 255, 255, alpha);
      icon->strokeWidth(2);
      icon->strokeCap(tvg::StrokeCap::Round);
      break;

    case Type::Error:
      // X mark
      icon->moveTo(cx - 5, cy - 5);
      icon->lineTo(cx + 5, cy + 5);
      icon->moveTo(cx + 5, cy - 5);
      icon->lineTo(cx - 5, cy + 5);
      icon->strokeFill(255, 255, 255, alpha);
      icon->strokeWidth(2);
      icon->strokeCap(tvg::StrokeCap::Round);
      break;

    case Type::Warning:
      // Triangle with !
      icon->moveTo(cx, cy - 7);
      icon->lineTo(cx + 8, cy + 6);
      icon->lineTo(cx - 8, cy + 6);
      icon->close();
      icon->strokeFill(255, 255, 255, alpha);
      icon->strokeWidth(1.5f);
      break;

    case Type::Info:
      // i in circle
      icon->appendCircle(cx, cy, 8, 8);
      icon->strokeFill(255, 255, 255, alpha);
      icon->strokeWidth(1.5f);
      break;

    default:
      return;
  }

  scene->push(std::move(icon));
}

void ToastWidget::render_text(tvg::Scene* scene, const Element& elem) {
  if (message_.empty()) return;

  auto* style = elem.computed_style;

  Color text_color = {255, 255, 255, 255};
  float font_size = 14.0f;
  std::string font_family = "Arial";

  if (style) {
    text_color = style->get_variable_color("--toast-text", text_color);
    font_size = style->font_size > 0 ? style->font_size : font_size;
    if (!style->font_family.empty()) font_family = style->font_family;
  }

  uint8_t alpha = static_cast<uint8_t>(text_color.a * opacity_);

  float icon_space = 44.0f;  // Space for icon
  float text_x = icon_space;
  float text_y = elem.height() / 2 + font_size / 3;

  auto text_shape = tvg::Text::gen();
  text_shape->font(font_family.c_str());
  text_shape->size(font_size);
  text_shape->text(message_.c_str());
  text_shape->fill(text_color.r, text_color.g, text_color.b);
  text_shape->opacity(alpha);
  text_shape->translate(text_x, text_y);

  scene->push(std::move(text_shape));
}

void ToastWidget::render_close_button(tvg::Scene* scene, const Element& elem) {
  float btn_size = 20.0f;
  float padding = 8.0f;
  float cx = elem.width() - padding - btn_size / 2;
  float cy = elem.height() / 2;

  uint8_t alpha = static_cast<uint8_t>(200 * opacity_);

  auto close = tvg::Shape::gen();
  close->moveTo(cx - 4, cy - 4);
  close->lineTo(cx + 4, cy + 4);
  close->moveTo(cx + 4, cy - 4);
  close->lineTo(cx - 4, cy + 4);
  close->strokeFill(255, 255, 255, alpha);
  close->strokeWidth(1.5f);
  close->strokeCap(tvg::StrokeCap::Round);

  scene->push(std::move(close));
}

bool ToastWidget::handle_event(const Event& event, Element& elem) {
  if (!visible_) return false;

  if (event.type == EventType::MouseDown) {
    // Check if click is on close button
    float btn_size = 24.0f;
    float padding = 8.0f;
    float btn_x = elem.width() - padding - btn_size;

    float local_x = event.x - elem.absolute_x();
    if (local_x >= btn_x) {
      hide();
      return true;
    }
  }
  return false;
}

void ToastWidget::update(float delta_ms, Element& elem) {
  if (!visible_) return;

  auto* style = elem.computed_style;
  float duration = duration_;
  if (style) {
    duration = style->get_variable_float("--toast-duration", duration);
  }

  // Fade in
  if (opacity_ < 1.0f) {
    opacity_ = std::min(1.0f, opacity_ + delta_ms / 200.0f);
    elem.mark_paint_dirty();
  }

  // Auto-dismiss
  if (duration > 0) {
    timer_ += delta_ms;
    if (timer_ >= duration) {
      // Fade out
      opacity_ = std::max(0.0f, opacity_ - delta_ms / 200.0f);
      elem.mark_paint_dirty();
      if (opacity_ <= 0) {
        hide();
      }
    }
  }
}

} // namespace tvgbox2
