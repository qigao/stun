/*
    src/m3_badge.cpp -- Material Design 3 Badge implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_badge.h>
#include <nanogui/opengl.h>
#include <cstdio>

NAMESPACE_BEGIN(nanogui)

M3Badge::M3Badge(Widget *parent, int count)
    : Widget(parent), m_count(count) {
}

M3Theme *M3Badge::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

Vector2i M3Badge::preferred_size(NVGcontext *ctx) const {
    if (m_count == 0) {
        // Dot badge
        return Vector2i(6, 6);
    } else if (m_count < 10) {
        // Single digit
        return Vector2i(16, 16);
    } else {
        // Multiple digits
        char count_str[16];
        snprintf(count_str, sizeof(count_str), "%d", m_count > 99 ? 99 : m_count);
        
        nvgFontSize(ctx, 11);
        nvgFontFace(ctx, "sans-bold");
        float tw = nvgTextBounds(ctx, 0, 0, count_str, nullptr, nullptr);
        
        return Vector2i(static_cast<int>(tw) + 8, 16);
    }
}

void M3Badge::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Widget::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();

    nvgSave(ctx);

    if (m_count == 0) {
        // Draw dot badge (6x6dp)
        nvgBeginPath(ctx);
        nvgCircle(ctx, x + 3, y + 3, 3);
        nvgFillColor(ctx, theme->error());
        nvgFill(ctx);
    } else {
        // Draw count badge
        char count_str[16];
        if (m_count > 99) {
            snprintf(count_str, sizeof(count_str), "99+");
        } else {
            snprintf(count_str, sizeof(count_str), "%d", m_count);
        }
        
        nvgFontSize(ctx, 11);
        nvgFontFace(ctx, "sans-bold");
        float tw = nvgTextBounds(ctx, 0, 0, count_str, nullptr, nullptr);
        
        float badge_width = std::max(16.0f, tw + 8);
        float badge_height = 16.0f;
        
        // Draw background
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, badge_width, badge_height, badge_height * 0.5f);
        nvgFillColor(ctx, theme->error());
        nvgFill(ctx);
        
        // Draw count text
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, theme->on_error());
        nvgText(ctx, x + badge_width * 0.5f, y + badge_height * 0.5f, count_str, nullptr);
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
