/*
    nanogui/m3_fab.h -- Material Design 3 Floating Action Button

    Implements M3 FAB with 3 sizes and proper elevation.

    Based on: https://m3.material.io/components/floating-action-button

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/button.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3FAB m3_fab.h nanogui/m3_fab.h
 *
 * \brief Material Design 3 Floating Action Button
 *
 * FABs are used for primary actions. They come in 3 sizes:
 * - Small: 40x40dp
 * - Regular: 56x56dp
 * - Large: 96x96dp
 */
class NANOGUI_EXPORT M3FAB : public Button {
public:
    /// FAB size variants
    enum class Size {
        Small,   ///< 40x40dp
        Regular, ///< 56x56dp (default)
        Large    ///< 96x96dp
    };

    /**
     * \brief Construct an M3 FAB
     *
     * \param parent Parent widget
     * \param icon Icon code (Font Awesome)
     * \param size FAB size
     */
    M3FAB(Widget *parent, int icon, Size size = Size::Regular);

    /// Set FAB size
    void set_size(Size size);

    /// Get FAB size
    Size size() const { return m_size; }

    /// Draw the FAB
    void draw(NVGcontext *ctx) override;

protected:
    /// Calculate preferred size
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;

    /// Get M3 theme
    M3Theme *m3_theme() const;

    /// Get size in pixels
    int get_pixel_size() const;

    /// Draw state layer
    void draw_state_layer(NVGcontext *ctx, float x, float y, float size);

    Size m_size;
};

NAMESPACE_END(nanogui)
