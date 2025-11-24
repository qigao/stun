#pragma once

#include <nanogui.h>
#include <nanogui/opengl.h>
#include <nanovg.h>
#include <string>
#include <vector>
#include <functional>

using namespace nanogui;

// Simple tool button widget
class ToolButtonWidget : public Widget {
public:
  ToolButtonWidget(Widget *parent, const std::string &icon, int id);
  
  void set_selected(bool selected) { m_selected = selected; }
  bool selected() const { return m_selected; }
  void set_callback(std::function<void()> callback) { m_callback = callback; }
  void set_is_lock_button(bool is_lock) { m_is_lock_button = is_lock; }
  void set_locked_state(bool locked) { m_locked_state = locked; }
  
  Vector2i preferred_size_impl(NVGcontext *) const override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  void draw(NVGcontext *ctx) override;

private:
  std::string m_icon;
  int m_id;
  bool m_selected = false;
  bool m_hovered = false;
  bool m_is_lock_button = false;
  bool m_locked_state = false;
  std::function<void()> m_callback;
};

class ToolbarPanelModule : public Widget {
public:
  ToolbarPanelModule(Widget *parent, Orientation orientation = Orientation::Horizontal);

  void set_orientation(Orientation orientation);
  void set_tool_callback(std::function<void(int)> callback) { m_tool_callback = callback; }
  void set_locked(bool locked);
  bool is_locked() const { return m_is_locked; }
  
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;
  void draw(NVGcontext *ctx) override;

private:
  void create_buttons();
  
  std::vector<ToolButtonWidget*> m_buttons;
  ToolButtonWidget *m_lock_button = nullptr;
  int m_selectedButton = 2;
  Orientation m_orientation;
  std::function<void(int)> m_tool_callback;
  bool m_is_locked = false;
  bool m_dragging = false;
  Vector2i m_drag_start;
};
