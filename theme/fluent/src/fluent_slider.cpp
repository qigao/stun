/*
    src/fluent_slider.cpp -- Fluent Design Slider implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_slider.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>
#include <sstream>
#include <iomanip>

NAMESPACE_BEGIN(nanogui)

FluentSlider::FluentSlider(Widget *parent)
    : Slider(parent), m_discrete(false), m_steps(10), 
      m_show_value(true), m_mouse_over(false) {
}

bool FluentSlider::mouse_enter_event(const Vector2i &p, bool enter) {
    m_mouse_over = enter;
    return Slider::mouse_enter_event(p, enter);
}

void FluentSlider::draw(NVGcontext *ctx) {
    float center_x = m_pos.x() + m_size.x() * 0.5f;
    float center_y = m_pos.y() + m_size.y() * 0.5f;
    float kr = (int)(m_size.y() * 0.5f);
    float kshadow = 3;

    float start_x = center_x - (m_size.x() - kr * 2.f) * 0.5f;
    float width_x = m_size.x() - kr * 2.f;

    float knob_pos_x = start_x + m_value * width_x;
    float knob_pos_y = center_y;

    nvgSave(ctx);

    // Draw track background
    NVGpaint track_bg = nvgBoxGradient(
        ctx, start_x, center_y - 2.f, width_x, 4.f, 2.f, 2.f,
        nvgRGBAf(0.f, 0.f, 0.f, 0.12f), nvgRGBAf(0.f, 0.f, 0.f, 0.12f));

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, start_x, center_y - 2.f, width_x, 4.f, 2.f);
    nvgFillPaint(ctx, track_bg);
    nvgFill(ctx);

    // Draw active track (filled portion)
    float active_width = m_value * width_x;
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, start_x, center_y - 2.f, active_width, 4.f, 2.f);
    nvgFillColor(ctx, nvgRGBAf(0.25f, 0.7f, 1.f, 1.f)); // Primary color
    nvgFill(ctx);

    // Draw discrete step markers if enabled
    if (m_discrete && m_steps > 1) {
        for (int i = 0; i <= m_steps; i++) {
            float step_x = start_x + (width_x * i) / m_steps;
            bool is_active = (i / (float)m_steps) <= m_value;
            
            nvgBeginPath(ctx);
            nvgCircle(ctx, step_x, center_y, 2.f);
            nvgFillColor(ctx, is_active ? 
                nvgRGBAf(0.25f, 0.7f, 1.f, 1.f) : 
                nvgRGBAf(0.f, 0.f, 0.f, 0.26f));
            nvgFill(ctx);
        }
    }

    // Draw knob shadow
    NVGpaint shadow_paint = nvgRadialGradient(
        ctx, knob_pos_x, knob_pos_y + 1.f, kr - kshadow, kr + kshadow,
        nvgRGBAf(0.f, 0.f, 0.f, 0.3f), nvgRGBAf(0.f, 0.f, 0.f, 0.f));

    nvgBeginPath(ctx);
    nvgCircle(ctx, knob_pos_x, knob_pos_y, kr + kshadow);
    nvgFillPaint(ctx, shadow_paint);
    nvgFill(ctx);

    // Draw knob
    float knob_radius = m_mouse_over ? kr * 1.2f : kr;
    
    nvgBeginPath(ctx);
    nvgCircle(ctx, knob_pos_x, knob_pos_y, knob_radius);
    nvgFillColor(ctx, nvgRGBAf(0.25f, 0.7f, 1.f, 1.f)); // Primary color
    nvgFill(ctx);

    // Draw value label on hover
    if (m_show_value && m_mouse_over) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << (m_value * 100.f);
        std::string value_text = oss.str();
        
        nvgFontSize(ctx, 12.f);
        nvgFontFace(ctx, "sans");
        
        float text_width = nvgTextBounds(ctx, 0, 0, value_text.c_str(), nullptr, nullptr);
        float label_w = text_width + 12.f;
        float label_h = 24.f;
        float label_x = knob_pos_x - label_w * 0.5f;
        float label_y = knob_pos_y - kr - label_h - 8.f;
        
        // Draw label background
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, label_x, label_y, label_w, label_h, 4.f);
        nvgFillColor(ctx, nvgRGBAf(0.4f, 0.4f, 0.4f, 0.9f));
        nvgFill(ctx);
        
        // Draw label text
        nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(ctx, label_x + label_w * 0.5f, label_y + label_h * 0.5f, 
                value_text.c_str(), nullptr);
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
