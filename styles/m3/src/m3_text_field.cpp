/*
    src/m3_text_field.cpp -- Material Design 3 Text Field implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_text_field.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>
#include <nanogui/window.h>

NAMESPACE_BEGIN(nanogui)

M3TextField::M3TextField(Widget *parent, const std::string &value, Style style)
    : TextBox(parent, value), m_style(style) {
    set_fixed_height(56); // M3 text field height
}

M3Theme *M3TextField::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

Vector2i M3TextField::preferred_size_impl(NVGcontext *ctx) const {
    Vector2i size = TextBox::preferred_size_impl(ctx);
    size.y() = 56; // M3 standard height
    return size;
}

void M3TextField::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Widget::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float w = m_size.x();
    float h = m_size.y();

    nvgSave(ctx);

    // Determine colors based on state
    Color bg_color, border_color, text_color, label_color;
    
    if (m_error) {
        border_color = theme->error();
        label_color = theme->error();
        text_color = theme->on_surface();
    } else if (m_focused) {
        border_color = theme->primary();
        label_color = theme->primary();
        text_color = theme->on_surface();
    } else {
        border_color = theme->outline();
        label_color = theme->on_surface_variant();
        text_color = theme->on_surface();
    }

    if (m_style == Style::Filled) {
        // Filled variant
        bg_color = theme->surface_variant();
        float corner_radius = theme->corner_radius(M3Theme::ShapeFamily::ExtraSmall);
        
        // Draw background (rounded top corners only)
        nvgBeginPath(ctx);
        nvgMoveTo(ctx, x, y + h);
        nvgLineTo(ctx, x, y + corner_radius);
        nvgArcTo(ctx, x, y, x + corner_radius, y, corner_radius);
        nvgLineTo(ctx, x + w - corner_radius, y);
        nvgArcTo(ctx, x + w, y, x + w, y + corner_radius, corner_radius);
        nvgLineTo(ctx, x + w, y + h);
        nvgClosePath(ctx);
        nvgFillColor(ctx, bg_color);
        nvgFill(ctx);

        // Draw bottom indicator
        float indicator_height = m_focused ? 2.0f : 1.0f;
        nvgBeginPath(ctx);
        nvgRect(ctx, x, y + h - indicator_height, w, indicator_height);
        nvgFillColor(ctx, border_color);
        nvgFill(ctx);

    } else {
        // Outlined variant
        bg_color = Color(0, 0, 0, 0); // Transparent
        float corner_radius = theme->corner_radius(M3Theme::ShapeFamily::Small);
        
        // Draw outline
        float stroke_width = m_focused ? 2.0f : 1.0f;
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x + stroke_width * 0.5f, y + stroke_width * 0.5f,
                      w - stroke_width, h - stroke_width, corner_radius);
        nvgStrokeWidth(ctx, stroke_width);
        nvgStrokeColor(ctx, border_color);
        nvgStroke(ctx);
    }

    // Draw label
    if (!m_label.empty()) {
        int label_font_size = 12;
        float label_y = y + 8;
        
        nvgFontSize(ctx, label_font_size);
        nvgFontFace(ctx, "sans");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(ctx, label_color);
        nvgText(ctx, x + 16, label_y, m_label.c_str(), nullptr);
    }

    // Draw text content
    float text_y = y + (m_label.empty() ? h * 0.5f : 28);
    int font_size = m_font_size == -1 ? m_theme->m_text_box_font_size : m_font_size;
    
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, text_color);
    
    float text_x = x + 16;
    float clip_x = x + 16;
    float clip_w = w - 32;

    nvgSave(ctx);
    nvgIntersectScissor(ctx, clip_x, y, clip_w, h);

    if (m_committed) {
        nvgText(ctx, text_x, text_y, m_value.c_str(), nullptr);
    } else {
        const int max_glyphs = 1024;
        NVGglyphPosition glyphs[max_glyphs];
        float text_bound[4];
        nvgTextBounds(ctx, text_x, text_y, m_value_temp.c_str(), nullptr, text_bound);
        float lineh = text_bound[3] - text_bound[1];

        int nglyphs = nvgTextGlyphPositions(ctx, text_x, text_y, m_value_temp.c_str(),
                                            nullptr, glyphs, max_glyphs);
        update_cursor(ctx, text_bound[2], glyphs, nglyphs);

        int prev_cpos = m_cursor_pos > 0 ? m_cursor_pos - 1 : 0;
        int next_cpos = m_cursor_pos < nglyphs ? m_cursor_pos + 1 : nglyphs;
        float prev_cx = cursor_index_to_position(prev_cpos, text_bound[2], glyphs, nglyphs);
        float next_cx = cursor_index_to_position(next_cpos, text_bound[2], glyphs, nglyphs);

        if (next_cx > clip_x + clip_w)
            m_text_offset -= next_cx - (clip_x + clip_w) + 1;
        if (prev_cx < clip_x)
            m_text_offset += clip_x - prev_cx + 1;

        text_x += m_text_offset;

        nvgText(ctx, text_x, text_y, m_value_temp.c_str(), nullptr);
        nvgTextBounds(ctx, text_x, text_y, m_value_temp.c_str(), nullptr, text_bound);

        nglyphs = nvgTextGlyphPositions(ctx, text_x, text_y, m_value_temp.c_str(), nullptr,
                                        glyphs, max_glyphs);

        if (m_cursor_pos > -1) {
            if (m_selection_pos > -1) {
                float caretx = cursor_index_to_position(m_cursor_pos, text_bound[2], glyphs, nglyphs);
                float selx = cursor_index_to_position(m_selection_pos, text_bound[2], glyphs, nglyphs);

                if (caretx > selx)
                    std::swap(caretx, selx);

                nvgBeginPath(ctx);
                nvgFillColor(ctx, nvgRGBA(255, 255, 255, 80));
                nvgRect(ctx, caretx, text_y - lineh * 0.5f, selx - caretx, lineh);
                nvgFill(ctx);
            }

            float caretx = cursor_index_to_position(m_cursor_pos, text_bound[2], glyphs, nglyphs);

            nvgBeginPath(ctx);
            nvgMoveTo(ctx, caretx, text_y - lineh * 0.5f);
            nvgLineTo(ctx, caretx, text_y + lineh * 0.5f);
            nvgStrokeColor(ctx, theme->primary());
            nvgStrokeWidth(ctx, 1.0f);
            nvgStroke(ctx);
        }
    }
    nvgRestore(ctx);

    // Draw helper text
    if (!m_helper_text.empty()) {
        nvgFontSize(ctx, 12);
        nvgFontFace(ctx, "sans");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(ctx, m_error ? theme->error() : theme->on_surface_variant());
        nvgText(ctx, x + 16, y + h + 4, m_helper_text.c_str(), nullptr);
    }

    nvgRestore(ctx);
}

void M3TextField::update_cursor(NVGcontext *, float lastx, const NVGglyphPosition *glyphs, int size) {
    if (m_mouse_down_pos.x() != -1) {
        if (m_mouse_down_modifier == GLFW_MOD_SHIFT) {
            if (m_selection_pos == -1)
                m_selection_pos = m_cursor_pos;
        } else
            m_selection_pos = -1;

        m_cursor_pos = position_to_cursor_index(m_mouse_down_pos.x(), lastx, glyphs, size);
        m_mouse_down_pos = Vector2i(-1, -1);
    } else if (m_mouse_drag_pos.x() != -1) {
        if (m_selection_pos == -1)
            m_selection_pos = m_cursor_pos;
        m_cursor_pos = position_to_cursor_index(m_mouse_drag_pos.x(), lastx, glyphs, size);
    } else {
        if (m_cursor_pos == -2)
            m_cursor_pos = size;
    }

    if (m_cursor_pos == m_selection_pos)
        m_selection_pos = -1;
}

float M3TextField::cursor_index_to_position(int index, float lastx, const NVGglyphPosition *glyphs, int size) {
    if (index == size)
        return lastx;
    return glyphs[index].x;
}

int M3TextField::position_to_cursor_index(float posx, float lastx, const NVGglyphPosition *glyphs, int size) {
    int m_cursor_id = 0;
    float caretx = glyphs[m_cursor_id].x;
    for (int j = 1; j < size; j++) {
        if (std::abs(caretx - posx) > std::abs(glyphs[j].x - posx)) {
            m_cursor_id = j;
            caretx = glyphs[m_cursor_id].x;
        }
    }
    if (std::abs(caretx - posx) > std::abs(lastx - posx))
        m_cursor_id = size;
    return m_cursor_id;
}

NAMESPACE_END(nanogui)
