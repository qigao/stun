/*
    nanogui/m3_tabs.h -- Material Design 3 Tabs

    Implements M3 tabs for navigation between views.

    Based on: https://m3.material.io/components/tabs

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
 * \class M3Tabs m3_tabs.h nanogui/m3_tabs.h
 *
 * \brief Material Design 3 Tabs
 *
 * Tabs organize content across different screens and views.
 */
class NANOGUI_EXPORT M3Tabs : public Widget {
public:
    /**
     * \brief Construct M3 tabs
     *
     * \param parent Parent widget
     * \param tab_names Tab labels
     */
    M3Tabs(Widget *parent, const std::vector<std::string> &tab_names = {});

    /// Set tab names
    void set_tab_names(const std::vector<std::string> &names) { m_tab_names = names; }

    /// Get tab names
    const std::vector<std::string> &tab_names() const { return m_tab_names; }

    /// Set active tab
    void set_active_tab(int index);

    /// Get active tab
    int active_tab() const { return m_active_tab; }

    /// Set callback
    void set_callback(const std::function<void(int)> &callback) { m_callback = callback; }

    /// Get callback
    const std::function<void(int)> &callback() const { return m_callback; }

    /// Handle mouse button events
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

    /// Handle mouse motion events
    bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;

    /// Draw the tabs
    void draw(NVGcontext *ctx) override;

    /// Preferred size
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;

    /// Get tab at position
    int tab_at_position(const Vector2i &p) const;

    /// Calculate tab positions
    std::vector<float> calculate_tab_positions(NVGcontext *ctx) const;

    std::vector<std::string> m_tab_names;
    int m_active_tab = 0;
    int m_hover_tab = -1;
    std::function<void(int)> m_callback;
};

NAMESPACE_END(nanogui)
