/*
    src/fluent_tooltip.cpp -- Fluent Design Tooltip implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_tooltip.h>
#include <chrono>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

NAMESPACE_BEGIN(nanogui)

FluentTooltip::FluentTooltip(Widget *parent, const std::string &text)
    : Widget(parent), m_text(text), m_position(Position::Auto), 
      m_anchor(nullptr), m_animation_progress(0.f) {
    set_visible(false);
}

void FluentTooltip::show_at_anchor() {
    if (!m_anchor)
        return;
    
    set_visible(true);
    m_show_time = std::chrono::steady_clock::now();
    m_animation_progress = 0.f;
    
    // Calculate preferred size
    Screen *screen = dynamic_cast<Screen*>(m_parent);
    if (!screen) return;
    Vector2i pref = preferred_size(screen->nvg_context());
    
    // Get anchor position and size
    Vector2i anchor_pos = m_anchor->absolute_position();
    Vector2i anchor_size = m_anchor->size();
    
    // Calculate tooltip position based on preference
    int x = 0, y = 0;
    int margin = 8;
    
    Position actual_pos = m_position;
    
    // Auto-position if needed
    if (actual_pos == Position::Auto) {
        // Default to bottom, but check if it fits
        if (anchor_pos.y() + anchor_size.y() + pref.y() + margin < m_parent->height()) {
            actual_pos = Position::Bottom;
        } else if (anchor_pos.y() - pref.y() - margin > 0) {
            actual_pos = Position::Top;
        } else if (anchor_pos.x() + anchor_size.x() + pref.x() + margin < m_parent->width()) {
            actual_pos = Position::Right;
        } else {
            actual_pos = Position::Left;
        }
    }
    
    switch (actual_pos) {
        case Position::Top:
            x = anchor_pos.x() + (anchor_size.x() - pref.x()) / 2;
            y = anchor_pos.y() - pref.y() - margin;
            break;
        case Position::Bottom:
            x = anchor_pos.x() + (anchor_size.x() - pref.x()) / 2;
            y = anchor_pos.y() + anchor_size.y() + margin;
            break;
        case Position::Left:
            x = anchor_pos.x() - pref.x() - margin;
            y = anchor_pos.y() + (anchor_size.y() - pref.y()) / 2;
            break;
        case Position::Right:
            x = anchor_pos.x() + anchor_size.x() + margin;
            y = anchor_pos.y() + (anchor_size.y() - pref.y()) / 2;
            break;
        case Position::Auto:
            break; // Already handled above
    }
    
    // Clamp to screen bounds
    x = std::max(4, std::min(x, m_parent->width() - pref.x() - 4));
    y = std::max(4, std::min(y, m_parent->height() - pref.y() - 4));
    
    Widget::set_position(Vector2i(x, y));
    set_size(pref);
}

void FluentTooltip::hide() {
    set_visible(false);
}

Vector2i FluentTooltip::preferred_size_impl(NVGcontext *ctx) const {
    int font_size = m_font_size == -1 ? 14 : m_font_size;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans");
    
    float text_width = nvgTextBounds(ctx, 0, 0, m_text.c_str(), nullptr, nullptr);
    
    // Tooltip padding: 8px horizontal, 6px vertical
    int width = (int)text_width + 16;
    int height = font_size + 12;
    
    // Max width: 200px
    width = std::min(width, 200);
    
    return Vector2i(width, height);
}

void FluentTooltip::draw(NVGcontext *ctx) {
    if (!m_visible)
        return;
    
    // Update animation
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_show_time).count();
    
    float anim_duration = 150.f; // 150ms fade-in
    if (elapsed < anim_duration) {
        m_animation_progress = elapsed / anim_duration;
    } else {
        m_animation_progress = 1.f;
    }
    
    Widget::draw(ctx);
    
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());
    
    nvgSave(ctx);
    
    // Apply fade-in animation
    float alpha = m_animation_progress;
    
    // Draw shadow
    NVGcolor shadow_outer = nvgRGBAf(0.f, 0.f, 0.f, 0.3f * alpha);
    NVGcolor shadow_inner = nvgRGBAf(0.f, 0.f, 0.f, 0.f);
    
    NVGpaint shadow = nvgBoxGradient(ctx, x, y + 2.f, w, h, 4.f, 6.f, shadow_outer, shadow_inner);
    
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x - 3.f, y - 3.f, w + 6.f, h + 6.f, 4.f);
    nvgFillPaint(ctx, shadow);
    nvgFill(ctx);
    
    // Draw tooltip background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, 4.f);
    nvgFillColor(ctx, nvgRGBAf(0.4f, 0.4f, 0.4f, 0.9f * alpha));
    nvgFill(ctx);
    
    // Draw text
    int font_size = m_font_size == -1 ? 14 : m_font_size;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans");
    nvgFillColor(ctx, nvgRGBAf(1.f, 1.f, 1.f, alpha));
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(ctx, x + w * 0.5f, y + h * 0.5f, m_text.c_str(), nullptr);
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
