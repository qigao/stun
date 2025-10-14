/*
    nanogui/m3_radio.h -- Material Design 3 Radio Button

    Implements M3 radio button group for single selection.

    Based on: https://m3.material.io/components/radio-button

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
 * \class M3Radio m3_radio.h nanogui/m3_radio.h
 *
 * \brief Material Design 3 Radio Button
 *
 * Radio buttons allow users to select one option from a set.
 */
class NANOGUI_EXPORT M3Radio : public Widget {
public:
    /**
     * \brief Construct an M3 radio button group
     *
     * \param parent Parent widget
     * \param items Radio button labels
     * \param selected_index Initially selected index (-1 for none)
     */
    M3Radio(Widget *parent, const std::vector<std::string> &items = {},
            int selected_index = -1);

    /// Set items
    void set_items(const std::vector<std::string> &items) { m_items = items; }

    /// Get items
    const std::vector<std::string> &items() const { return m_items; }

    /// Set selected index
    void set_selected_index(int index);

    /// Get selected index
    int selected_index() const { return m_selected_index; }

    /// Set callback
    void set_callback(const std::function<void(int)> &callback) { m_callback = callback; }

    /// Get callback
    const std::function<void(int)> &callback() const { return m_callback; }

    /// Handle mouse button events
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

    /// Draw the radio buttons
    void draw(NVGcontext *ctx) override;

    /// Preferred size
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;

    /// Get radio button at position
    int radio_at_position(const Vector2i &p) const;

    std::vector<std::string> m_items;
    int m_selected_index;
    int m_hover_index = -1;
    std::function<void(int)> m_callback;
};

NAMESPACE_END(nanogui)
