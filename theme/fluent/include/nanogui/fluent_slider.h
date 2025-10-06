/*
    nanogui/fluent_slider.h -- Fluent Design Slider widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/slider.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentSlider fluent_slider.h nanogui/fluent_slider.h
 *
 * \brief Fluent Design Slider widget.
 *
 * Enhanced slider with Fluent Design styling, including:
 * - Larger touch target
 * - Value label on hover
 * - Smooth animations
 * - Discrete mode with step markers
 */
class NANOGUI_EXPORT FluentSlider : public Slider {
public:
    FluentSlider(Widget *parent);

    /// Enable/disable discrete mode (shows step markers)
    bool discrete() const { return m_discrete; }
    void set_discrete(bool discrete) { m_discrete = discrete; }

    /// Number of steps for discrete mode
    int steps() const { return m_steps; }
    void set_steps(int steps) { m_steps = steps; }

    /// Show value label on hover
    bool show_value() const { return m_show_value; }
    void set_show_value(bool show) { m_show_value = show; }

    void draw(NVGcontext *ctx) override;
    bool mouse_enter_event(const Vector2i &p, bool enter) override;

protected:
    bool m_discrete;
    int m_steps;
    bool m_show_value;
    bool m_mouse_over;
};

NAMESPACE_END(nanogui)
