/*
    src/fluent_badge.cpp -- Fluent Design Badge implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_badge.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentBadge::FluentBadge(Widget *parent, int count)
    : Widget(parent), m_count(count), m_dot_mode(false), 
      m_position(Position::TopRight), m_badge_color(0.96f, 0.26f, 0.21f, 1.f) {
}

Vector2i FluentBadge::preferred_size_impl(NVGcontext *ctx) const {
    if (m_dot_mode || m_count == 0) {
        return Vector2i(8, 8); // Dot size
    }
    
    // Calculate size based on count
    std::string text = m_count > 99 ? "99+" : std::to_string(m_count);
    
    nvgFontSize(ctx, 11.f);
    nvgFontFace(ctx, "sans-bold");
    float text_width = nvgTextBounds(ctx, 0, 0, text.c_str(), nullptr, nullptr);
    
    int width = std::max(20, (int)(text_width + 12));
    int height = 20;
    
    return Vector2i(width, height);
}

void FluentBadge::draw(NVGcontext *ctx) {
    if (m_count == 0 && !m_dot_mode)
        return; // Don't draw if count is 0 and not in dot mode
    
    Widget::draw(ctx);
    
    nvgSave(ctx);
    
    // Get parent widget bounds to position badge
    float badge_w = m_size.x();
    float badge_h = m_size.y();
    
    // Position relative to parent (default to TopRight)
    float badge_x = m_pos.x() + m_size.x() - badge_w * 0.5f;
    float badge_y = m_pos.y() - badge_h * 0.5f;
    
    switch (m_position) {
        case Position::TopRight:
            badge_x = m_pos.x() + m_size.x() - badge_w * 0.5f;
            badge_y = m_pos.y() - badge_h * 0.5f;
            break;
        case Position::TopLeft:
            badge_x = m_pos.x() - badge_w * 0.5f;
            badge_y = m_pos.y() - badge_h * 0.5f;
            break;
        case Position::BottomRight:
            badge_x = m_pos.x() + m_size.x() - badge_w * 0.5f;
            badge_y = m_pos.y() + m_size.y() - badge_h * 0.5f;
            break;
        case Position::BottomLeft:
            badge_x = m_pos.x() - badge_w * 0.5f;
            badge_y = m_pos.y() + m_size.y() - badge_h * 0.5f;
            break;
    }
    
    if (m_dot_mode) {
        // Draw simple dot
        float radius = 4.f;
        nvgBeginPath(ctx);
        nvgCircle(ctx, badge_x + radius, badge_y + radius, radius);
        nvgFillColor(ctx, nvgRGBAf(m_badge_color.r(), m_badge_color.g(), 
                                   m_badge_color.b(), m_badge_color.w()));
        nvgFill(ctx);
    } else {
        // Draw badge with count
        float radius = badge_h * 0.5f;
        
        // Draw shadow
        NVGcolor shadow_outer = nvgRGBAf(0.f, 0.f, 0.f, 0.2f);
        NVGcolor shadow_inner = nvgRGBAf(0.f, 0.f, 0.f, 0.f);
        
        NVGpaint shadow = nvgRadialGradient(
            ctx, badge_x + badge_w * 0.5f, badge_y + badge_h * 0.5f + 1.f, 
            radius * 0.5f, radius + 3.f,
            shadow_outer, shadow_inner
        );
        
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, badge_x, badge_y + 1.f, badge_w, badge_h, radius);
        nvgFillPaint(ctx, shadow);
        nvgFill(ctx);
        
        // Draw badge background
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, badge_x, badge_y, badge_w, badge_h, radius);
        nvgFillColor(ctx, nvgRGBAf(m_badge_color.r(), m_badge_color.g(), 
                                   m_badge_color.b(), m_badge_color.w()));
        nvgFill(ctx);
        
        // Draw count text
        std::string text = m_count > 99 ? "99+" : std::to_string(m_count);
        
        nvgFontSize(ctx, 11.f);
        nvgFontFace(ctx, "sans-bold");
        nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(ctx, badge_x + badge_w * 0.5f, badge_y + badge_h * 0.5f, 
                text.c_str(), nullptr);
    }
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
