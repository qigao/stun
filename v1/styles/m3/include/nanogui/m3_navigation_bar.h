/*
    nanogui/m3_navigation_bar.h -- Material Design 3 Navigation Bar

    Implements M3 bottom navigation bar.

    Based on: https://m3.material.io/components/navigation-bar

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/m3_theme.h>
#include <vector>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3NavigationBar m3_navigation_bar.h nanogui/m3_navigation_bar.h
 *
 * \brief Material Design 3 Navigation Bar
 *
 * Bottom navigation bars allow movement between primary destinations in an app.
 */
class NANOGUI_EXPORT M3NavigationBar : public Widget {
public:
    /// Navigation item structure
    struct NavItem {
        std::string label;
        int icon = 0;
        bool enabled = true;
    };

    /**
     * \brief Construct an M3 navigation bar
     *
     * \param parent Parent widget
     * \param items Navigation items
     */
    M3NavigationBar(Widget *parent, const std::vector<NavItem> &items = {});

    /// Set items
    void set_items(const std::vector<NavItem> &items) { m_items = items; }

    /// Get items
    const std::vector<NavItem> &items() const { return m_items; }

    /// Set active item
    void set_active_item(int index);

    /// Get active item
    int active_item() const { return m_active_item; }

    /// Set callback
    void set_callback(const std::function<void(int)> &callback) { m_callback = callback; }

    /// Get callback
    const std::function<void(int)> &callback() const { return m_callback; }

    /// Handle mouse button events
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

    /// Handle mouse motion events
    bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;

    /// Draw the navigation bar
    void draw(NVGcontext *ctx) override;

    /// Preferred size
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;

    /// Get item at position
    int item_at_position(const Vector2i &p) const;

    std::vector<NavItem> m_items;
    int m_active_item = 0;
    int m_hover_item = -1;
    std::function<void(int)> m_callback;
};

NAMESPACE_END(nanogui)
