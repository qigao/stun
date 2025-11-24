#pragma once

#include <algorithm>
#include <functional>
#include <nanogui.h>
#include <nanogui/opengl.h>
#include <nanovg.h>
#include <string>
#include <vector>

using namespace nanogui;

// Simple zoom button widget
class ZoomButtonWidget : public Widget {
public:
  ZoomButtonWidget(Widget *parent, int icon, const std::string &label);
  
  void set_callback(std::function<void()> callback) { m_callback = callback; }
  
  Vector2i preferred_size_impl(NVGcontext *) const override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  void draw(NVGcontext *ctx) override;

private:
  int m_icon;
  std::string m_label;
  bool m_hovered = false;
  bool m_pressed = false;
  std::function<void()> m_callback;
};

class ZoomPanelModule : public Widget {
public:
  ZoomPanelModule(Widget *parent);

  void set_zoom_callback(std::function<void(float)> callback) { m_zoom_callback = callback; }
  void set_zoom_level(float zoom);
  float get_zoom_level() const { return m_zoom_level; }
  
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;
  void draw(NVGcontext *ctx) override;

private:
  void create_buttons();
  
  std::vector<ZoomButtonWidget*> m_buttons;
  Label *m_zoom_label = nullptr;
  float m_zoom_level = 1.0f;
  std::function<void(float)> m_zoom_callback;
  bool m_dragging = false;
  Vector2i m_drag_start;
};
