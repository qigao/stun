/*
    nanogui/fluent_app_bar.h -- Fluent Design App Bar widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentAppBar fluent_app_bar.h nanogui/fluent_app_bar.h
 *
 * \brief Fluent Design App Bar (Top App Bar) widget.
 *
 * A container for navigation and actions at the top of the screen.
 * Typically contains a title, navigation icon, and action buttons.
 */
class NANOGUI_EXPORT FluentAppBar : public Widget {
public:
    enum class Type {
        Regular,    // Standard height (64px)
        Medium,     // Medium height (112px)
        Large       // Large height (152px)
    };

    FluentAppBar(Widget *parent, const std::string &title = "", Type type = Type::Regular);

    const std::string &title() const { return m_title; }
    void set_title(const std::string &title) { m_title = title; }

    Type type() const { return m_type; }
    void set_type(Type type);

    int navigation_icon() const { return m_navigation_icon; }
    void set_navigation_icon(int icon) { m_navigation_icon = icon; }

    const std::function<void()> &navigation_callback() const { return m_navigation_callback; }
    void set_navigation_callback(const std::function<void()> &callback) { 
        m_navigation_callback = callback; 
    }

    /// Get the actions container widget for adding action buttons
    Widget *actions_container() { return m_actions_container; }

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

protected:
    std::string m_title;
    Type m_type;
    int m_navigation_icon;
    std::function<void()> m_navigation_callback;
    Widget *m_actions_container;
};

NAMESPACE_END(nanogui)
