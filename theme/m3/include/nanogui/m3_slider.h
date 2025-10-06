/*
    nanogui/m3_slider.h -- Material Design 3 Slider component

    Implements M3 slider with proper state layers and value display.

    Based on: https://m3.material.io/components/sliders

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/slider.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3Slider m3_slider.h nanogui/m3_slider.h
 *
 * \brief Material Design 3 Slider
 *
 * Sliders allow users to make selections from a range of values.
 */
class NANOGUI_EXPORT M3Slider : public Slider {
public:
    /**
     * \brief Construct an M3 slider
     *
     * \param parent Parent widget
     */
    M3Slider(Widget *parent);

    /// Draw the slider
    void draw(NVGcontext *ctx) override;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;
};

NAMESPACE_END(nanogui)
