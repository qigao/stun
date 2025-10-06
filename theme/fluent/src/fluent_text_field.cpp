/*
    src/fluent_text_field.cpp -- Fluent Design Text Field implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_text_field.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentTextField::FluentTextField(Widget *parent, const std::string &value, Variant variant)
    : TextBox(parent, value), m_error(false), m_leading_icon(0), 
      m_trailing_icon(0), m_variant(variant), m_label_floating(false) {
}

Vector2i FluentTextField::preferred_size_impl(NVGcontext *ctx) const {
    Vector2i base_size = TextBox::preferred_size_impl(ctx);
    
    // Add space for label and helper text
    int height = base_size.y() + 24; // Label space
    if (!m_helper_text.empty() || !m_error_text.empty()) {
        height += 20; // Helper text space
    }
    
    // Add space for icons
    int width = base_size.x();
    if (m_leading_icon) width += 40;
    if (m_trailing_icon) width += 40;
    
    return Vector2i(width, height);
}

bool FluentTextField::focus_event(bool focused) {
    m_label_floating = focused || !m_value.empty();
    return TextBox::focus_event(focused);
}

void FluentTextField::draw(NVGcontext *ctx) {
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    
    nvgSave(ctx);
    
    // Update label floating state
    m_label_floating = m_focused || !m_value.empty();
    
    float label_offset = 24.f;
    float input_y = y + label_offset;
    float input_h = 56.f;
    
    // Draw container based on variant
    if (m_variant == Variant::Filled) {
        // Filled variant
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, input_y, w, input_h, 4.f);
        nvgFillColor(ctx, nvgRGBAf(0.f, 0.f, 0.f, 0.06f));
        nvgFill(ctx);
        
        // Bottom indicator line
        float line_color = m_error ? 0.96f : 0.25f;
        float line_g = m_error ? 0.26f : 0.7f;
        float line_b = m_error ? 0.21f : 1.f;
        float line_width = m_focused ? 2.f : 1.f;
        
        nvgBeginPath(ctx);
        nvgRect(ctx, x, input_y + input_h - line_width, w, line_width);
        nvgFillColor(ctx, nvgRGBAf(line_color, line_g, line_b, 1.f));
        nvgFill(ctx);
        
    } else {
        // Outlined variant
        float border_color = m_error ? 0.96f : (m_focused ? 0.25f : 0.7f);
        float border_g = m_error ? 0.26f : (m_focused ? 0.7f : 0.7f);
        float border_b = m_error ? 0.21f : (m_focused ? 1.f : 0.7f);
        float border_width = m_focused ? 2.f : 1.f;
        
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, input_y, w, input_h, 4.f);
        nvgStrokeWidth(ctx, border_width);
        nvgStrokeColor(ctx, nvgRGBAf(border_color, border_g, border_b, 1.f));
        nvgStroke(ctx);
    }
    
    // Draw label
    if (!m_label.empty()) {
        float label_size = m_label_floating ? 12.f : 16.f;
        float label_y = m_label_floating ? y + 8.f : input_y + input_h * 0.5f;
        float label_x = x + (m_leading_icon ? 48.f : 16.f);
        
        nvgFontSize(ctx, label_size);
        nvgFontFace(ctx, "sans");
        
        Color label_color = m_error ? Color(0.96f, 0.26f, 0.21f, 1.f) :
                           (m_focused ? Color(0.25f, 0.7f, 1.f, 1.f) :
                            Color(0.4f, 0.4f, 0.4f, 1.f));
        
        nvgFillColor(ctx, nvgRGBAf(label_color.r(), label_color.g(), 
                                   label_color.b(), label_color.w()));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, label_x, label_y, m_label.c_str(), nullptr);
    }
    
    // Draw leading icon
    if (m_leading_icon) {
        auto icon = utf8(m_leading_icon);
        nvgFontSize(ctx, 24.f);
        nvgFontFace(ctx, "icons");
        nvgFillColor(ctx, nvgRGBAf(0.4f, 0.4f, 0.4f, 1.f));
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(ctx, x + 20.f, input_y + input_h * 0.5f, icon.data(), nullptr);
    }
    
    // Draw trailing icon
    if (m_trailing_icon) {
        auto icon = utf8(m_trailing_icon);
        nvgFontSize(ctx, 24.f);
        nvgFontFace(ctx, "icons");
        nvgFillColor(ctx, nvgRGBAf(0.4f, 0.4f, 0.4f, 1.f));
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(ctx, x + w - 20.f, input_y + input_h * 0.5f, icon.data(), nullptr);
    }
    
    // Draw input text
    float text_x = x + (m_leading_icon ? 48.f : 16.f);
    float text_y = input_y + input_h * 0.5f;
    
    nvgFontSize(ctx, 16.f);
    nvgFontFace(ctx, "sans");
    nvgFillColor(ctx, nvgRGBAf(0.2f, 0.2f, 0.2f, 1.f));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    
    // Draw value or placeholder
    if (!m_value.empty()) {
        nvgText(ctx, text_x, text_y, m_value.c_str(), nullptr);
    } else if (!m_placeholder.empty() && !m_focused) {
        nvgFillColor(ctx, nvgRGBAf(0.6f, 0.6f, 0.6f, 1.f));
        nvgText(ctx, text_x, text_y, m_placeholder.c_str(), nullptr);
    }
    
    // Draw cursor if focused
    if (m_focused) {
        float cursor_x = text_x + nvgTextBounds(ctx, 0, 0, m_value.c_str(), nullptr, nullptr);
        nvgBeginPath(ctx);
        nvgRect(ctx, cursor_x, text_y - 10.f, 1.f, 20.f);
        nvgFillColor(ctx, nvgRGBAf(0.25f, 0.7f, 1.f, 1.f));
        nvgFill(ctx);
    }
    
    // Draw helper or error text
    const std::string &support_text = m_error ? m_error_text : m_helper_text;
    if (!support_text.empty()) {
        nvgFontSize(ctx, 12.f);
        nvgFontFace(ctx, "sans");
        
        Color support_color = m_error ? Color(0.96f, 0.26f, 0.21f, 1.f) :
                                       Color(0.6f, 0.6f, 0.6f, 1.f);
        
        nvgFillColor(ctx, nvgRGBAf(support_color.r(), support_color.g(), 
                                   support_color.b(), support_color.w()));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgText(ctx, x + 16.f, input_y + input_h + 4.f, support_text.c_str(), nullptr);
    }
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
