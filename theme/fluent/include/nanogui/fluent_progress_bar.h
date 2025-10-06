/*
    nanogui/fluent_progress_bar.h -- Fluent Design Progress Bar widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>
#include <chrono>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentProgressBar fluent_progress_bar.h nanogui/fluent_progress_bar.h
 *
 * \brief Fluent Design Linear Progress Bar widget.
 *
 * Shows progress as a horizontal bar. Supports both determinate and
 * indeterminate modes.
 */
class NANOGUI_EXPORT FluentProgressBar : public Widget {
public:
    FluentProgressBar(Widget *parent);

    float value() const { return m_value; }
    void set_value(float value) { m_value = std::max(0.f, std::min(1.f, value)); }

    bool indeterminate() const { return m_indeterminate; }
    void set_indeterminate(bool indeterminate) { m_indeterminate = indeterminate; }

    const Color &color() const { return m_color; }
    void set_color(const Color &color) { m_color = color; }

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

protected:
    float m_value;
    bool m_indeterminate;
    Color m_color;
    std::chrono::steady_clock::time_point m_start_time;
};

NAMESPACE_END(nanogui)
