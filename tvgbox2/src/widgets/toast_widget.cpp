/*
 * tvgbox2 - ToastWidget Implementation
 */

#include <tvgbox2/widgets/toast_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/renderer.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

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
    case Type::Success: return {0.13f, 0.77f, 0.37f, 1.0f};    // Green
    case Type::Error:   return {0.94f, 0.27f, 0.27f, 1.0f};    // Red
    case Type::Warning: return {0.92f, 0.70f, 0.03f, 1.0f};    // Yellow
    case Type::Info:    return {0.23f, 0.51f, 0.96f, 1.0f};    // Blue
    default:            return {0.20f, 0.20f, 0.20f, 0.94f};   // Dark gray
  }
}

void ToastWidget::render(const Element& elem, Renderer& renderer) {
  if (!visible_ || opacity_ <= 0) return;

  auto& r = renderer.flex();
  render_background(r, elem);
  render_icon(r, elem);
  render_text(r, elem);
  render_close_button(r, elem);
}

void ToastWidget::render_background(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;

  Color bg_color = get_type_color();
  float radius = 8.0f;

  if (style) {
    bg_color = style->get_variable_color("--toast-bg", bg_color);
    radius = style->border_radius[0];
  }

  Color bg_with_alpha = bg_color;
  bg_with_alpha.a *= opacity_;

  // Shadow
  r.draw_rect(2, 2, elem.width(), elem.height(), radius,
              Paint::solid(Color{0.0f, 0.0f, 0.0f, 0.12f * opacity_}), Paint::none(), 0);

  r.draw_rect(0, 0, elem.width(), elem.height(), radius,
              Paint::solid(bg_with_alpha), Paint::none(), 0);
}

void ToastWidget::render_icon(flex::Renderer& r, const Element& elem) {
  float icon_size = 20.0f;
  float padding = 12.0f;
  float cx = padding + icon_size / 2;
  float cy = elem.height() / 2;

  Color icon_color = {1.0f, 1.0f, 1.0f, opacity_};
  char path[256];

  switch (type_) {
    case Type::Success:
      // Checkmark
      snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
               cx - 6, cy, cx - 2, cy + 4, cx + 6, cy - 4);
      r.stroke_path(path, Paint::solid(icon_color), 2);
      break;

    case Type::Error:
      // X mark
      snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g M %.4g %.4g L %.4g %.4g",
               cx - 5, cy - 5, cx + 5, cy + 5,
               cx + 5, cy - 5, cx - 5, cy + 5);
      r.stroke_path(path, Paint::solid(icon_color), 2);
      break;

    case Type::Warning:
      // Triangle
      snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g Z",
               cx, cy - 7, cx + 8, cy + 6, cx - 8, cy + 6);
      r.stroke_path(path, Paint::solid(icon_color), 1.5f);
      break;

    case Type::Info:
      // Circle
      r.draw_circle(cx, cy, 8, Paint::none(), Paint::solid(icon_color), 1.5f);
      break;

    default:
      break;
  }
}

void ToastWidget::render_text(flex::Renderer& r, const Element& elem) {
  if (message_.empty()) return;

  auto* style = elem.computed_style;

  Color text_color = {1.0f, 1.0f, 1.0f, 1.0f};
  float font_size = 14.0f;
  std::string font_family = "Arial";

  if (style) {
    text_color = style->get_variable_color("--toast-text", text_color);
    font_size = style->font_size > 0 ? style->font_size : font_size;
    if (!style->font_family.empty()) font_family = style->font_family;
  }

  Color text_with_alpha = text_color;
  text_with_alpha.a *= opacity_;

  float icon_space = 44.0f;  // Space for icon
  float text_x = icon_space;
  float text_y = elem.height() / 2 + font_size / 3;

  r.draw_text(message_, text_x, text_y, font_family, font_size, false, text_with_alpha);
}

void ToastWidget::render_close_button(flex::Renderer& r, const Element& elem) {
  float btn_size = 20.0f;
  float padding = 8.0f;
  float cx = elem.width() - padding - btn_size / 2;
  float cy = elem.height() / 2;

  Color close_color = {1.0f, 1.0f, 1.0f, 0.78f * opacity_};

  char path[128];
  snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g M %.4g %.4g L %.4g %.4g",
           cx - 4, cy - 4, cx + 4, cy + 4,
           cx + 4, cy - 4, cx - 4, cy + 4);
  r.stroke_path(path, Paint::solid(close_color), 1.5f);
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
