/*
 * flexUI - ModalWidget Implementation
 */

#include <flexUI/widgets/modal_widget.h>
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

ModalWidget::ModalWidget(const std::string& title) : title_(title) {}

const char* ModalWidget::side_name(Side side) {
  switch (side) {
    case Side::Left:
      return "left";
    case Side::Right:
      return "right";
    case Side::Top:
      return "top";
    case Side::Bottom:
      return "bottom";
    default:
      return "center";
  }
}

void ModalWidget::sync_host_semantics() {
  set_host_attribute("role", "dialog");
  set_host_attribute("data-state", open_ ? "open" : "closed");
  set_host_boolean_attribute("aria-modal", open_);
  set_host_boolean_attribute("aria-hidden", !open_);
  if (side_ != Side::Center) {
    set_host_attribute("data-side", side_name(side_));
  } else {
    clear_host_attribute("data-side");
  }
  if (title_.empty()) {
    clear_host_attribute("aria-label");
  } else {
    set_host_attribute("aria-label", title_);
  }
}

void ModalWidget::open() {
  open_ = true;
  sync_host_semantics();
  opacity_ = 0;
  dirty_ = true;
}

void ModalWidget::close() {
  open_ = false;
  sync_host_semantics();
  if (on_close_) on_close_();
  dirty_ = true;
}

void ModalWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
  sync_host_semantics();
  if (!open_ && opacity_ <= 0) return;

  emit_overlay_background(commands, elem);
  render_dialog(commands, elem);
}

void ModalWidget::emit_overlay_background(RenderCommandList& commands, const Element& elem) {
  float alpha = 0.5f * opacity_;

  commands.draw_rect(0, 0, elem.width(), elem.height(), 0,
                     Paint::solid(Color{0.0f, 0.0f, 0.0f, alpha}),
                     Paint::none(), 0);
}

ModalWidget::DialogBounds ModalWidget::current_dialog_bounds(
    const Element& elem) const {
  auto* style = elem.computed_style;

  float dialog_w = std::min(500.0f, elem.width() - 40.0f);
  float dialog_h = std::min(400.0f, elem.height() - 40.0f);

  if (style) {
    dialog_w = style->get_variable_float("--modal-width", dialog_w);
    dialog_h = style->get_variable_float("--modal-height", dialog_h);
  }

  float dialog_x = 0.0f;
  float dialog_y = 0.0f;
  switch (side_) {
    case Side::Left:
      dialog_h = style ? style->get_variable_float("--modal-height", elem.height())
                       : elem.height();
      dialog_x = 0.0f;
      dialog_y = 0.0f;
      break;
    case Side::Right:
      dialog_h = style ? style->get_variable_float("--modal-height", elem.height())
                       : elem.height();
      dialog_x = elem.width() - dialog_w;
      dialog_y = 0.0f;
      break;
    case Side::Top:
      dialog_w = style ? style->get_variable_float("--modal-width", elem.width())
                       : elem.width();
      dialog_x = 0.0f;
      dialog_y = 0.0f;
      break;
    case Side::Bottom:
      dialog_w = style ? style->get_variable_float("--modal-width", elem.width())
                       : elem.width();
      dialog_x = 0.0f;
      dialog_y = elem.height() - dialog_h;
      break;
    case Side::Center:
    default:
      dialog_x = (elem.width() - dialog_w) / 2.0f;
      dialog_y = (elem.height() - dialog_h) / 2.0f;
      break;
  }

  return {dialog_x, dialog_y, dialog_w, dialog_h};
}

void ModalWidget::render_dialog(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;

  Color bg_color = {1.0f, 1.0f, 1.0f, 1.0f};
  float radius = 12.0f;
  if (style) {
    bg_color = style->get_variable_color("--modal-bg", bg_color);
    radius = style->border_radius[0];
  }

  const auto bounds = current_dialog_bounds(elem);
  float dialog_x = bounds.x;
  float dialog_y = bounds.y;
  float dialog_w = bounds.w;
  float dialog_h = bounds.h;

  float render_x = dialog_x;
  float render_y = dialog_y;
  float render_w = dialog_w;
  float render_h = dialog_h;
  if (side_ == Side::Center) {
    const float scale = 0.9f + 0.1f * opacity_;
    render_w = dialog_w * scale;
    render_h = dialog_h * scale;
    render_x += dialog_w * (1.0f - scale) * 0.5f;
    render_y += dialog_h * (1.0f - scale) * 0.5f;
  } else {
    const float slide = (1.0f - opacity_) * 24.0f;
    switch (side_) {
      case Side::Left:
        render_x -= slide;
        break;
      case Side::Right:
        render_x += slide;
        break;
      case Side::Top:
        render_y -= slide;
        break;
      case Side::Bottom:
        render_y += slide;
        break;
      default:
        break;
    }
  }

  Color bg_with_alpha = bg_color;
  bg_with_alpha.a *= opacity_;

  commands.draw_rect(dialog_x + 4, dialog_y + 4, dialog_w, dialog_h, radius,
                     Paint::solid(Color{0.0f, 0.0f, 0.0f, 0.16f * opacity_}),
                     Paint::none(), 0);
  commands.draw_rect(render_x, render_y, render_w, render_h, radius,
                     Paint::solid(bg_with_alpha), Paint::none(), 0);

  if (opacity_ > 0.5f) {
    render_header(commands, elem, render_x, render_y, render_w);
  }
}

void ModalWidget::render_header(RenderCommandList& commands, const Element& elem,
                                float dialog_x, float dialog_y, float dialog_w) {
  auto* style = elem.computed_style;

  Color text_color = {0.0f, 0.0f, 0.0f, 1.0f};
  float font_size = 18.0f;
  float header_height = 56.0f;

  if (style) {
    text_color = style->get_variable_color("--modal-text", text_color);
    font_size = style->font_size > 0 ? style->font_size : font_size;
  }

  if (!title_.empty() && style) {
    Color title_color = text_color;
    title_color.a *= opacity_;
    const auto text_block = layout_text_block(
        style, title_, dialog_x + 20.0f, dialog_y,
        std::max(0.0f, dialog_w - 64.0f), header_height, title_color,
        TextVerticalAlign::Middle);
    emit_text_block(commands, text_block);
  }

  if (show_close_) {
    float x = dialog_x + dialog_w - 36;
    float y = dialog_y + header_height / 2;
    Color close_color = {0.39f, 0.39f, 0.39f, opacity_};
    char close_path[128];
    stbsp_snprintf(close_path, sizeof(close_path), "M %.4g %.4g L %.4g %.4g M %.4g %.4g L %.4g %.4g",
             x - 6, y - 6, x + 6, y + 6,
             x + 6, y - 6, x - 6, y + 6);
    commands.stroke_path(close_path, Paint::solid(close_color), 2);
  }

  // Divider
  char path[128];
  stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g",
           dialog_x, dialog_y + header_height,
           dialog_x + dialog_w, dialog_y + header_height);
  commands.stroke_path(path, Paint::solid(Color{0.90f, 0.91f, 0.92f, opacity_}), 1);
}

bool ModalWidget::handle_event(const Event& event, Element& elem) {
  if (!open_) return false;

  if (event.type == EventType::MouseDown) {
    const auto bounds = current_dialog_bounds(elem);
    float dialog_w = bounds.w;
    float dialog_h = bounds.h;
    float dialog_x = bounds.x;
    float dialog_y = bounds.y;

    const flex::Vec2 local_pos =
        detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
    float local_x = local_pos.x;
    float local_y = local_pos.y;

    if (local_x < dialog_x || local_x > dialog_x + dialog_w ||
        local_y < dialog_y || local_y > dialog_y + dialog_h) {
      if (close_on_overlay_) {
        close();
        elem.mark_paint_dirty();
      }
      return true;
    }

    if (show_close_) {
      float close_x = dialog_x + dialog_w - 36;
      float close_y = dialog_y + 28;
      if (std::abs(local_x - close_x) < 12 && std::abs(local_y - close_y) < 12) {
        close();
        elem.mark_paint_dirty();
        return true;
      }
    }

    return true;
  }

  return open_;
}

void ModalWidget::update(float delta_ms, Element& elem) {
  sync_host_semantics();

  float target = open_ ? 1.0f : 0.0f;
  float speed = 8.0f * delta_ms / 1000.0f;

  if (std::abs(opacity_ - target) > 0.01f) {
    opacity_ += (target - opacity_) * speed;
    elem.mark_paint_dirty();
  } else {
    opacity_ = target;
  }
}

} // namespace flexUI
