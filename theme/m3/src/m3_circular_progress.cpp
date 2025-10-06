/*
    src/m3_circular_progress.cpp -- Material Design 3 Circular Progress implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_circular_progress.h>
#include <nanogui/opengl.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cmath>

NAMESPACE_BEGIN(nanogui)

M3CircularProgress::M3CircularProgress(Widget *parent, Size size)
    : Widget(parent), m_size(size) {
    int pixel_size = get_pixel_size();
    set_fixed_size(Vector2i(pixel_size, pixel_size));
}

M3Theme *M3CircularProgress::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3CircularProgress::set_size(Size size) {
    m_size = size;
    int pixel_size = get_pixel_size();
    set_fixed_size(Vector2i(pixel_size, pixel_size));
}

int M3CircularProgress::get_pixel_size() const {
    switch (m_size) {
        case Size::Small: return 24;
        case Size::Medium: return 48;
        case Size::Large: return 64;
        default: return 48;
    }
}

Vector2i M3CircularProgress::preferred_size(NVGcontext *) const {
    int pixel_size = get_pixel_size();
    return Vector2i(pixel_size, pixel_size);
}

void M3CircularProgress::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Widget::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float size = static_cast<float>(get_pixel_size());
    float cx = x + size * 0.5f;
    float cy = y + size * 0.5f;
    float radius = size * 0.5f - 4.0f; // 4dp stroke width
    float stroke_width = 4.0f;

    nvgSave(ctx);

    // Draw track
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, radius);
    nvgStrokeWidth(ctx, stroke_width);
    nvgStrokeColor(ctx, Color(theme->primary().r(), theme->primary().g(),
                              theme->primary().b(), 0.24f));
    nvgStroke(ctx);

    // Draw progress
    if (m_value >= 0.0f) {
        // Determinate progress
        float angle = m_value * 2.0f * static_cast<float>(M_PI);
        
        nvgBeginPath(ctx);
        nvgArc(ctx, cx, cy, radius, -static_cast<float>(M_PI) * 0.5f, 
               -static_cast<float>(M_PI) * 0.5f + angle, NVG_CW);
        nvgStrokeWidth(ctx, stroke_width);
        nvgStrokeColor(ctx, theme->primary());
        nvgStroke(ctx);
    } else {
        // Indeterminate progress (simplified - would need animation)
        float angle = static_cast<float>(M_PI) * 0.5f;
        
        nvgBeginPath(ctx);
        nvgArc(ctx, cx, cy, radius, 0, angle, NVG_CW);
        nvgStrokeWidth(ctx, stroke_width);
        nvgStrokeColor(ctx, theme->primary());
        nvgStroke(ctx);
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
