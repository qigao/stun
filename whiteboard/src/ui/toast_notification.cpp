#include "whiteboard/ui/toast_notification.h"
#include <nanogui/opengl.h>
#include <nanogui/theme.h>
#include <nanovg.h>

namespace whiteboard {

ToastNotification::ToastNotification(nanogui::Widget* parent)
    : Widget(parent), m_type(Type::Info), m_alpha(0.0f), m_target_alpha(0.0f),
      m_duration(3.0f), m_elapsed(0.0f) {
    set_visible(false);
    set_fixed_size(nanogui::Vector2i(300, 60));
}

void ToastNotification::show(const std::string& message, Type type, float duration) {
    m_message = message;
    m_type = type;
    m_duration = duration;
    m_elapsed = 0.0f;
    m_target_alpha = 1.0f;
    set_visible(true);
}

void ToastNotification::hide() {
    m_target_alpha = 0.0f;
}

void ToastNotification::update_animation(float dt) {
    // Update elapsed time
    m_elapsed += dt;
    
    // Auto-hide after duration
    if (m_elapsed >= m_duration && m_target_alpha > 0.0f) {
        m_target_alpha = 0.0f;
    }
    
    // Animate alpha
    float alpha_speed = 5.0f;  // Fade speed
    if (m_alpha < m_target_alpha) {
        m_alpha = std::min(m_alpha + dt * alpha_speed, m_target_alpha);
    } else if (m_alpha > m_target_alpha) {
        m_alpha = std::max(m_alpha - dt * alpha_speed, m_target_alpha);
    }
    
    // Hide widget when fully faded out
    if (m_alpha <= 0.0f && m_target_alpha <= 0.0f) {
        set_visible(false);
    }
}

nanogui::Color ToastNotification::get_background_color() const {
    switch (m_type) {
        case Type::Success:
            return nanogui::Color(76, 175, 80, 230);   // Green
        case Type::Warning:
            return nanogui::Color(255, 152, 0, 230);   // Orange
        case Type::Error:
            return nanogui::Color(244, 67, 54, 230);   // Red
        case Type::Info:
        default:
            return nanogui::Color(33, 150, 243, 230);  // Blue
    }
}

nanogui::Color ToastNotification::get_text_color() const {
    return nanogui::Color(255, 255, 255, 255);  // White
}

int ToastNotification::get_icon() const {
    switch (m_type) {
        case Type::Success:
            return 0xf058;  // FA_CHECK_CIRCLE
        case Type::Warning:
            return 0xf071;  // FA_EXCLAMATION_TRIANGLE
        case Type::Error:
            return 0xf057;  // FA_TIMES_CIRCLE
        case Type::Info:
        default:
            return 0xf05a;  // FA_INFO_CIRCLE
    }
}

void ToastNotification::draw(NVGcontext* ctx) {
    if (!visible())
        return;
    
    // Update animation (approximate dt as 1/60)
    update_animation(1.0f / 60.0f);
    
    nvgSave(ctx);
    
    float x = m_pos.x();
    float y = m_pos.y();
    float w = m_size.x();
    float h = m_size.y();
    
    // Apply alpha to all drawing
    nvgGlobalAlpha(ctx, m_alpha);
    
    // Draw shadow
    NVGpaint shadow_paint = nvgBoxGradient(ctx, x, y + 2, w, h, 8, 10,
                                          nvgRGBA(0, 0, 0, 128), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x - 5, y - 5, w + 10, h + 10, 10);
    nvgFillPaint(ctx, shadow_paint);
    nvgFill(ctx);
    
    // Draw background
    auto bg_color = get_background_color();
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, 8);
    nvgFillColor(ctx, nvgRGBA(bg_color.r(), bg_color.g(), bg_color.b(), 
                              static_cast<int>(bg_color.a() * m_alpha)));
    nvgFill(ctx);
    
    // Draw icon
    nvgFontSize(ctx, 24.0f);
    nvgFontFace(ctx, "icons");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    auto text_color = get_text_color();
    nvgFillColor(ctx, nvgRGBA(text_color.r(), text_color.g(), text_color.b(),
                              static_cast<int>(text_color.a() * m_alpha)));
    
    int icon = get_icon();
    std::string icon_str = nanogui::utf8(icon);
    nvgText(ctx, x + 15, y + h / 2, icon_str.c_str(), nullptr);
    
    // Draw message text
    nvgFontSize(ctx, 14.0f);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(ctx, x + 50, y + h / 2, m_message.c_str(), nullptr);
    
    nvgRestore(ctx);
    
    Widget::draw(ctx);
}

} // namespace whiteboard
