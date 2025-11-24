#pragma once

#include <nanogui/fluent_web_tooltip.h>
#include <nanogui/widget.h>
#include <nanogui/vector.h>

#include <functional>
#include <string>
#include <vector>

NAMESPACE_BEGIN(nanogui)

class FluentWebTheme;
class FluentWebButton;

/**
 * Fluent-styled contextual menu.
 *
 * Presents vertical command lists with Fluent 2 colors and spacing.
 * Supports keyboard navigation (Up/Down/Enter/Escape) and optional
 * keyboard shortcut labels along the right edge.
 */
class NANOGUI_EXPORT FluentWebMenu : public FluentWebPopover {
public:
  struct Item {
    std::string label;
    std::string shortcut;
    int icon;
    bool enabled;
    bool separator;
    std::function<void()> callback;
  };

  explicit FluentWebMenu(Widget *parent, Window *parent_window = nullptr);

  /// Adds an actionable item. Returns its index.
  int add_item(const std::string &label, const std::function<void()> &callback,
               int icon = 0, const std::string &shortcut = {}, bool enabled = true);

  /// Adds a visual divider between sections.
  void add_separator();

  /// Clears all menu items.
  void clear_items();

  /// Shows the menu aligned to the provided anchor widget.
  void show_for_anchor(Widget *anchor);

  /// Hides the menu, leaving it instantiated for reuse.
  void dismiss();

  void set_theme(Theme *theme) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  bool keyboard_event(int key, int scancode, int action, int modifiers) override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

protected:
  class MenuItemWidget;
  class SeparatorWidget;

  void refresh_tokens();
  void update_layout_metrics();
  void rebuild_item_widgets();
  void ensure_highlight_valid();
  void set_highlight(int index);
  void activate(int index);
  int next_enabled_index(int start, int delta) const;
  void update_item_states();

  std::vector<Item> m_items;
  std::vector<MenuItemWidget *> m_item_widgets;
  Widget *m_anchor;
  int m_highlight_index;
  bool m_close_on_selection;

  // Cached Fluent styling tokens
  Color m_item_text;
  Color m_item_disabled_text;
  Color m_item_shortcut_text;
  Color m_item_bg_hover;
  Color m_item_bg_active;
  Color m_separator_color;
  float m_separator_thickness;
  int m_item_height;
  int m_item_padding;
  int m_item_gap;
  int m_menu_margin;
};

/**
 * Horizontal Fluent menu bar hosting drop-down menus.
 *
 * Each menu button spawns a FluentWebMenu configured with Fluent tokens.
 */
class NANOGUI_EXPORT FluentWebMenuBar : public Widget {
public:
  explicit FluentWebMenuBar(Widget *parent);

  /// Creates a menu button and returns the associated menu for population.
  FluentWebMenu *add_menu(const std::string &label);

  /// Removes all menus.
  void clear_menus();

  void set_theme(Theme *theme) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  void perform_layout(NVGcontext *ctx) override;
  void draw(NVGcontext *ctx) override;

protected:
  struct MenuEntry {
    FluentWebButton *button;
    FluentWebMenu *menu;
  };

  void refresh_tokens();
  void close_open_menu();
  void handle_button_activation(FluentWebMenu *menu);

  std::vector<MenuEntry> m_menus;
  FluentWebMenu *m_open_menu;
  Color m_background;
  Color m_border;
  int m_padding;
  int m_spacing;
};

NAMESPACE_END(nanogui)
