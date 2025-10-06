/*
    src/fluent_checkbox.cpp -- Fluent Design Checkbox implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_checkbox.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentCheckbox::FluentCheckbox(Widget *parent, const std::string &caption)
    : Widget(parent), m_checked(false), m_caption(caption) {
}

Vector2i FluentCheckbox::preferred_size_impl(NVGcontext *ctx) const {
    int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans");
    
    float text_width = 0.f;
    if (!m_caption.empty()) {
        text_width = nvgTextBounds(ctx, 0, 0, m_caption.c_str(), nullptr, nullptr);
    }
    
    // Checkbox size: 18x18, with 40x40 touch target
    int total_width = 40 + (text_width > 0 ? 8 + (int)text_width : 0);
    int total_height = 40;
    
    return Vector2i(total_width, total_height);
}

bool FluentCheckbox::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
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

void FluentCheckbox::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    
    // Center checkbox in 40x40 touch target
    float checkbox_x = x + 11.f;
    float checkbox_y = y + 11.f;
    float checkbox_size = 18.f;
    
    nvgSave(ctx);
    
    // Colors based on state
    Color box_color, check_color;
    
    if (!m_enabled) {
        box_color = Color(0.f, 0.f, 0.f, 0.38f);
        check_color = Color(0.f, 0.f, 0.f, 0.38f);
    } else if (m_checked) {
        box_color = Color(0.25f, 0.7f, 1.f, 1.f); // Primary color
        check_color = Color(1.f, 1.f, 1.f, 1.f);
    } else {
        box_color = Color(0.f, 0.f, 0.f, 0.54f);
        check_color = Color(1.f, 1.f, 1.f, 1.f);
    }
    
    // Draw checkbox box
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, checkbox_x, checkbox_y, checkbox_size, checkbox_size, 2.f);
    
    if (m_checked) {
        nvgFillColor(ctx, nvgRGBAf(box_color.r(), box_color.g(), box_color.b(), box_color.w()));
        nvgFill(ctx);
    } else {
        nvgStrokeWidth(ctx, 2.f);
        nvgStrokeColor(ctx, nvgRGBAf(box_color.r(), box_color.g(), box_color.b(), box_color.w()));
        nvgStroke(ctx);
    }
    
    // Draw checkmark if checked
    if (m_checked) {
        nvgBeginPath(ctx);
        nvgMoveTo(ctx, checkbox_x + 4.f, checkbox_y + 9.f);
        nvgLineTo(ctx, checkbox_x + 7.f, checkbox_y + 13.f);
        nvgLineTo(ctx, checkbox_x + 14.f, checkbox_y + 5.f);
        nvgStrokeWidth(ctx, 2.f);
        nvgStrokeColor(ctx, nvgRGBAf(check_color.r(), check_color.g(), 
                                     check_color.b(), check_color.w()));
        nvgStroke(ctx);
    }
    
    // Draw caption
    if (!m_caption.empty()) {
        int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
        nvgFontSize(ctx, font_size);
        nvgFontFace(ctx, "sans");
        
        Color text_color = m_enabled ? m_theme->m_text_color : m_theme->m_disabled_text_color;
        nvgFillColor(ctx, nvgRGBAf(text_color.r(), text_color.g(), text_color.b(), text_color.w()));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, x + 48.f, y + 20.f, m_caption.c_str(), nullptr);
    }
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
