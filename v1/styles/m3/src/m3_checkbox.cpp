/*
    src/m3_checkbox.cpp -- Material Design 3 Checkbox implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_checkbox.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3Checkbox::M3Checkbox(Widget *parent, const std::string &caption, bool checked)
    : Widget(parent), m_caption(caption), m_checked(checked) {
}

M3Theme *M3Checkbox::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

bool M3Checkbox::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!m_enabled || button != NANOGUI_MOUSE_BUTTON_LEFT)
        return false;

    if (down) {
        m_checked = !m_checked;
        if (m_callback)
            m_callback(m_checked);
    }

    return true;
}

Vector2i M3Checkbox::preferred_size(NVGcontext *ctx) const {
    int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans");
    
    float tw = 0;
    if (!m_caption.empty()) {
        tw = nvgTextBounds(ctx, 0, 0, m_caption.c_str(), nullptr, nullptr);
    }
    
    return Vector2i(static_cast<int>(18 + (tw > 0 ? tw + 8 : 0)), 
                    std::max(18, font_size));
}

void M3Checkbox::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Widget::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float box_size = 18.0f;

    nvgSave(ctx);

    // Determine colors
    Color box_color, check_color;
    
    if (!m_enabled) {
        box_color = Color(theme->on_surface().r(), theme->on_surface().g(),
                         theme->on_surface().b(), 0.38f);
        check_color = Color(theme->on_surface().r(), theme->on_surface().g(),
                           theme->on_surface().b(), 0.38f);
    } else if (m_checked) {
        box_color = theme->primary();
        check_color = theme->on_primary();
    } else {
        box_color = theme->on_surface_variant();
        check_color = theme->on_primary();
    }

    // Draw state layer if hovered
    if (m_mouse_focus && m_enabled) {
        Color state_color = theme->state_layer(
            m_checked ? theme->primary() : theme->on_surface(),
            0.08f
        );
        
        nvgBeginPath(ctx);
        nvgCircle(ctx, x + box_size * 0.5f, y + box_size * 0.5f, 20.0f);
        nvgFillColor(ctx, state_color);
        nvgFill(ctx);
    }

    // Draw checkbox box
    float corner_radius = 2.0f;
    
    if (m_checked) {
        // Filled box
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, box_size, box_size, corner_radius);
        nvgFillColor(ctx, box_color);
        nvgFill(ctx);
        
        // Draw checkmark
        nvgFontSize(ctx, 14);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, check_color);
        nvgText(ctx, x + box_size * 0.5f, y + box_size * 0.5f, 
                utf8(0xf00c).data(), nullptr); // Check icon
    } else {
        // Outlined box
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x + 1, y + 1, box_size - 2, box_size - 2, corner_radius);
        nvgStrokeWidth(ctx, 2.0f);
        nvgStrokeColor(ctx, box_color);
        nvgStroke(ctx);
    }

    // Draw label
    if (!m_caption.empty()) {
        int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
        nvgFontSize(ctx, font_size);
        nvgFontFace(ctx, "sans");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, m_enabled ? theme->on_surface() : 
                     Color(theme->on_surface().r(), theme->on_surface().g(),
                          theme->on_surface().b(), 0.38f));
        nvgText(ctx, x + box_size + 8, y + box_size * 0.5f, m_caption.c_str(), nullptr);
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
