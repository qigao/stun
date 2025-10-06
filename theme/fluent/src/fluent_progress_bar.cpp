/*
    src/fluent_progress_bar.cpp -- Fluent Design Progress Bar implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_progress_bar.h>
#include <chrono>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentProgressBar::FluentProgressBar(Widget *parent)
    : Widget(parent), m_value(0.f), m_indeterminate(false),
      m_color(0.25f, 0.7f, 1.f, 1.f) {
    m_start_time = std::chrono::steady_clock::now();
}

Vector2i FluentProgressBar::preferred_size_impl(NVGcontext *) const {
    return Vector2i(0, 4); // Height 4px, width fills parent
}

void FluentProgressBar::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(Widget::size().x());
    float h = static_cast<float>(Widget::size().y());
    
    nvgSave(ctx);
    
    // Draw track background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, h * 0.5f);
    nvgFillColor(ctx, nvgRGBAf(m_color.r(), m_color.g(), m_color.b(), 0.2f));
    nvgFill(ctx);
    
    if (m_indeterminate) {
        // Indeterminate mode - animated bar
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time).count();
        
        // Animation: bar moves across and loops
        float cycle = 2000.f; // 2 second cycle
        float progress = (elapsed % (int)cycle) / cycle;
        
        float bar_width = w * 0.3f;
        float bar_x = x + (w - bar_width) * progress;
        
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, bar_x, y, bar_width, h, h * 0.5f);
        nvgFillColor(ctx, nvgRGBAf(m_color.r(), m_color.g(), m_color.b(), m_color.w()));
        nvgFill(ctx);
        
    } else {
        // Determinate mode - show progress value
        if (m_value > 0.f) {
            float bar_width = w * m_value;
            
            nvgBeginPath(ctx);
            nvgRoundedRect(ctx, x, y, bar_width, h, h * 0.5f);
            nvgFillColor(ctx, nvgRGBAf(m_color.r(), m_color.g(), m_color.b(), m_color.w()));
            nvgFill(ctx);
        }
    }
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
