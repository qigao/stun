/*
 * tvgbox2 - ModalWidget Implementation
 */

#include <tvgbox2/widgets/modal_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/renderer.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tvgbox2 {

ModalWidget::ModalWidget(const std::string& title) : title_(title) {}

void ModalWidget::open() {
  open_ = true;
  opacity_ = 0;
  dirty_ = true;
}

void ModalWidget::close() {
  open_ = false;
  if (on_close_) on_close_();
  dirty_ = true;
}

void ModalWidget::render(const Element& elem, Renderer& renderer) {
  if (!open_ && opacity_ <= 0) return;

  auto& r = renderer.flex();
  render_overlay(r, elem);
  render_dialog(r, elem);
}

void ModalWidget::render_overlay(flex::Renderer& r, const Element& elem) {
  float alpha = 0.5f * opacity_;

  r.draw_rect(0, 0, elem.width(), elem.height(), 0,
              Paint::solid(Color{0.0f, 0.0f, 0.0f, alpha}), Paint::none(), 0);
}

void ModalWidget::render_dialog(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;

  Color bg_color = {1.0f, 1.0f, 1.0f, 1.0f};
  float dialog_w = std::min(500.0f, elem.width() - 40);
  float dialog_h = std::min(400.0f, elem.height() - 40);
  float dialog_x = (elem.width() - dialog_w) / 2;
  float dialog_y = (elem.height() - dialog_h) / 2;
  float radius = 12.0f;

  if (style) {
    bg_color = style->get_variable_color("--modal-bg", bg_color);
    dialog_w = style->get_variable_float("--modal-width", dialog_w);
    dialog_h = style->get_variable_float("--modal-height", dialog_h);
    radius = style->border_radius[0];
    dialog_x = (elem.width() - dialog_w) / 2;
    dialog_y = (elem.height() - dialog_h) / 2;
  }

  float scale = 0.9f + 0.1f * opacity_;
  float offset_x = dialog_w * (1 - scale) / 2;
  float offset_y = dialog_h * (1 - scale) / 2;

  // Shadow
  r.draw_rect(dialog_x + 4, dialog_y + 4, dialog_w, dialog_h, radius,
              Paint::solid(Color{0.0f, 0.0f, 0.0f, 0.16f * opacity_}), Paint::none(), 0);

  // Background
  Color bg_with_alpha = bg_color;
  bg_with_alpha.a *= opacity_;

  r.draw_rect(dialog_x + offset_x, dialog_y + offset_y,
              dialog_w * scale, dialog_h * scale, radius,
              Paint::solid(bg_with_alpha), Paint::none(), 0);

  if (opacity_ > 0.5f) {
    render_header(r, elem, dialog_x + offset_x, dialog_y + offset_y, dialog_w * scale);
  }
}

void ModalWidget::render_header(flex::Renderer& r, const Element& elem,
                                float dialog_x, float dialog_y, float dialog_w) {
  auto* style = elem.computed_style;

  Color text_color = {0.0f, 0.0f, 0.0f, 1.0f};
  float font_size = 18.0f;
  std::string font_family = "Arial";
  float header_height = 56.0f;

  if (style) {
    text_color = style->get_variable_color("--modal-text", text_color);
    font_size = style->font_size > 0 ? style->font_size : font_size;
    if (!style->font_family.empty()) font_family = style->font_family;
  }

  if (!title_.empty()) {
    Color title_color = text_color;
    title_color.a *= opacity_;

    r.draw_text(title_, dialog_x + 20, dialog_y + header_height / 2 + font_size / 3,
                font_family, font_size, false, title_color);
  }

  if (show_close_) {
    render_close_button(r, dialog_x + dialog_w - 36, dialog_y + header_height / 2);
  }

  // Divider
  char path[128];
  snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g",
           dialog_x, dialog_y + header_height,
           dialog_x + dialog_w, dialog_y + header_height);
  r.stroke_path(path, Paint::solid(Color{0.90f, 0.91f, 0.92f, opacity_}), 1);
}

void ModalWidget::render_close_button(flex::Renderer& r, float x, float y) {
  Color close_color = {0.39f, 0.39f, 0.39f, opacity_};

  char path[128];
  snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g M %.4g %.4g L %.4g %.4g",
           x - 6, y - 6, x + 6, y + 6,
           x + 6, y - 6, x - 6, y + 6);
  r.stroke_path(path, Paint::solid(close_color), 2);
}

bool ModalWidget::handle_event(const Event& event, Element& elem) {
  if (!open_) return false;

  if (event.type == EventType::MouseDown) {
    auto* style = elem.computed_style;
    float dialog_w = style ? style->get_variable_float("--modal-width", 500.0f) : 500.0f;
    float dialog_h = style ? style->get_variable_float("--modal-height", 400.0f) : 400.0f;
    float dialog_x = (elem.width() - dialog_w) / 2;
    float dialog_y = (elem.height() - dialog_h) / 2;

    float local_x = event.x - elem.absolute_x();
    float local_y = event.y - elem.absolute_y();

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
  float target = open_ ? 1.0f : 0.0f;
  float speed = 8.0f * delta_ms / 1000.0f;

  if (std::abs(opacity_ - target) > 0.01f) {
    opacity_ += (target - opacity_) * speed;
    elem.mark_paint_dirty();
  } else {
    opacity_ = target;
  }
}

} // namespace tvgbox2
