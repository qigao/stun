/*
    nanogui/fluent_fab.h -- Fluent Design Floating Action Button

    Circular button with high elevation, typically used for primary actions.

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/button.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentFAB fluent_fab.h nanogui/fluent_fab.h
 *
 * \brief Fluent Design Floating Action Button
 *
 * A circular button with high elevation used for primary actions.
 * FABs come in three sizes: regular (56x56), small (40x40), and large (96x96).
 */
class NANOGUI_EXPORT FluentFAB : public Button {
public:
    /// FAB size variants
    enum class Size {
        Small,   ///< 40x40 pixels
        Regular, ///< 56x56 pixels (default)
        Large    ///< 96x96 pixels
    };

    /**
     * \brief Construct a Floating Action Button
     *
     * \param parent
     *     Parent widget
     *
     * \param icon
     *     Icon to display (Font Awesome code)
     *
     * \param size
     *     FAB size variant
     */
    FluentFAB(Widget *parent, int icon, Size size = Size::Regular);

    /// Set FAB size
    void set_fab_size(Size size);

    /// Get FAB size
    Size fab_size() const { return m_fab_size; }

    /// Draw the FAB
    void draw(NVGcontext *ctx) override;

    /// Preferred size
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
    /// Draw elevation shadow
    void draw_shadow(NVGcontext *ctx, float cx, float cy, float radius);

    Size m_fab_size;
    float m_elevation = 3.f;
};

NAMESPACE_END(nanogui)
