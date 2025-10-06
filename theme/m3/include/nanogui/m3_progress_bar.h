/*
    nanogui/m3_progress_bar.h -- Material Design 3 Progress Bar

    Implements M3 linear progress indicator.

    Based on: https://m3.material.io/components/progress-indicators

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/progressbar.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3ProgressBar m3_progress_bar.h nanogui/m3_progress_bar.h
 *
 * \brief Material Design 3 Progress Bar
 *
 * Linear progress indicators display progress by animating along the length of a fixed track.
 */
class NANOGUI_EXPORT M3ProgressBar : public ProgressBar {
public:
    /**
     * \brief Construct an M3 progress bar
     *
     * \param parent Parent widget
     */
    M3ProgressBar(Widget *parent);

    /// Draw the progress bar
    void draw(NVGcontext *ctx) override;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;
};

NAMESPACE_END(nanogui)
