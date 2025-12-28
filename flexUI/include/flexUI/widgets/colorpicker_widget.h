/*
 * flexUI - ColorPickerWidget
 *
 * Color picker - 使用 flex::Renderer 渲染
 */

#ifndef FLEXUI_COLORPICKER_WIDGET_H
#define FLEXUI_COLORPICKER_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include "../types.h"
#include <functional>

namespace flexUI {

class ColorPickerWidget : public Widget {
public:
  ColorPickerWidget();

  void render(const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "ColorPickerWidget"; }
  bool wants_mouse_capture() const override { return dragging_gradient_ || dragging_hue_; }

  Color color() const { return color_; }
  void set_color(const Color& c);

  float hue() const { return hue_; }
  float saturation() const { return sat_; }
  float value() const { return val_; }
  void set_hsv(float h, float s, float v);

  using ChangeCallback = std::function<void(const Color& color)>;
  void set_change_callback(ChangeCallback cb) { on_change_ = std::move(cb); }

private:
  void render_gradient(flex::Renderer& r, const Element& elem);
  void render_hue_bar(flex::Renderer& r, const Element& elem);
  void render_preview(flex::Renderer& r, const Element& elem);
  void render_cursor(flex::Renderer& r, const Element& elem);

  void hsv_to_rgb(float h, float s, float v, float& r, float& g, float& b);
  void rgb_to_hsv(float r, float g, float b, float& h, float& s, float& v);

  Color color_{1.0f, 0.0f, 0.0f, 1.0f};
  float hue_ = 0;
  float sat_ = 1.0f;
  float val_ = 1.0f;
  bool dragging_gradient_ = false;
  bool dragging_hue_ = false;
  ChangeCallback on_change_;
};

} // namespace flexUI

#endif // FLEXUI_COLORPICKER_WIDGET_H
