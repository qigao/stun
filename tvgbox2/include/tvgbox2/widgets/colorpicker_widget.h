#ifndef TVGBOX2_COLORPICKER_WIDGET_H
#define TVGBOX2_COLORPICKER_WIDGET_H

#include "../widget.h"
#include "../types.h"
#include <functional>

namespace tvgbox2 {

class ColorPickerWidget : public Widget {
public:
  ColorPickerWidget();

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "ColorPickerWidget"; }

  Color color() const { return color_; }
  void set_color(const Color& c);

  float hue() const { return hue_; }
  float saturation() const { return sat_; }
  float value() const { return val_; }
  void set_hsv(float h, float s, float v);

  using ChangeCallback = std::function<void(const Color& color)>;
  void set_change_callback(ChangeCallback cb) { on_change_ = std::move(cb); }

private:
  void render_gradient(tvg::Scene* scene, const Element& elem);
  void render_hue_bar(tvg::Scene* scene, const Element& elem);
  void render_preview(tvg::Scene* scene, const Element& elem);
  void render_cursor(tvg::Scene* scene, const Element& elem);

  void hsv_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b);
  void rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b, float& h, float& s, float& v);

  Color color_ = {255, 0, 0, 255};
  float hue_ = 0;
  float sat_ = 1.0f;
  float val_ = 1.0f;
  bool dragging_gradient_ = false;
  bool dragging_hue_ = false;
  ChangeCallback on_change_;
};

} // namespace tvgbox2
#endif
