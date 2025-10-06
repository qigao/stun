/*
    src/fluent_circular_progress.cpp -- Fluent Design Circular Progress implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_circular_progress.h>
#include <chrono>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>
#include <cmath>

NAMESPACE_BEGIN(nanogui)

FluentCircularProgress::FluentCircularProgress(Widget *parent, Size size)
    : Widget(parent), m_value(0.f), m_indeterminate(false), m_size(size),
      m_color(0.25f, 0.7f, 1.f, 1.f) {
    m_start_time = std::chrono::steady_clock::now();
    set_size(size);
}

void FluentCircularProgress::set_size(Size size) {
    m_size = size;
    
    int widget_size = 40; // Default to Medium
    switch (size) {
        case Size::Small:
            widget_size = 24;
            break;
        case Size::Medium:
            widget_size = 40;
            break;
        case Size::Large:
            widget_size = 56;
            break;
    }
    
    set_fixed_size(Vector2i(widget_size, widget_size));
}

Vector2i FluentCircularProgress::preferred_size_impl(NVGcontext *) const {
    switch (m_size) {
        case Size::Small:
            return Vector2i(24, 24);
        case Size::Medium:
            return Vector2i(40, 40);
        case Size::Large:
            return Vector2i(56, 56);
    }
    return Vector2i(40, 40);
}

void FluentCircularProgress::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float widget_size = static_cast<float>(Widget::size().x());
    
    float cx = x + widget_size * 0.5f;
    float cy = y + widget_size * 0.5f;
    float radius = widget_size * 0.4f;
    float stroke_width = widget_size * 0.1f;
    
    nvgSave(ctx);
    
    if (m_indeterminate) {
        // Indeterminate mode - spinning animation
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time).count();
        
        // Rotation speed: 1 full rotation per 1.4 seconds
        float rotation = (elapsed % 1400) / 1400.f * 2.f * NVG_PI;
        
        // Arc length oscillates between 0.1 and 0.75 of circle
        float arc_phase = (elapsed % 2000) / 2000.f * 2.f * NVG_PI;
        float arc_length = 0.1f + 0.65f * (0.5f + 0.5f * std::sin(arc_phase));
        
        // Draw background circle (track)
        nvgBeginPath(ctx);
        nvgCircle(ctx, cx, cy, radius);
        nvgStrokeWidth(ctx, stroke_width);
        nvgStrokeColor(ctx, nvgRGBAf(m_color.r(), m_color.g(), m_color.b(), 0.2f));
        nvgStroke(ctx);
        
        // Draw animated arc
        float start_angle = rotation - NVG_PI * 0.5f;
        float end_angle = start_angle + arc_length * 2.f * NVG_PI;
        
        nvgBeginPath(ctx);
        nvgArc(ctx, cx, cy, radius, start_angle, end_angle, NVG_CW);
        nvgStrokeWidth(ctx, stroke_width);
        nvgStrokeColor(ctx, nvgRGBAf(m_color.r(), m_color.g(), m_color.b(), m_color.w()));
        nvgStroke(ctx);
        
    } else {
        // Determinate mode - show progress value
        
        // Draw background circle (track)
        nvgBeginPath(ctx);
        nvgCircle(ctx, cx, cy, radius);
        nvgStrokeWidth(ctx, stroke_width);
        nvgStrokeColor(ctx, nvgRGBAf(m_color.r(), m_color.g(), m_color.b(), 0.2f));
        nvgStroke(ctx);
        
        // Draw progress arc
        if (m_value > 0.f) {
            float start_angle = -NVG_PI * 0.5f; // Start at top
            float end_angle = start_angle + m_value * 2.f * NVG_PI;
            
            nvgBeginPath(ctx);
            nvgArc(ctx, cx, cy, radius, start_angle, end_angle, NVG_CW);
            nvgStrokeWidth(ctx, stroke_width);
            nvgStrokeColor(ctx, nvgRGBAf(m_color.r(), m_color.g(), m_color.b(), m_color.w()));
            nvgStroke(ctx);
        }
    }
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
