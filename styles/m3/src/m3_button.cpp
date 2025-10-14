/*
    src/m3_button.cpp -- Material Design 3 button implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_button.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3Button::M3Button(Widget *parent, const std::string &caption, int icon, Style style)
    : Button(parent, caption, icon), m_style(style) {
    // M3 buttons have 40dp height
    set_fixed_height(40);
}

void M3Button::set_style(Style style) {
    m_style = style;
}

M3Theme *M3Button::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

Color M3Button::get_background_color() const {
    M3Theme *theme = m3_theme();
    if (!theme) return Color(0.5f, 0.5f, 0.5f, 1.0f);

    if (!m_enabled) {
        return Color(theme->on_surface().r(), theme->on_surface().g(),
                     theme->on_surface().b(), 0.12f);
    }

    switch (m_style) {
        case Style::Filled:
            return theme->primary();
        
        case Style::Tonal:
            return theme->primary_container();
        
        case Style::Elevated:
            // Surface with elevation tint
            return theme->surface();
        
        case Style::Outlined:
        case Style::Text:
            return Color(0.0f, 0.0f, 0.0f, 0.0f); // Transparent
        
        default:
            return theme->primary();
    }
}

Color M3Button::get_text_color() const {
    M3Theme *theme = m3_theme();
    if (!theme) return Color(1.0f, 1.0f, 1.0f, 1.0f);

    if (!m_enabled) {
        return Color(theme->on_surface().r(), theme->on_surface().g(),
                     theme->on_surface().b(), 0.38f);
    }

    switch (m_style) {
        case Style::Filled:
            return theme->on_primary();
        
        case Style::Tonal:
            return theme->on_primary_container();
        
        case Style::Elevated:
            return theme->primary();
        
        case Style::Outlined:
        case Style::Text:
            return theme->primary();
        
        default:
            return theme->on_primary();
    }
}

Color M3Button::get_border_color() const {
    M3Theme *theme = m3_theme();
    if (!theme) return Color(0.5f, 0.5f, 0.5f, 1.0f);

    if (!m_enabled) {
        return Color(theme->on_surface().r(), theme->on_surface().g(),
                     theme->on_surface().b(), 0.12f);
    }

    return theme->outline();
}

void M3Button::draw_state_layer(NVGcontext *ctx, float x, float y, float w, float h) {
    M3Theme *theme = m3_theme();
    if (!theme || !m_enabled) return;

    Color base_color = get_text_color();
    float opacity = 0.0f;

    if (m_pushed) {
        opacity = 0.12f; // Press state
    } else if (m_mouse_focus) {
        opacity = 0.08f; // Hover state
    } else if (m_focused) {
        opacity = 0.12f; // Focus state
    }

    if (opacity > 0.0f) {
        Color state_color = theme->state_layer(base_color, opacity);
        
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, w, h, theme->corner_radius(M3Theme::ShapeFamily::Small));
        nvgFillColor(ctx, state_color);
        nvgFill(ctx);
    }
}

Vector2i M3Button::preferred_size_impl(NVGcontext *ctx) const {
    int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans-bold");
    
    float tw = nvgTextBounds(ctx, 0, 0, m_caption.c_str(), nullptr, nullptr);
    
    // M3 button padding: 24dp horizontal, 10dp vertical
    int padding_x = 24;
    int padding_y = 10;
    
    if (m_icon) {
        float icon_width = font_size * 1.2f;
        tw += icon_width + 8; // Icon + spacing
    }
    
    return Vector2i(static_cast<int>(tw) + 2 * padding_x,
                    font_size + 2 * padding_y);
}

void M3Button::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        // Fallback to base Button draw
        Button::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float w = m_size.x();
    float h = m_size.y();
    float corner_radius = theme->corner_radius(M3Theme::ShapeFamily::Small);

    nvgSave(ctx);

    // Draw elevation for Elevated style
    if (m_style == Style::Elevated && m_enabled) {
        Color elevation_tint = theme->elevation_tint(M3Theme::Elevation::Level1);
        
        // Draw shadow
        NVGpaint shadow = nvgBoxGradient(ctx, x, y + 2, w, h, corner_radius, 4.0f,
                                         nvgRGBAf(0, 0, 0, 0.15f),
                                         nvgRGBAf(0, 0, 0, 0));
        nvgBeginPath(ctx);
        nvgRect(ctx, x - 4, y - 4, w + 8, h + 10);
        nvgRoundedRect(ctx, x, y, w, h, corner_radius);
        nvgPathWinding(ctx, NVG_HOLE);
        nvgFillPaint(ctx, shadow);
        nvgFill(ctx);
    }

    // Draw background
    Color bg_color = get_background_color();
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, corner_radius);
    nvgFillColor(ctx, bg_color);
    nvgFill(ctx);

    // Draw elevation tint for Elevated style
    if (m_style == Style::Elevated && m_enabled) {
        Color tint = theme->elevation_tint(M3Theme::Elevation::Level1);
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, w, h, corner_radius);
        nvgFillColor(ctx, tint);
        nvgFill(ctx);
    }

    // Draw state layer
    draw_state_layer(ctx, x, y, w, h);

    // Draw border for Outlined style
    if (m_style == Style::Outlined) {
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x + 0.5f, y + 0.5f, w - 1, h - 1, corner_radius);
        nvgStrokeWidth(ctx, 1.0f);
        nvgStrokeColor(ctx, get_border_color());
        nvgStroke(ctx);
    }

    // Draw icon and text
    Color text_color = get_text_color();
    int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
    
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans-bold");
    nvgFillColor(ctx, text_color);

    float text_x = x + w * 0.5f;
    float text_y = y + h * 0.5f;

    if (m_icon) {
        // Draw icon
        nvgFontSize(ctx, font_size * 1.2f);
        nvgFontFace(ctx, "icons");
        
        float icon_w = nvgTextBounds(ctx, 0, 0, utf8(m_icon).data(), nullptr, nullptr);
        float text_w = 0;
        
        if (!m_caption.empty()) {
            nvgFontSize(ctx, font_size);
            nvgFontFace(ctx, "sans-bold");
            text_w = nvgTextBounds(ctx, 0, 0, m_caption.c_str(), nullptr, nullptr);
        }
        
        float total_w = icon_w + (text_w > 0 ? text_w + 8 : 0);
        float icon_x = x + (w - total_w) * 0.5f;
        
        nvgFontSize(ctx, font_size * 1.2f);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, text_color);
        nvgText(ctx, icon_x, text_y, utf8(m_icon).data(), nullptr);
        
        if (!m_caption.empty()) {
            nvgFontSize(ctx, font_size);
            nvgFontFace(ctx, "sans-bold");
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(ctx, icon_x + icon_w + 8, text_y, m_caption.c_str(), nullptr);
        }
    } else {
        // Draw text only
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(ctx, text_x, text_y, m_caption.c_str(), nullptr);
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
