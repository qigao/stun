/**
 * \file context_menu_module.h
 * \brief Context menu for right-click operations on canvas objects.
 */

#pragma once

#include <nanogui/widget.h>
#include <nanovg.h>
#include <functional>
#include <string>
#include <vector>

namespace whiteboard {

/**
 * \struct MenuItem
 * \brief Represents a single item in the context menu.
 */
struct MenuItem {
  std::string label;                    // Display text
  std::string shortcut;                 // Keyboard shortcut text (e.g., "Ctrl+C")
  int icon;                             // Font Awesome icon code (0 for none)
  std::function<void()> callback;       // Action to execute
  bool enabled;                         // Whether item is clickable
  bool separator;                       // Whether this is a separator line
  
  MenuItem(const std::string &lbl = "", const std::string &sc = "", int ic = 0,
           std::function<void()> cb = nullptr, bool en = true, bool sep = false)
      : label(lbl), shortcut(sc), icon(ic), callback(cb), enabled(en), separator(sep) {}
  
  // Factory method for separator
  static MenuItem Separator() {
    return MenuItem("", "", 0, nullptr, false, true);
  }
};

/**
 * \class ContextMenuModule
 * \brief A right-click context menu with customizable items.
 *
 * Features:
 * - Custom NanoVG rendering
 * - Icon support (Font Awesome)
 * - Keyboard shortcut display
 * - Hover highlighting
 * - Disabled item graying
 * - Separator lines
 * - Auto-close on selection or outside click
 */
class ContextMenuModule : public nanogui::Widget {
public:
  /**
   * \brief Constructor.
   * \param parent Parent widget
   */
  ContextMenuModule(nanogui::Widget *parent);

  /**
   * \brief Show the context menu at a specific position.
   * \param pos Position in screen coordinates
   * \param items Menu items to display
   */
  void show_at(const nanogui::Vector2i &pos, const std::vector<MenuItem> &items);

  /**
   * \brief Hide the context menu.
   */
  void hide();

  /**
   * \brief Check if menu is currently visible.
   */
  bool is_visible() const { return m_visible; }

  /**
   * \brief Draw the context menu.
   * \param ctx NanoVG context
   */
  void draw(NVGcontext *ctx) override;

  /**
   * \brief Handle mouse button events.
   */
  bool mouse_button_event(const nanogui::Vector2i &p, int button, bool down,
                          int modifiers) override;

  /**
   * \brief Handle mouse motion events.
   */
  bool mouse_motion_event(const nanogui::Vector2i &p, const nanogui::Vector2i &rel,
                          int button, int modifiers) override;

  /**
   * \brief Handle keyboard events.
   */
  bool keyboard_event(int key, int scancode, int action, int modifiers) override;

private:
  std::vector<MenuItem> m_items;        // Menu items
  int m_hovered_item;                   // Index of hovered item (-1 for none)
  bool m_visible;                       // Visibility state
  
  // Layout constants
  static constexpr float ITEM_HEIGHT = 28.0f;
  static constexpr float SEPARATOR_HEIGHT = 9.0f;
  static constexpr float PADDING = 8.0f;
  static constexpr float ICON_SIZE = 16.0f;
  static constexpr float ICON_SPACING = 8.0f;
  static constexpr float SHORTCUT_SPACING = 24.0f;
  static constexpr float MIN_WIDTH = 180.0f;
  static constexpr float CORNER_RADIUS = 6.0f;
  static constexpr float SHADOW_SIZE = 8.0f;

  /**
   * \brief Calculate menu dimensions.
   * \param ctx NanoVG context
   * \param width Output width
   * \param height Output height
   */
  void calculate_size(NVGcontext *ctx, float &width, float &height) const;

  /**
   * \brief Find which item is at a given position.
   * \param p Position relative to menu
   * \return Item index or -1 if none
   */
  int item_at_position(const nanogui::Vector2i &p) const;

  /**
   * \brief Execute the callback for an item.
   * \param index Item index
   */
  void execute_item(int index);
};

} // namespace whiteboard
