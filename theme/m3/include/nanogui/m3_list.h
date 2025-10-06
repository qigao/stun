/*
    nanogui/m3_list.h -- Material Design 3 List

    Implements M3 list container.

    Based on: https://m3.material.io/components/lists

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3List m3_list.h nanogui/m3_list.h
 *
 * \brief Material Design 3 List
 *
 * Lists are continuous, vertical indexes of text or images.
 */
class NANOGUI_EXPORT M3List : public Widget {
public:
    /**
     * \brief Construct an M3 list
     *
     * \param parent Parent widget
     */
    M3List(Widget *parent);

    /// Draw the list
    void draw(NVGcontext *ctx) override;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;
};

/**
 * \class M3ListItem m3_list.h nanogui/m3_list.h
 *
 * \brief Material Design 3 List Item
 *
 * Individual items in a list.
 */
class NANOGUI_EXPORT M3ListItem : public Widget {
public:
    /**
     * \brief Construct an M3 list item
     *
     * \param parent Parent widget (usually M3List)
     * \param text Primary text
     * \param secondary_text Optional secondary text
     * \param icon Optional leading icon
     */
    M3ListItem(Widget *parent, const std::string &text = "",
               const std::string &secondary_text = "", int icon = 0);

    /// Set primary text
    void set_text(const std::string &text) { m_text = text; }

    /// Get primary text
    const std::string &text() const { return m_text; }

    /// Set secondary text
    void set_secondary_text(const std::string &text) { m_secondary_text = text; }

    /// Get secondary text
    const std::string &secondary_text() const { return m_secondary_text; }

    /// Set icon
    void set_icon(int icon) { m_icon = icon; }

    /// Get icon
    int icon() const { return m_icon; }

    /// Set callback
    void set_callback(const std::function<void()> &callback) { m_callback = callback; }

    /// Get callback
    const std::function<void()> &callback() const { return m_callback; }

    /// Handle mouse button events
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

    /// Draw the list item
    void draw(NVGcontext *ctx) override;

    /// Preferred size
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;

    std::string m_text;
    std::string m_secondary_text;
    int m_icon;
    std::function<void()> m_callback;
};

NAMESPACE_END(nanogui)
