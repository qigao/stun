/*
    src/m3_progress_bar.cpp -- Material Design 3 Progress Bar implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_progress_bar.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3ProgressBar::M3ProgressBar(Widget *parent)
    : ProgressBar(parent) {
    set_fixed_height(4); // M3 progress bar height
}

M3Theme *M3ProgressBar::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3ProgressBar::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        ProgressBar::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float w = m_size.x();
    float h = m_size.y();

    nvgSave(ctx);

    // Determine colors
    Color track_color = theme->surface_variant();
    Color progress_color = theme->primary();

    // Draw track
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, h * 0.5f);
    nvgFillColor(ctx, track_color);
    nvgFill(ctx);

    // Draw progress
    float progress_width = w * m_value;
    if (progress_width > 0) {
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, progress_width, h, h * 0.5f);
        nvgFillColor(ctx, progress_color);
        nvgFill(ctx);
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
