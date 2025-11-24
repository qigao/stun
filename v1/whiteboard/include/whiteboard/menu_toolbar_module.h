#pragma once

#include <nanogui.h>
#include <nanogui/opengl.h>
#include <nanovg.h>
#include <string>
#include <vector>
#include <functional>

using namespace nanogui;

class MenuToolbarModule : public Widget {
public:
  struct MenuItem {
    float x, y, width, height;
    std::string label;
    int icon;
    bool hovered;
    bool pressed;
    int id;
    std::function<void()> callback;
  };

  MenuToolbarModule(Widget *parent);

  Vector2i preferred_size_impl(NVGcontext *) const override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;
  bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;
  void draw(NVGcontext *ctx) override;

  void add_menu_item(const std::string &label, int icon, std::function<void()> callback);
  void add_separator();
  void clear_items();
  void layout_items();

private:
  void drawMenuItem(NVGcontext *ctx, float x, float y, float w, float h, const char *label,
                    int icon, bool hovered, bool pressed);
  void drawSeparator(NVGcontext *ctx, float x, float y, float h);
  int find_item_at(const Vector2i &pos);

  std::vector<MenuItem> m_items;
  std::vector<float> m_separator_positions;
  int m_hovered_item;
  int m_pressed_item;
  bool m_dragging;
  Vector2i m_drag_start;
};
