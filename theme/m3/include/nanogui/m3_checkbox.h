/*
    nanogui/m3_checkbox.h -- Material Design 3 Checkbox component

    Implements M3 checkbox with proper state layers.

    Based on: https://m3.material.io/components/checkbox

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3Checkbox m3_checkbox.h nanogui/m3_checkbox.h
 *
 * \brief Material Design 3 Checkbox
 *
 * Checkboxes allow users to select one or more items from a set.
 */
class NANOGUI_EXPORT M3Checkbox : public Widget {
public:
    /**
     * \brief Construct an M3 checkbox
     *
     * \param parent Parent widget
     * \param caption Label text
     * \param checked Initial checked state
     */
    M3Checkbox(Widget *parent, const std::string &caption = "",
               bool checked = false);

    /// Set checked state
    void set_checked(bool checked) { m_checked = checked; }

    /// Get checked state
    bool checked() const { return m_checked; }

    /// Set caption
    void set_caption(const std::string &caption) { m_caption = caption; }

    /// Get caption
    const std::string &caption() const { return m_caption; }

    /// Set callback
    void set_callback(const std::function<void(bool)> &callback) { m_callback = callback; }

    /// Get callback
    const std::function<void(bool)> &callback() const { return m_callback; }

    /// Handle mouse button events
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

    /// Draw the checkbox
    void draw(NVGcontext *ctx) override;

    /// Preferred size
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;

    std::string m_caption;
    bool m_checked;
    std::function<void(bool)> m_callback;
};

NAMESPACE_END(nanogui)
