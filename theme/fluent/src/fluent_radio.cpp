#include <nanogui/opengl.h>
#include <nanogui/fluent_radio.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/screen.h>

NAMESPACE_BEGIN(nanogui)

FluentRadio::FluentRadio(Widget *parent, const std::string &caption)
    : Widget(parent), m_caption(caption), m_checked(false), 
      m_enabled(true), m_group(0) {
}

void FluentRadio::set_checked(bool checked) {
    if (m_checked == checked)
        return;
    
    m_checked = checked;
    
    // Uncheck other radios in the same group
    if (m_checked && m_parent) {
        for (auto child : m_parent->children()) {
            auto radio = dynamic_cast<FluentRadio*>(child);
            if (radio && radio != this && radio->group() == m_group) {
                radio->set_checked(false);
            }
        }
    }
    
    if (m_callback)
        m_callback(m_checked);
}

Vector2i FluentRadio::preferred_size_impl(NVGcontext *ctx) const {
    int font_size = 16;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans");
    
    float text_width = 0.f;
    if (!m_caption.empty()) {
        text_width = nvgTextBounds(ctx, 0, 0, m_caption.c_str(), nullptr, nullptr);
    }
    
    return Vector2i(20 + (text_width > 0 ? 8 + text_width : 0), 20);
}

bool FluentRadio::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!m_enabled || button != GLFW_MOUSE_BUTTON_1)
        return false;
    
    if (down && !m_checked) {
        set_checked(true);
        return true;
    }
    
    return false;
}

void FluentRadio::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    auto theme = dynamic_cast<FluentTheme*>(m_theme.get());
    if (!theme) return;
    
    float radius = 10.f;
    float cx = m_pos.x() + radius;
    float cy = m_pos.y() + m_size.y() * 0.5f;
    
    // Outer circle
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, radius);
    
    Color border_color = m_enabled ? 
        (m_checked ? theme->primary_color() : Color(0.6f, 0.6f, 0.6f, 1.f)) :
        Color(0.4f, 0.4f, 0.4f, 1.f);
    
    nvgStrokeWidth(ctx, 2.f);
    nvgStrokeColor(ctx, border_color);
    nvgStroke(ctx);
    
    // Inner circle (when checked)
    if (m_checked) {
        nvgBeginPath(ctx);
        nvgCircle(ctx, cx, cy, radius * 0.5f);
        nvgFillColor(ctx, m_enabled ? theme->primary_color() : Color(0.4f, 0.4f, 0.4f, 1.f));
        nvgFill(ctx);
    }
    
    // Caption
    if (!m_caption.empty()) {
        nvgFontSize(ctx, 16.f);
        nvgFontFace(ctx, "sans");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        
        Color text_color = m_enabled ? 
            theme->on_surface_color() : 
            Color(theme->on_surface_color().r(), theme->on_surface_color().g(), 
                  theme->on_surface_color().b(), 0.38f);
        
        nvgFillColor(ctx, text_color);
        nvgText(ctx, cx + radius + 8, cy, m_caption.c_str(), nullptr);
    }
}

NAMESPACE_END(nanogui)
