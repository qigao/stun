/*
 * tvgbox2 - ModalWidget Implementation
 */

#include <tvgbox2/widgets/modal_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <thorvg.h>
#include <algorithm>
#include <cmath>

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

void ModalWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  if (!open_ && opacity_ <= 0) return;

  render_overlay(scene, elem);
  render_dialog(scene, elem);
}

void ModalWidget::render_overlay(tvg::Scene* scene, const Element& elem) {
  uint8_t alpha = static_cast<uint8_t>(128 * opacity_);
  
  auto overlay = tvg::Shape::gen();
  overlay->appendRect(0, 0, elem.width(), elem.height());
  overlay->fill(0, 0, 0, alpha);
  scene->push(std::move(overlay));
}

void ModalWidget::render_dialog(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;

  Color bg_color = {255, 255, 255, 255};
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

  auto shadow = tvg::Shape::gen();
  shadow->appendRect(dialog_x + 4, dialog_y + 4, dialog_w, dialog_h, radius, radius);
  shadow->fill(0, 0, 0, static_cast<uint8_t>(40 * opacity_));
  scene->push(std::move(shadow));

  auto bg = tvg::Shape::gen();
  bg->appendRect(dialog_x + offset_x, dialog_y + offset_y, 
                 dialog_w * scale, dialog_h * scale, radius, radius);
  bg->fill(bg_color.r, bg_color.g, bg_color.b, static_cast<uint8_t>(bg_color.a * opacity_));
  scene->push(std::move(bg));

  if (opacity_ > 0.5f) {
    render_header(scene, elem, dialog_x + offset_x, dialog_y + offset_y, dialog_w * scale);
  }
}

void ModalWidget::render_header(tvg::Scene* scene, const Element& elem, 
                                float dialog_x, float dialog_y, float dialog_w) {
  auto* style = elem.computed_style;
  
  Color text_color = {0, 0, 0, 255};
  float font_size = 18.0f;
  std::string font_family = "Arial";
  float header_height = 56.0f;

  if (style) {
    text_color = style->get_variable_color("--modal-text", text_color);
    font_size = style->font_size > 0 ? style->font_size : font_size;
    if (!style->font_family.empty()) font_family = style->font_family;
  }

  if (!title_.empty()) {
    auto title = tvg::Text::gen();
    title->font(font_family.c_str());
    title->size(font_size);
    title->text(title_.c_str());
    title->fill(text_color.r, text_color.g, text_color.b);
    title->opacity(static_cast<uint8_t>(255 * opacity_));
    title->translate(dialog_x + 20, dialog_y + header_height / 2 + font_size / 3);
    scene->push(std::move(title));
  }

  if (show_close_) {
    render_close_button(scene, dialog_x + dialog_w - 36, dialog_y + header_height / 2);
  }

  auto divider = tvg::Shape::gen();
  divider->moveTo(dialog_x, dialog_y + header_height);
  divider->lineTo(dialog_x + dialog_w, dialog_y + header_height);
  divider->strokeFill(229, 231, 235, static_cast<uint8_t>(255 * opacity_));
  divider->strokeWidth(1);
  scene->push(std::move(divider));
}

void ModalWidget::render_close_button(tvg::Scene* scene, float x, float y) {
  uint8_t alpha = static_cast<uint8_t>(255 * opacity_);

  auto close = tvg::Shape::gen();
  close->moveTo(x - 6, y - 6);
  close->lineTo(x + 6, y + 6);
  close->moveTo(x + 6, y - 6);
  close->lineTo(x - 6, y + 6);
  close->strokeFill(100, 100, 100, alpha);
  close->strokeWidth(2);
  close->strokeCap(tvg::StrokeCap::Round);
  scene->push(std::move(close));
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
