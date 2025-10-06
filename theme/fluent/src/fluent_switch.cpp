/*
    src/fluent_switch.cpp -- Fluent Design Switch implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_switch.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentSwitch::FluentSwitch(Widget *parent, const std::string &caption)
    : Widget(parent), m_checked(false), m_caption(caption) {
}

Vector2i FluentSwitch::preferred_size_impl(NVGcontext *ctx) const {
    int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans");
    
    float text_width = 0.f;
    if (!m_caption.empty()) {
        text_width = nvgTextBounds(ctx, 0, 0, m_caption.c_str(), nullptr, nullptr);
    }
    
    // Switch dimensions: 52x32 (Fluent Design spec)
    int switch_width = 52;
    int switch_height = 32;
    
    int total_width = switch_width + (text_width > 0 ? 8 + (int)text_width : 0);
    int total_height = std::max(switch_height, font_size + 4);
    
    return Vector2i(total_width, total_height);
}

bool FluentSwitch::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!m_enabled || button != GLFW_MOUSE_BUTTON_1)
        return false;
    
    if (down) {
        m_checked = !m_checked;
        if (m_callback)
            m_callback(m_checked);
        return true;
    }
    
    return false;
}

void FluentSwitch::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    
    // Switch dimensions
    float track_width = 52.f;
    float track_height = 32.f;
    float track_radius = track_height * 0.5f;
    
    float thumb_size = 20.f;
    float thumb_radius = thumb_size * 0.5f;
    
    // Center vertically
    float switch_y = y + (m_size.y() - track_height) * 0.5f;
    
    nvgSave(ctx);
    
    // Colors based on state
    Color track_color, thumb_color;
    
    if (!m_enabled) {
        track_color = Color(0.f, 0.f, 0.f, 0.12f);
        thumb_color = Color(0.5f, 0.5f, 0.5f, 0.38f);
    } else if (m_checked) {
        // On state - use primary color
        track_color = Color(0.25f, 0.7f, 1.f, 0.5f);
        thumb_color = Color(0.25f, 0.7f, 1.f, 1.f);
    } else {
        // Off state
        track_color = Color(0.f, 0.f, 0.f, 0.38f);
        thumb_color = Color(1.f, 1.f, 1.f, 1.f);
    }
    
    // Draw track
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, switch_y, track_width, track_height, track_radius);
    nvgFillColor(ctx, nvgRGBAf(track_color.r(), track_color.g(), track_color.b(), track_color.w()));
    nvgFill(ctx);
    
    // Calculate thumb position
    float thumb_x = m_checked ? 
        x + track_width - thumb_radius - 6.f :  // On position
        x + thumb_radius + 6.f;                  // Off position
    float thumb_y = switch_y + track_height * 0.5f;
    
    // Draw thumb shadow
    if (m_enabled) {
        NVGcolor shadow_outer = nvgRGBAf(0.f, 0.f, 0.f, 0.15f);
        NVGcolor shadow_inner = nvgRGBAf(0.f, 0.f, 0.f, 0.f);
        
        NVGpaint shadow = nvgRadialGradient(
            ctx, thumb_x, thumb_y + 2.f, thumb_radius * 0.5f, thumb_radius + 4.f,
            shadow_outer, shadow_inner
        );
        
        nvgBeginPath(ctx);
        nvgCircle(ctx, thumb_x, thumb_y + 2.f, thumb_radius + 4.f);
        nvgFillPaint(ctx, shadow);
        nvgFill(ctx);
    }
    
    // Draw thumb
    nvgBeginPath(ctx);
    nvgCircle(ctx, thumb_x, thumb_y, thumb_radius);
    nvgFillColor(ctx, nvgRGBAf(thumb_color.r(), thumb_color.g(), thumb_color.b(), thumb_color.w()));
    nvgFill(ctx);
    
    // Draw caption
    if (!m_caption.empty()) {
        int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
        nvgFontSize(ctx, font_size);
        nvgFontFace(ctx, "sans");
        
        Color text_color = m_enabled ? m_theme->m_text_color : m_theme->m_disabled_text_color;
        nvgFillColor(ctx, nvgRGBAf(text_color.r(), text_color.g(), text_color.b(), text_color.w()));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, x + track_width + 8.f, y + m_size.y() * 0.5f, m_caption.c_str(), nullptr);
    }
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
