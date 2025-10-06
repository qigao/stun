/*
    nanogui/fluent_divider.h -- Fluent Design Divider widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentDivider fluent_divider.h nanogui/fluent_divider.h
 *
 * \brief Fluent Design Divider widget.
 *
 * A thin line that groups content in lists and layouts.
 * Can be horizontal or vertical.
 */
class NANOGUI_EXPORT FluentDivider : public Widget {
public:
    enum class Orientation {
        Horizontal,
        Vertical
    };

    FluentDivider(Widget *parent, Orientation orientation = Orientation::Horizontal);

    Orientation orientation() const { return m_orientation; }
    void set_orientation(Orientation orientation) { m_orientation = orientation; }

    const Color &color() const { return m_color; }
    void set_color(const Color &color) { m_color = color; }

    int thickness() const { return m_thickness; }
    void set_thickness(int thickness) { m_thickness = thickness; }

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

protected:
    Orientation m_orientation;
    Color m_color;
    int m_thickness;
};

NAMESPACE_END(nanogui)
