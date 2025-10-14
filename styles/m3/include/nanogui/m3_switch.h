/*
    nanogui/m3_switch.h -- Material Design 3 Switch component

    Implements M3 toggle switch with proper state layers and animations.

    Based on: https://m3.material.io/components/switch

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3Switch m3_switch.h nanogui/m3_switch.h
 *
 * \brief Material Design 3 Switch (toggle)
 *
 * A switch toggles the state of a single item on or off.
 */
class NANOGUI_EXPORT M3Switch : public Widget {
public:
    /**
     * \brief Construct an M3 switch
     *
     * \param parent Parent widget
     * \param checked Initial checked state
     */
    M3Switch(Widget *parent, bool checked = false);

    /// Set checked state
    void set_checked(bool checked) { m_checked = checked; }

    /// Get checked state
    bool checked() const { return m_checked; }

    /// Set callback for state changes
    void set_callback(const std::function<void(bool)> &callback) { m_callback = callback; }

    /// Get callback
    const std::function<void(bool)> &callback() const { return m_callback; }

    /// Handle mouse button events
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

    /// Draw the switch
    void draw(NVGcontext *ctx) override;

    /// Preferred size
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;

    bool m_checked;
    std::function<void(bool)> m_callback;
};

NAMESPACE_END(nanogui)
