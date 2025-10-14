/*
    src/m3_switch.cpp -- Material Design 3 Switch implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_switch.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3Switch::M3Switch(Widget *parent, bool checked)
    : Widget(parent), m_checked(checked) {
}

M3Theme *M3Switch::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

bool M3Switch::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
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

Vector2i M3Switch::preferred_size(NVGcontext *) const {
    return Vector2i(52, 32); // M3 switch size
}

void M3Switch::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Widget::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float w = 52.0f; // M3 switch width
    float h = 32.0f; // M3 switch height
    float track_height = 32.0f;
    float thumb_size = m_checked ? 24.0f : 16.0f;

    nvgSave(ctx);

    // Determine colors based on state
    Color track_color, thumb_color;
    
    if (!m_enabled) {
        track_color = Color(theme->on_surface().r(), theme->on_surface().g(),
                           theme->on_surface().b(), 0.12f);
        thumb_color = Color(theme->on_surface().r(), theme->on_surface().g(),
                           theme->on_surface().b(), 0.38f);
    } else if (m_checked) {
        track_color = theme->primary();
        thumb_color = theme->on_primary();
    } else {
        track_color = theme->surface_variant();
        thumb_color = theme->outline();
    }

    // Draw track
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, track_height, track_height * 0.5f);
    nvgFillColor(ctx, track_color);
    nvgFill(ctx);

    // Draw outline for unchecked state
    if (!m_checked && m_enabled) {
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x + 1, y + 1, w - 2, track_height - 2, (track_height - 2) * 0.5f);
        nvgStrokeWidth(ctx, 2.0f);
        nvgStrokeColor(ctx, theme->outline());
        nvgStroke(ctx);
    }

    // Draw state layer if hovered
    if (m_mouse_focus && m_enabled) {
        float thumb_x = m_checked ? x + w - thumb_size - 4 : x + 4;
        float thumb_y = y + (h - thumb_size) * 0.5f;
        
        Color state_color = theme->state_layer(
            m_checked ? theme->primary() : theme->on_surface(),
            0.08f
        );
        
        nvgBeginPath(ctx);
        nvgCircle(ctx, thumb_x + thumb_size * 0.5f, thumb_y + thumb_size * 0.5f, 20.0f);
        nvgFillColor(ctx, state_color);
        nvgFill(ctx);
    }

    // Draw thumb
    float thumb_x = m_checked ? x + w - thumb_size - 4 : x + 4;
    float thumb_y = y + (h - thumb_size) * 0.5f;
    
    nvgBeginPath(ctx);
    nvgCircle(ctx, thumb_x + thumb_size * 0.5f, thumb_y + thumb_size * 0.5f, thumb_size * 0.5f);
    nvgFillColor(ctx, thumb_color);
    nvgFill(ctx);

    // Draw icon on thumb when checked
    if (m_checked && m_enabled) {
        nvgFontSize(ctx, 12);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, theme->primary());
        nvgText(ctx, thumb_x + thumb_size * 0.5f, thumb_y + thumb_size * 0.5f, 
                utf8(0xf00c).data(), nullptr); // Check icon
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
