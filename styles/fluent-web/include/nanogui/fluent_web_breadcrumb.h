#pragma once

#include <nanogui/widget.h>
#include <nanogui/vector.h>

#include <functional>
#include <string>
#include <vector>

NAMESPACE_BEGIN(nanogui)

class FluentWebTheme;

/**
 * Fluent 2 breadcrumb navigation component.
 */
class NANOGUI_EXPORT FluentWebBreadcrumb : public Widget {
public:
  struct Item {
    std::string label;
    int icon;
    bool clickable;

    Item(const std::string &lbl, int ic = 0, bool click = true)
        : label(lbl), icon(ic), clickable(click) {}
  };

  explicit FluentWebBreadcrumb(Widget *parent);

  void add_item(const std::string &label, int icon = 0, bool clickable = true);
  void set_items(const std::vector<Item> &items);
  void clear_items();
  const std::vector<Item> &items() const { return m_items; }

  void set_item_callback(const std::function<void(int)> &cb) { m_item_callback = cb; }

  void set_theme(Theme *theme) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  void draw(NVGcontext *ctx) override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button,
                          int modifiers) override;

protected:
  void refresh_tokens();
  int item_at_position(const Vector2i &p) const;

  std::vector<Item> m_items;
  std::function<void(int)> m_item_callback;
  int m_hover_index;

  Color m_text_color;
  Color m_hover_color;
  Color m_disabled_color;
  Color m_separator_color;
  int m_item_spacing;
  int m_icon_size;
  float m_font_size;
};

NAMESPACE_END(nanogui)
