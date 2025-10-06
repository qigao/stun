/*
    nanogui/fluent_card.h -- Fluent Design card container

    Fluent Design card with elevation, rounded corners, and proper shadows.

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentCard fluent_card.h nanogui/fluent_card.h
 *
 * \brief Fluent Design card container
 *
 * A card is a surface that displays content and actions on a single topic.
 * Cards should be easy to scan for relevant and actionable information.
 */
class NANOGUI_EXPORT FluentCard : public Widget {
public:
    /// Card elevation levels (Fluent Design)
    enum class Elevation {
        Level0 = 0,  ///< No elevation (flat)
        Level1 = 1,  ///< Subtle elevation
        Level2 = 2,  ///< Default elevation
        Level3 = 3,  ///< Raised elevation
        Level4 = 4,  ///< High elevation
        Level5 = 5   ///< Maximum elevation
    };

    /**
     * \brief Construct a Fluent Design card
     *
     * \param parent
     *     Parent widget
     *
     * \param elevation
     *     Card elevation level (affects shadow depth)
     */
    FluentCard(Widget *parent, Elevation elevation = Elevation::Level2);

    /// Set the card elevation
    void set_elevation(Elevation elevation);

    /// Get the current elevation
    Elevation elevation() const { return m_elevation; }

    /// Set corner radius (default: 12)
    void set_corner_radius(float radius) { m_corner_radius = radius; }

    /// Get corner radius
    float corner_radius() const { return m_corner_radius; }

    /// Set background color (overrides theme)
    void set_background_color(const Color &color) { m_background_color = color; }

    /// Get background color
    const Color &background_color() const { return m_background_color; }

    /// Set padding
    void set_padding(int padding) { m_padding = padding; }

    /// Get padding
    int padding() const { return m_padding; }

    /// Draw the card
    void draw(NVGcontext *ctx) override;

    /// Perform layout
    void perform_layout(NVGcontext *ctx) override;

protected:
    /// Draw elevation shadow
    void draw_shadow(NVGcontext *ctx, float x, float y, float w, float h);

    Elevation m_elevation;
    float m_corner_radius = 12.f;
    Color m_background_color;
    int m_padding = 16;
};

NAMESPACE_END(nanogui)
