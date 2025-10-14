/*
    src/m3_fab.cpp -- Material Design 3 FAB implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_fab.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3FAB::M3FAB(Widget *parent, int icon, Size size)
    : Button(parent, "", icon), m_size(size) {
    int pixel_size = get_pixel_size();
    set_fixed_size(Vector2i(pixel_size, pixel_size));
}

void M3FAB::set_size(Size size) {
    m_size = size;
    int pixel_size = get_pixel_size();
    set_fixed_size(Vector2i(pixel_size, pixel_size));
}

M3Theme *M3FAB::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

int M3FAB::get_pixel_size() const {
    switch (m_size) {
        case Size::Small: return 40;
        case Size::Regular: return 56;
        case Size::Large: return 96;
        default: return 56;
    }
}

void M3FAB::draw_state_layer(NVGcontext *ctx, float x, float y, float size) {
    M3Theme *theme = m3_theme();
    if (!theme || !m_enabled) return;

    float opacity = 0.0f;
    if (m_pushed) {
        opacity = 0.12f;
    } else if (m_mouse_focus) {
        opacity = 0.08f;
    } else if (m_focused) {
        opacity = 0.12f;
    }

    if (opacity > 0.0f) {
        Color state_color = theme->state_layer(theme->on_primary_container(), opacity);
        
        nvgBeginPath(ctx);
        nvgCircle(ctx, x + size * 0.5f, y + size * 0.5f, size * 0.5f);
        nvgFillColor(ctx, state_color);
        nvgFill(ctx);
    }
}

Vector2i M3FAB::preferred_size_impl(NVGcontext *) const {
    int pixel_size = get_pixel_size();
    return Vector2i(pixel_size, pixel_size);
}

void M3FAB::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Button::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float size = static_cast<float>(get_pixel_size());
    float radius = size * 0.5f;
    float cx = x + radius;
    float cy = y + radius;

    nvgSave(ctx);

    // Draw shadow (elevation 3)
    if (m_enabled) {
        NVGpaint shadow = nvgRadialGradient(ctx, cx, cy + 3, radius - 3, radius + 6,
                                           nvgRGBAf(0, 0, 0, 0.2f),
                                           nvgRGBAf(0, 0, 0, 0));
        nvgBeginPath(ctx);
        nvgCircle(ctx, cx, cy + 3, radius + 6);
        nvgCircle(ctx, cx, cy, radius);
        nvgPathWinding(ctx, NVG_HOLE);
        nvgFillPaint(ctx, shadow);
        nvgFill(ctx);
    }

    // Draw background
    Color bg_color = m_enabled ? theme->primary_container() 
                               : Color(theme->on_surface().r(), theme->on_surface().g(),
                                      theme->on_surface().b(), 0.12f);
    
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, radius);
    nvgFillColor(ctx, bg_color);
    nvgFill(ctx);

    // Draw elevation tint
    if (m_enabled) {
        Color tint = theme->elevation_tint(M3Theme::Elevation::Level3);
        nvgBeginPath(ctx);
        nvgCircle(ctx, cx, cy, radius);
        nvgFillColor(ctx, tint);
        nvgFill(ctx);
    }

    // Draw state layer
    draw_state_layer(ctx, x, y, size);

    // Draw icon
    Color icon_color = m_enabled ? theme->on_primary_container()
                                 : Color(theme->on_surface().r(), theme->on_surface().g(),
                                        theme->on_surface().b(), 0.38f);

    int icon_size = m_size == Size::Large ? 36 : 24;
    nvgFontSize(ctx, icon_size);
    nvgFontFace(ctx, "icons");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, icon_color);
    nvgText(ctx, cx, cy, utf8(m_icon).data(), nullptr);

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
