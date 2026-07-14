/*
 * flexUI - ToastWidget Implementation
 */

#include <flexUI/widgets/toast_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/detail/css_render_transform.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <flexUI/text_layout.h>
#include <algorithm>
#include <cmath>
#include <stb_sprintf.h>

namespace flexUI {

ToastWidget::ToastWidget(const std::string& message, Type type)
    : message_(message), type_(type) {}

bool ToastWidget::measure_intrinsic_size(const Element& elem, float available_width,
                                         float available_height, float& out_width,
                                         float& out_height) const {
  (void)available_width;
  (void)available_height;
  const auto* style = elem.computed_style;
  ComputedStyle measure_style;
  if (style) {
    measure_style = *style;
  }
  measure_style.font_size =
      style && style->font_size > 0.0f ? style->font_size : 14.0f;
  const float text_width =
      message_.empty() ? 120.0f : approximate_segmented_text_width(&measure_style, message_);
  out_width = std::max(240.0f, text_width + 72.0f);
  out_height = 48.0f;
  return true;
}

void ToastWidget::show() {
  const bool urgent = type_ == Type::Error || type_ == Type::Warning;
  set_host_attribute("role", urgent ? "alert" : "status");
  set_host_attribute("data-state", "open");
  set_host_boolean_attribute("aria-hidden", false);
  set_host_attribute("aria-live", urgent ? "assertive" : "polite");
  set_host_attribute("aria-atomic", "true");
  switch (type_) {
    case Type::Success:
      set_host_attribute("data-type", "success");
      break;
    case Type::Error:
      set_host_attribute("data-type", "error");
      break;
    case Type::Warning:
      set_host_attribute("data-type", "warning");
      break;
    case Type::Info:
      set_host_attribute("data-type", "info");
      break;
    default:
      set_host_attribute("data-type", "default");
      break;
  }
  visible_ = true;
  timer_ = 0;
  opacity_ = 0;
  dirty_ = true;
}

void ToastWidget::hide() {
  const bool urgent = type_ == Type::Error || type_ == Type::Warning;
  set_host_attribute("role", urgent ? "alert" : "status");
  set_host_attribute("data-state", "closed");
  set_host_boolean_attribute("aria-hidden", true);
  set_host_attribute("aria-live", urgent ? "assertive" : "polite");
  set_host_attribute("aria-atomic", "true");
  switch (type_) {
    case Type::Success:
      set_host_attribute("data-type", "success");
      break;
    case Type::Error:
      set_host_attribute("data-type", "error");
      break;
    case Type::Warning:
      set_host_attribute("data-type", "warning");
      break;
    case Type::Info:
      set_host_attribute("data-type", "info");
      break;
    default:
      set_host_attribute("data-type", "default");
      break;
  }
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

void ToastWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
  const bool urgent = type_ == Type::Error || type_ == Type::Warning;
  set_host_attribute("role", urgent ? "alert" : "status");
  set_host_attribute("data-state", visible_ ? "open" : "closed");
  set_host_boolean_attribute("aria-hidden", !visible_);
  set_host_attribute("aria-live", urgent ? "assertive" : "polite");
  set_host_attribute("aria-atomic", "true");
  switch (type_) {
    case Type::Success:
      set_host_attribute("data-type", "success");
      break;
    case Type::Error:
      set_host_attribute("data-type", "error");
      break;
    case Type::Warning:
      set_host_attribute("data-type", "warning");
      break;
    case Type::Info:
      set_host_attribute("data-type", "info");
      break;
    default:
      set_host_attribute("data-type", "default");
      break;
  }
  if (!visible_ || opacity_ <= 0) return;

  render_background(commands, elem);
  render_icon(commands, elem);
  render_text(commands, elem);
  render_close_button(commands, elem);
}

void ToastWidget::render_background(RenderCommandList& commands, const Element& elem) {
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
  commands.draw_rect(2, 2, elem.width(), elem.height(), radius,
                     Paint::solid(Color{0.0f, 0.0f, 0.0f, 0.12f * opacity_}),
                     Paint::none(), 0);

  commands.draw_rect(0, 0, elem.width(), elem.height(), radius,
                     Paint::solid(bg_with_alpha), Paint::none(), 0);
}

void ToastWidget::render_icon(RenderCommandList& commands, const Element& elem) {
  float icon_size = 20.0f;
  float padding = 12.0f;
  float cx = padding + icon_size / 2;
  float cy = elem.height() / 2;

  Color icon_color = {1.0f, 1.0f, 1.0f, opacity_};
  char path[256];

  switch (type_) {
    case Type::Success:
      // Checkmark
      stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
               cx - 6, cy, cx - 2, cy + 4, cx + 6, cy - 4);
      commands.stroke_path(path, Paint::solid(icon_color), 2);
      break;

    case Type::Error:
      // X mark
      stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g M %.4g %.4g L %.4g %.4g",
               cx - 5, cy - 5, cx + 5, cy + 5,
               cx + 5, cy - 5, cx - 5, cy + 5);
      commands.stroke_path(path, Paint::solid(icon_color), 2);
      break;

    case Type::Warning:
      // Triangle
      stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g Z",
               cx, cy - 7, cx + 8, cy + 6, cx - 8, cy + 6);
      commands.stroke_path(path, Paint::solid(icon_color), 1.5f);
      break;

    case Type::Info:
      // Circle
      commands.draw_circle(cx, cy, 8, Paint::none(), Paint::solid(icon_color), 1.5f);
      break;

    default:
      break;
  }
}

void ToastWidget::render_text(RenderCommandList& commands, const Element& elem) {
  if (message_.empty()) return;

  auto* style = elem.computed_style;

  Color text_color = {1.0f, 1.0f, 1.0f, 1.0f};
  float font_size = 14.0f;
  if (style) {
    text_color = style->get_variable_color("--toast-text", text_color);
    font_size = style->font_size > 0 ? style->font_size : font_size;
  }

  Color text_with_alpha = text_color;
  text_with_alpha.a *= opacity_;

  float icon_space = 44.0f;  // Space for icon
  const float trailing_space = 28.0f;
  if (style) {
    const auto text_block = layout_text_block(
        style, message_, icon_space, 0.0f,
        std::max(0.0f, elem.width() - icon_space - trailing_space), elem.height(),
        text_with_alpha, TextVerticalAlign::Middle);
    emit_text_block(commands, text_block);
  }
}

void ToastWidget::render_close_button(RenderCommandList& commands, const Element& elem) {
  float btn_size = 20.0f;
  float padding = 8.0f;
  float cx = elem.width() - padding - btn_size / 2;
  float cy = elem.height() / 2;

  Color close_color = {1.0f, 1.0f, 1.0f, 0.78f * opacity_};

  char path[128];
  stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g M %.4g %.4g L %.4g %.4g",
           cx - 4, cy - 4, cx + 4, cy + 4,
           cx + 4, cy - 4, cx - 4, cy + 4);
  commands.stroke_path(path, Paint::solid(close_color), 1.5f);
}

bool ToastWidget::handle_event(const Event& event, Element& elem) {
  if (!visible_) return false;

  if (event.type == EventType::MouseDown) {
    // Check if click is on close button
    float btn_size = 24.0f;
    float padding = 8.0f;
    float btn_x = elem.width() - padding - btn_size;

    const flex::Vec2 local_pos =
        detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
    float local_x = local_pos.x;
    if (local_x >= btn_x) {
      hide();
      return true;
    }
  }
  return false;
}

void ToastWidget::update(float delta_ms, Element& elem) {
  const bool urgent = type_ == Type::Error || type_ == Type::Warning;
  set_host_attribute("role", urgent ? "alert" : "status");
  set_host_attribute("data-state", visible_ ? "open" : "closed");
  set_host_boolean_attribute("aria-hidden", !visible_);
  set_host_attribute("aria-live", urgent ? "assertive" : "polite");
  set_host_attribute("aria-atomic", "true");
  switch (type_) {
    case Type::Success:
      set_host_attribute("data-type", "success");
      break;
    case Type::Error:
      set_host_attribute("data-type", "error");
      break;
    case Type::Warning:
      set_host_attribute("data-type", "warning");
      break;
    case Type::Info:
      set_host_attribute("data-type", "info");
      break;
    default:
      set_host_attribute("data-type", "default");
      break;
  }

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

} // namespace flexUI
