/*
    nanogui/m3_divider.h -- Material Design 3 Divider

    Implements M3 divider for separating content.

    Based on: https://m3.material.io/components/divider

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3Divider m3_divider.h nanogui/m3_divider.h
 *
 * \brief Material Design 3 Divider
 *
 * Dividers separate content into clear groups.
 */
class NANOGUI_EXPORT M3Divider : public Widget {
public:
    /// Divider orientation
    enum class Orientation {
        Horizontal,  ///< Horizontal divider
        Vertical     ///< Vertical divider
    };

    /**
     * \brief Construct an M3 divider
     *
     * \param parent Parent widget
     * \param orientation Divider orientation
     */
    M3Divider(Widget *parent, Orientation orientation = Orientation::Horizontal);

    /// Set orientation
    void set_orientation(Orientation orientation) { m_orientation = orientation; }

    /// Get orientation
    Orientation orientation() const { return m_orientation; }

    /// Draw the divider
    void draw(NVGcontext *ctx) override;

    /// Preferred size
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;

    Orientation m_orientation;
};

NAMESPACE_END(nanogui)
