/*
    nanogui/fluent_tooltip.h -- Fluent Design Tooltip widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>
#include <chrono>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentTooltip fluent_tooltip.h nanogui/fluent_tooltip.h
 *
 * \brief Fluent Design Tooltip widget.
 *
 * Displays informative text when hovering over an element.
 * Automatically positions itself relative to the anchor widget.
 */
class NANOGUI_EXPORT FluentTooltip : public Widget {
public:
    enum class Position {
        Top,
        Bottom,
        Left,
        Right,
        Auto  // Automatically choose best position
    };

    FluentTooltip(Widget *parent, const std::string &text);

    const std::string &text() const { return m_text; }
    void set_text(const std::string &text) { m_text = text; }

    Position position() const { return m_position; }
    void set_position(Position pos) { m_position = pos; }

    /// Set the anchor widget (widget to show tooltip for)
    void set_anchor(Widget *anchor) { m_anchor = anchor; }
    Widget *anchor() const { return m_anchor; }

    /// Show tooltip at anchor position
    void show_at_anchor();

    /// Hide tooltip
    void hide();

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

protected:
    std::string m_text;
    Position m_position;
    Widget *m_anchor;
    float m_animation_progress;
    std::chrono::steady_clock::time_point m_show_time;
};

NAMESPACE_END(nanogui)
