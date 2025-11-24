/*
    nanogui/m3_menu.h -- Material Design 3 Menu

    Implements M3 menu for displaying options.

    Based on: https://m3.material.io/components/menus

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/popup.h>
#include <nanogui/m3_theme.h>
#include <vector>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3Menu m3_menu.h nanogui/m3_menu.h
 *
 * \brief Material Design 3 Menu
 *
 * Menus display a list of choices on temporary surfaces.
 */
class NANOGUI_EXPORT M3Menu : public Popup {
public:
    /// Anchor position for menu placement
    enum class AnchorPosition {
        TOP_START,      ///< Align top-left with anchor
        TOP_END,        ///< Align top-right with anchor
        BOTTOM_START,   ///< Align bottom-left with anchor (default)
        BOTTOM_END      ///< Align bottom-right with anchor
    };

    /// Menu item structure
    struct MenuItem {
        std::string label;
        int icon = 0;
        bool enabled = true;
        bool divider = false;
        std::string trailing_text;
        std::function<void()> callback;
        M3Menu *submenu = nullptr;
    };

    /**
     * \brief Construct an M3 menu
     *
     * \param parent Parent widget
     * \param anchor Anchor widget (button that opens menu)
     */
    M3Menu(Widget *parent, Widget *anchor = nullptr);

    /// Add menu item
    void add_item(const std::string &label, const std::function<void()> &callback = nullptr,
                  int icon = 0, bool enabled = true);

    /// Add menu item with trailing text (e.g., keyboard shortcut)
    void add_item_with_trailing(const std::string &label, const std::string &trailing,
                                const std::function<void()> &callback = nullptr,
                                int icon = 0, bool enabled = true);

    /// Add submenu item
    void add_submenu(const std::string &label, M3Menu *submenu, int icon = 0);

    /// Add divider
    void add_divider();

    /// Clear all items
    void clear_items() { m_items.clear(); }

    /// Get items
    const std::vector<MenuItem> &items() const { return m_items; }

    /// Set anchor position
    void set_anchor_position(AnchorPosition pos) { m_anchor_position = pos; }

    /// Get anchor position
    AnchorPosition anchor_position() const { return m_anchor_position; }

    /// Show menu at anchor widget position
    void show_at(Widget *anchor);

    /// Show menu at absolute position
    void show_at_position(const Vector2i &pos);

    /// Override set_visible to close submenus when menu is hidden
    void set_visible(bool visible);

    /// Handle mouse button events
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

    /// Handle mouse motion events
    bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;

    /// Handle keyboard events
    bool keyboard_event(int key, int scancode, int action, int modifiers) override;

    /// Draw the menu
    void draw(NVGcontext *ctx) override;

    /// Perform layout
    void perform_layout(NVGcontext *ctx) override;

    /// Preferred size
    Vector2i preferred_size(NVGcontext *ctx) const;
    
    // Accessibility support
    
    /// Get accessibility role (always "menu")
    virtual const char* accessibility_role() const { return "menu"; }
    
    /// Get accessibility role for a menu item
    const char* item_accessibility_role(int index) const;
    
    /// Check if item has submenu (for aria-haspopup)
    bool item_has_submenu(int index) const;
    
    /// Check if submenu is expanded (for aria-expanded)
    bool submenu_is_expanded(int index) const;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;

    /// Get menu item at position
    int item_at_position(const Vector2i &p) const;

    /// Reposition menu if it extends beyond screen bounds
    void reposition_if_needed();

    /// Set focus to a specific item
    void focus_item(int index);

    /// Activate the item at the given index
    void activate_item(int index);

    /// Open submenu at the given item index
    void open_submenu(int index);

    /// Close currently open submenu
    void close_submenu();

    std::vector<MenuItem> m_items;
    std::vector<M3Menu*> m_owned_submenus; // Store submenu instances for cleanup
    int m_hover_item = -1;
    int m_focused_item = -1;
    AnchorPosition m_anchor_position = AnchorPosition::BOTTOM_START;
    M3Menu *m_open_submenu = nullptr;
    int m_submenu_item_index = -1;
    float m_submenu_hover_time = 0.0f;
    float m_submenu_leave_time = 0.0f;
    
    // Animation state
    float m_opacity = 0.0f;              // Current opacity (0.0 to 1.0)
    bool m_animating = false;            // Whether animation is in progress
    bool m_fading_in = false;            // True for fade-in, false for fade-out
    float m_animation_time = 0.0f;       // Current animation time
    float m_animation_duration = 0.0f;   // Animation duration in seconds
    
    /// Update animation state (called each frame)
    void update_animation(float dt);
    
    /// Start show animation
    void animate_show();
    
    /// Start hide animation
    void animate_hide();
};

NAMESPACE_END(nanogui)
