/*
    nanogui/fluent_badge.h -- Fluent Design Badge widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentBadge fluent_badge.h nanogui/fluent_badge.h
 *
 * \brief Fluent Design Badge widget.
 *
 * Badges display notification counts or status indicators on other widgets.
 * Can show numbers or just a dot.
 */
class NANOGUI_EXPORT FluentBadge : public Widget {
public:
    enum class Position {
        TopRight,
        TopLeft,
        BottomRight,
        BottomLeft
    };

    FluentBadge(Widget *parent, int count = 0);

    int count() const { return m_count; }
    void set_count(int count) { m_count = count; }

    bool dot_mode() const { return m_dot_mode; }
    void set_dot_mode(bool dot) { m_dot_mode = dot; }

    Position position() const { return m_position; }
    void set_position(Position pos) { m_position = pos; }

    const Color &badge_color() const { return m_badge_color; }
    void set_badge_color(const Color &color) { m_badge_color = color; }

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

protected:
    int m_count;
    bool m_dot_mode;
    Position m_position;
    Color m_badge_color;
};

NAMESPACE_END(nanogui)
