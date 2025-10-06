/*
    nanogui/fluent_circular_progress.h -- Fluent Design Circular Progress Indicator

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>
#include <chrono>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentCircularProgress fluent_circular_progress.h nanogui/fluent_circular_progress.h
 *
 * \brief Fluent Design Circular Progress Indicator.
 *
 * Shows progress as a circular arc. Supports both determinate (with value) and
 * indeterminate (spinning) modes.
 */
class NANOGUI_EXPORT FluentCircularProgress : public Widget {
public:
    enum class Size {
        Small,   // 24px
        Medium,  // 40px
        Large    // 56px
    };

    FluentCircularProgress(Widget *parent, Size size = Size::Medium);

    /// Get/set progress value (0.0 to 1.0)
    float value() const { return m_value; }
    void set_value(float value) { m_value = std::max(0.f, std::min(1.f, value)); }

    /// Get/set indeterminate mode (spinning animation)
    bool indeterminate() const { return m_indeterminate; }
    void set_indeterminate(bool indeterminate) { m_indeterminate = indeterminate; }

    /// Get/set size
    Size size() const { return m_size; }
    void set_size(Size size);

    /// Get/set color
    const Color &color() const { return m_color; }
    void set_color(const Color &color) { m_color = color; }

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

protected:
    float m_value;
    bool m_indeterminate;
    Size m_size;
    Color m_color;
    std::chrono::steady_clock::time_point m_start_time;
};

NAMESPACE_END(nanogui)
