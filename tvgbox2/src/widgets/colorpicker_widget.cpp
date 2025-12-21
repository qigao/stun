/*
 * tvgbox2 - ColorPickerWidget Implementation
 */

#include <tvgbox2/widgets/colorpicker_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <thorvg.h>
#include <algorithm>
#include <cmath>

namespace tvgbox2 {

ColorPickerWidget::ColorPickerWidget() {
  hsv_to_rgb(hue_, sat_, val_, color_.r, color_.g, color_.b);
}

void ColorPickerWidget::set_color(const Color& c) {
  color_ = c;
  rgb_to_hsv(c.r, c.g, c.b, hue_, sat_, val_);
  dirty_ = true;
}

void ColorPickerWidget::set_hsv(float h, float s, float v) {
  hue_ = std::clamp(h, 0.0f, 360.0f);
  sat_ = std::clamp(s, 0.0f, 1.0f);
  val_ = std::clamp(v, 0.0f, 1.0f);
  hsv_to_rgb(hue_, sat_, val_, color_.r, color_.g, color_.b);
  dirty_ = true;
}

void ColorPickerWidget::hsv_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
  float c = v * s;
  float x = c * (1 - std::abs(std::fmod(h / 60.0f, 2.0f) - 1));
  float m = v - c;
  float rf, gf, bf;

  if (h < 60)       { rf = c; gf = x; bf = 0; }
  else if (h < 120) { rf = x; gf = c; bf = 0; }
  else if (h < 180) { rf = 0; gf = c; bf = x; }
  else if (h < 240) { rf = 0; gf = x; bf = c; }
  else if (h < 300) { rf = x; gf = 0; bf = c; }
  else              { rf = c; gf = 0; bf = x; }

  r = static_cast<uint8_t>((rf + m) * 255);
  g = static_cast<uint8_t>((gf + m) * 255);
  b = static_cast<uint8_t>((bf + m) * 255);
}

void ColorPickerWidget::rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b, float& h, float& s, float& v) {
  float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
  float cmax = std::max({rf, gf, bf});
  float cmin = std::min({rf, gf, bf});
  float delta = cmax - cmin;

  if (delta == 0) h = 0;
  else if (cmax == rf) h = 60 * std::fmod((gf - bf) / delta + 6, 6.0f);
  else if (cmax == gf) h = 60 * ((bf - rf) / delta + 2);
  else h = 60 * ((rf - gf) / delta + 4);

  s = (cmax == 0) ? 0 : delta / cmax;
  v = cmax;
}

void ColorPickerWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  auto* style = elem.computed_style;
  Color bg_color = {255, 255, 255, 255};
  if (style) bg_color = style->get_variable_color("--picker-bg", bg_color);

  auto bg = tvg::Shape::gen();
  bg->appendRect(0, 0, elem.width(), elem.height(), 8, 8);
  bg->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);
  scene->push(std::move(bg));

  render_gradient(scene, elem);
  render_hue_bar(scene, elem);
  render_preview(scene, elem);
  render_cursor(scene, elem);
}

void ColorPickerWidget::render_gradient(tvg::Scene* scene, const Element& elem) {
  float padding = 12.0f;
  float hue_bar_w = 24.0f;
  float preview_h = 40.0f;
  float grad_w = elem.width() - padding * 3 - hue_bar_w;
  float grad_h = elem.height() - padding * 2 - preview_h - 8;

  uint8_t hr, hg, hb;
  hsv_to_rgb(hue_, 1.0f, 1.0f, hr, hg, hb);

  auto base = tvg::Shape::gen();
  base->appendRect(padding, padding, grad_w, grad_h, 4, 4);
  base->fill(hr, hg, hb, 255);
  scene->push(std::move(base));

  auto white_grad = tvg::Shape::gen();
  white_grad->appendRect(padding, padding, grad_w, grad_h, 4, 4);
  auto fill_w = tvg::LinearGradient::gen();
  fill_w->linear(padding, padding, padding + grad_w, padding);
  tvg::Fill::ColorStop stops_w[2] = {{0, 255, 255, 255, 255}, {1, 255, 255, 255, 0}};
  fill_w->colorStops(stops_w, 2);
  white_grad->fill(std::move(fill_w));
  scene->push(std::move(white_grad));

  auto black_grad = tvg::Shape::gen();
  black_grad->appendRect(padding, padding, grad_w, grad_h, 4, 4);
  auto fill_b = tvg::LinearGradient::gen();
  fill_b->linear(padding, padding, padding, padding + grad_h);
  tvg::Fill::ColorStop stops_b[2] = {{0, 0, 0, 0, 0}, {1, 0, 0, 0, 255}};
  fill_b->colorStops(stops_b, 2);
  black_grad->fill(std::move(fill_b));
  scene->push(std::move(black_grad));

  auto border = tvg::Shape::gen();
  border->appendRect(padding, padding, grad_w, grad_h, 4, 4);
  border->strokeFill(200, 200, 200, 255);
  border->strokeWidth(1);
  scene->push(std::move(border));
}

void ColorPickerWidget::render_hue_bar(tvg::Scene* scene, const Element& elem) {
  float padding = 12.0f;
  float hue_bar_w = 24.0f;
  float preview_h = 40.0f;
  float bar_h = elem.height() - padding * 2 - preview_h - 8;
  float bar_x = elem.width() - padding - hue_bar_w;

  for (int i = 0; i < 6; i++) {
    float y1 = padding + i * bar_h / 6;
    float y2 = padding + (i + 1) * bar_h / 6;

    uint8_t r1, g1, b1, r2, g2, b2;
    hsv_to_rgb(i * 60.0f, 1.0f, 1.0f, r1, g1, b1);
    hsv_to_rgb((i + 1) * 60.0f, 1.0f, 1.0f, r2, g2, b2);

    auto seg = tvg::Shape::gen();
    seg->appendRect(bar_x, y1, hue_bar_w, y2 - y1);
    auto fill = tvg::LinearGradient::gen();
    fill->linear(bar_x, y1, bar_x, y2);
    tvg::Fill::ColorStop stops[2] = {{0, r1, g1, b1, 255}, {1, r2, g2, b2, 255}};
    fill->colorStops(stops, 2);
    seg->fill(std::move(fill));
    scene->push(std::move(seg));
  }

  float indicator_y = padding + (hue_ / 360.0f) * bar_h;
  auto indicator = tvg::Shape::gen();
  indicator->appendRect(bar_x - 2, indicator_y - 3, hue_bar_w + 4, 6, 2, 2);
  indicator->strokeFill(255, 255, 255, 255);
  indicator->strokeWidth(2);
  scene->push(std::move(indicator));
}

void ColorPickerWidget::render_preview(tvg::Scene* scene, const Element& elem) {
  float padding = 12.0f;
  float preview_h = 40.0f;
  float preview_y = elem.height() - padding - preview_h;
  float preview_w = elem.width() - padding * 2;

  auto preview = tvg::Shape::gen();
  preview->appendRect(padding, preview_y, preview_w, preview_h, 4, 4);
  preview->fill(color_.r, color_.g, color_.b, color_.a);
  scene->push(std::move(preview));

  auto border = tvg::Shape::gen();
  border->appendRect(padding, preview_y, preview_w, preview_h, 4, 4);
  border->strokeFill(200, 200, 200, 255);
  border->strokeWidth(1);
  scene->push(std::move(border));
}

void ColorPickerWidget::render_cursor(tvg::Scene* scene, const Element& elem) {
  float padding = 12.0f;
  float hue_bar_w = 24.0f;
  float preview_h = 40.0f;
  float grad_w = elem.width() - padding * 3 - hue_bar_w;
  float grad_h = elem.height() - padding * 2 - preview_h - 8;

  float cx = padding + sat_ * grad_w;
  float cy = padding + (1 - val_) * grad_h;

  auto outer = tvg::Shape::gen();
  outer->appendCircle(cx, cy, 8, 8);
  outer->strokeFill(255, 255, 255, 255);
  outer->strokeWidth(2);
  scene->push(std::move(outer));

  auto inner = tvg::Shape::gen();
  inner->appendCircle(cx, cy, 6, 6);
  inner->strokeFill(0, 0, 0, 255);
  inner->strokeWidth(1);
  scene->push(std::move(inner));
}

bool ColorPickerWidget::handle_event(const Event& event, Element& elem) {
  float padding = 12.0f;
  float hue_bar_w = 24.0f;
  float preview_h = 40.0f;
  float grad_w = elem.width() - padding * 3 - hue_bar_w;
  float grad_h = elem.height() - padding * 2 - preview_h - 8;
  float bar_x = elem.width() - padding - hue_bar_w;

  float local_x = event.x - elem.absolute_x();
  float local_y = event.y - elem.absolute_y();

  if (event.type == EventType::MouseDown) {
    if (local_x >= bar_x && local_x <= bar_x + hue_bar_w &&
        local_y >= padding && local_y <= padding + grad_h) {
      dragging_hue_ = true;
      hue_ = std::clamp((local_y - padding) / grad_h * 360.0f, 0.0f, 360.0f);
      hsv_to_rgb(hue_, sat_, val_, color_.r, color_.g, color_.b);
      if (on_change_) on_change_(color_);
      elem.mark_paint_dirty();
      return true;
    }

    if (local_x >= padding && local_x <= padding + grad_w &&
        local_y >= padding && local_y <= padding + grad_h) {
      dragging_gradient_ = true;
      sat_ = std::clamp((local_x - padding) / grad_w, 0.0f, 1.0f);
      val_ = std::clamp(1.0f - (local_y - padding) / grad_h, 0.0f, 1.0f);
      hsv_to_rgb(hue_, sat_, val_, color_.r, color_.g, color_.b);
      if (on_change_) on_change_(color_);
      elem.mark_paint_dirty();
      return true;
    }
  }

  if (event.type == EventType::MouseMove) {
    if (dragging_hue_) {
      hue_ = std::clamp((local_y - padding) / grad_h * 360.0f, 0.0f, 360.0f);
      hsv_to_rgb(hue_, sat_, val_, color_.r, color_.g, color_.b);
      if (on_change_) on_change_(color_);
      elem.mark_paint_dirty();
      return true;
    }
    if (dragging_gradient_) {
      sat_ = std::clamp((local_x - padding) / grad_w, 0.0f, 1.0f);
      val_ = std::clamp(1.0f - (local_y - padding) / grad_h, 0.0f, 1.0f);
      hsv_to_rgb(hue_, sat_, val_, color_.r, color_.g, color_.b);
      if (on_change_) on_change_(color_);
      elem.mark_paint_dirty();
      return true;
    }
  }

  if (event.type == EventType::MouseUp) {
    dragging_hue_ = false;
    dragging_gradient_ = false;
  }

  return false;
}

void ColorPickerWidget::update(float delta_ms, Element& elem) {}

} // namespace tvgbox2
