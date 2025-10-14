/*
    src/m3_radio.cpp -- Material Design 3 Radio Button implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_radio.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3Radio::M3Radio(Widget *parent, const std::vector<std::string> &items, int selected_index)
    : Widget(parent), m_items(items), m_selected_index(selected_index) {
}

M3Theme *M3Radio::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3Radio::set_selected_index(int index) {
    if (index >= -1 && index < static_cast<int>(m_items.size())) {
        m_selected_index = index;
        if (m_callback)
            m_callback(index);
    }
}

int M3Radio::radio_at_position(const Vector2i &p) const {
    int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
    int item_height = font_size + 10;
    
    Vector2i local = p - m_pos;
    if (local.x() < 0 || local.x() > m_size.x())
        return -1;
    
    int index = local.y() / item_height;
    if (index >= 0 && index < static_cast<int>(m_items.size()))
        return index;
    
    return -1;
}

bool M3Radio::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!m_enabled || button != NANOGUI_MOUSE_BUTTON_LEFT)
        return false;

    if (down) {
        int index = radio_at_position(p);
        if (index >= 0) {
            set_selected_index(index);
            return true;
        }
    }

    return false;
}

Vector2i M3Radio::preferred_size(NVGcontext *ctx) const {
    int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans");
    
    float max_width = 0;
    for (const auto &item : m_items) {
        float tw = nvgTextBounds(ctx, 0, 0, item.c_str(), nullptr, nullptr);
        max_width = std::max(max_width, tw);
    }
    
    int item_height = font_size + 10;
    int total_height = static_cast<int>(m_items.size()) * item_height;
    
    return Vector2i(static_cast<int>(20 + max_width + 8), total_height);
}

void M3Radio::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Widget::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
    int item_height = font_size + 10;
    float radio_size = 20.0f;

    nvgSave(ctx);

    for (size_t i = 0; i < m_items.size(); ++i) {
        float item_y = y + i * item_height;
        bool selected = (static_cast<int>(i) == m_selected_index);
        bool hovered = (static_cast<int>(i) == m_hover_index);

        // Determine colors
        Color outer_color, inner_color;
        
        if (!m_enabled) {
            outer_color = Color(theme->on_surface().r(), theme->on_surface().g(),
                               theme->on_surface().b(), 0.38f);
            inner_color = outer_color;
        } else if (selected) {
            outer_color = theme->primary();
            inner_color = theme->primary();
        } else {
            outer_color = theme->on_surface_variant();
            inner_color = theme->on_surface_variant();
        }

        // Draw state layer if hovered
        if (hovered && m_enabled) {
            Color state_color = theme->state_layer(
                selected ? theme->primary() : theme->on_surface(),
                0.08f
            );
            
            nvgBeginPath(ctx);
            nvgCircle(ctx, x + radio_size * 0.5f, item_y + radio_size * 0.5f, 20.0f);
            nvgFillColor(ctx, state_color);
            nvgFill(ctx);
        }

        // Draw outer circle
        nvgBeginPath(ctx);
        nvgCircle(ctx, x + radio_size * 0.5f, item_y + radio_size * 0.5f, radio_size * 0.5f);
        nvgStrokeWidth(ctx, 2.0f);
        nvgStrokeColor(ctx, outer_color);
        nvgStroke(ctx);

        // Draw inner circle if selected
        if (selected) {
            nvgBeginPath(ctx);
            nvgCircle(ctx, x + radio_size * 0.5f, item_y + radio_size * 0.5f, radio_size * 0.25f);
            nvgFillColor(ctx, inner_color);
            nvgFill(ctx);
        }

        // Draw label
        nvgFontSize(ctx, font_size);
        nvgFontFace(ctx, "sans");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, m_enabled ? theme->on_surface() :
                     Color(theme->on_surface().r(), theme->on_surface().g(),
                          theme->on_surface().b(), 0.38f));
        nvgText(ctx, x + radio_size + 8, item_y + radio_size * 0.5f, 
                m_items[i].c_str(), nullptr);
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
