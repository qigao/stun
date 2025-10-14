/*
    src/m3_slider.cpp -- Material Design 3 Slider implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_slider.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3Slider::M3Slider(Widget *parent)
    : Slider(parent) {
    set_fixed_height(20);
}

M3Theme *M3Slider::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3Slider::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Slider::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float w = m_size.x();
    float h = m_size.y();

    float center_y = y + h * 0.5f;
    float knob_radius = 10.0f;
    float track_height = 4.0f;

    nvgSave(ctx);

    // Calculate knob position
    float knob_x = x + (w - 2 * knob_radius) * m_value + knob_radius;

    // Determine colors
    Color active_color, inactive_color, knob_color;
    
    if (!m_enabled) {
        active_color = Color(theme->on_surface().r(), theme->on_surface().g(),
                            theme->on_surface().b(), 0.38f);
        inactive_color = Color(theme->on_surface().r(), theme->on_surface().g(),
                              theme->on_surface().b(), 0.12f);
        knob_color = Color(theme->on_surface().r(), theme->on_surface().g(),
                          theme->on_surface().b(), 0.38f);
    } else {
        active_color = theme->primary();
        inactive_color = theme->surface_variant();
        knob_color = theme->primary();
    }

    // Draw inactive track
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, center_y - track_height * 0.5f, 
                   w, track_height, track_height * 0.5f);
    nvgFillColor(ctx, inactive_color);
    nvgFill(ctx);

    // Draw active track
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, center_y - track_height * 0.5f,
                   knob_x - x, track_height, track_height * 0.5f);
    nvgFillColor(ctx, active_color);
    nvgFill(ctx);

    // Draw state layer if hovered or focused
    if ((m_mouse_focus || m_focused) && m_enabled) {
        Color state_color = theme->state_layer(theme->primary(), 0.08f);
        
        nvgBeginPath(ctx);
        nvgCircle(ctx, knob_x, center_y, 20.0f);
        nvgFillColor(ctx, state_color);
        nvgFill(ctx);
    }

    // Draw knob
    nvgBeginPath(ctx);
    nvgCircle(ctx, knob_x, center_y, knob_radius);
    nvgFillColor(ctx, knob_color);
    nvgFill(ctx);

    // Draw value label when dragging
    if (m_mouse_focus && m_enabled) {
        char value_str[16];
        snprintf(value_str, sizeof(value_str), "%.0f", m_value * 100);
        
        nvgFontSize(ctx, 12);
        nvgFontFace(ctx, "sans");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
        nvgFillColor(ctx, theme->on_surface());
        nvgText(ctx, knob_x, y - 5, value_str, nullptr);
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
