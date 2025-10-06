/*
    src/fluent_snackbar.cpp -- Fluent Design Snackbar implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_snackbar.h>
#include <chrono>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

NAMESPACE_BEGIN(nanogui)

FluentSnackbar::FluentSnackbar(Widget *parent, const std::string &message)
    : Widget(parent), m_message(message), m_duration_ms(4000), 
      m_showing(false), m_animation_progress(0.f) {
    set_visible(false);
}

void FluentSnackbar::show() {
    m_showing = true;
    m_show_time = std::chrono::steady_clock::now();
    m_animation_progress = 0.f;
    set_visible(true);
    
    // Position at bottom center of parent
    if (m_parent) {
        Screen *screen = dynamic_cast<Screen*>(m_parent);
        if (screen) {
            Vector2i pref = preferred_size(screen->nvg_context());
            int x = (m_parent->width() - pref.x()) / 2;
            int y = m_parent->height() - pref.y() - 16;
            set_position(Vector2i(x, y));
        }
    }
}

void FluentSnackbar::hide() {
    m_showing = false;
    set_visible(false);
}

Vector2i FluentSnackbar::preferred_size_impl(NVGcontext *ctx) const {
    int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans");
    
    float text_width = nvgTextBounds(ctx, 0, 0, m_message.c_str(), nullptr, nullptr);
    
    int width = 16 + (int)text_width + 16; // Padding
    
    // Add action button width if present
    if (!m_action_text.empty()) {
        float action_width = nvgTextBounds(ctx, 0, 0, m_action_text.c_str(), nullptr, nullptr);
        width += 16 + (int)action_width + 16;
    }
    
    // Snackbar constraints: min 344px, max 672px
    width = std::max(344, std::min(672, width));
    
    int height = 48; // Standard snackbar height
    
    return Vector2i(width, height);
}

bool FluentSnackbar::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!m_enabled || button != GLFW_MOUSE_BUTTON_1 || !down)
        return false;
    
    // Check if action button was clicked
    if (!m_action_text.empty()) {
        Screen *screen = dynamic_cast<Screen*>(m_parent);
        if (!screen) return false;
        
        int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
        NVGcontext *ctx = screen->nvg_context();
        nvgFontSize(ctx, font_size);
        nvgFontFace(ctx, "sans-bold");
        
        float action_width = nvgTextBounds(ctx, 0, 0, m_action_text.c_str(), nullptr, nullptr);
        float action_x = m_size.x() - 16.f - action_width - 16.f;
        float action_y = 0.f;
        float action_h = m_size.y();
        
        if (p.x() >= action_x && p.x() <= m_size.x() - 16 &&
            p.y() >= action_y && p.y() <= action_h) {
            if (m_action_callback)
                m_action_callback();
            hide();
            return true;
        }
    }
    
    return false;
}

void FluentSnackbar::draw(NVGcontext *ctx) {
    if (!m_showing)
        return;
    
    // Check for auto-dismiss
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_show_time).count();
    
    if (elapsed > m_duration_ms) {
        hide();
        return;
    }
    
    // Update animation progress
    float anim_duration = 200.f; // 200ms animation
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
    
    // Apply slide-up animation
    float offset = (1.f - m_animation_progress) * 20.f;
    y += offset;
    
    nvgSave(ctx);
    
    // Draw shadow
    NVGcolor shadow_outer = nvgRGBAf(0.f, 0.f, 0.f, 0.3f * m_animation_progress);
    NVGcolor shadow_inner = nvgRGBAf(0.f, 0.f, 0.f, 0.f);
    
    NVGpaint shadow = nvgBoxGradient(ctx, x, y + 2.f, w, h, 4.f, 8.f, shadow_outer, shadow_inner);
    
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x - 4.f, y - 4.f, w + 8.f, h + 8.f, 4.f);
    nvgFillPaint(ctx, shadow);
    nvgFill(ctx);
    
    // Draw snackbar background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, 4.f);
    nvgFillColor(ctx, nvgRGBAf(0.2f, 0.2f, 0.2f, 0.95f * m_animation_progress));
    nvgFill(ctx);
    
    // Draw message text
    int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans");
    nvgFillColor(ctx, nvgRGBAf(1.f, 1.f, 1.f, m_animation_progress));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(ctx, x + 16.f, y + h * 0.5f, m_message.c_str(), nullptr);
    
    // Draw action button if present
    if (!m_action_text.empty()) {
        nvgFontFace(ctx, "sans-bold");
        nvgFillColor(ctx, nvgRGBAf(0.5f, 0.8f, 1.f, m_animation_progress)); // Primary color
        nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, x + w - 16.f, y + h * 0.5f, m_action_text.c_str(), nullptr);
    }
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
