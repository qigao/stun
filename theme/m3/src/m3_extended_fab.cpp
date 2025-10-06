/*
    src/m3_extended_fab.cpp -- M3 Extended FAB implementation
*/

#include <nanogui/m3_extended_fab.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3ExtendedFAB::M3ExtendedFAB(Widget *parent, const std::string &label, int icon, Size size)
    : Button(parent, label, icon), m_size_type(size) {
    set_size_type(size);
}

M3Theme *M3ExtendedFAB::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3ExtendedFAB::set_size_type(Size size) {
    m_size_type = size;
    int height = (size == Size::Large) ? 96 : 56;
    set_fixed_height(height);
}

Vector2i M3ExtendedFAB::preferred_size(NVGcontext *ctx) const {
    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, "sans-bold");
    
    float text_width = 0;
    if (m_expanded && !m_caption.empty()) {
        text_width = nvgTextBounds(ctx, 0, 0, m_caption.c_str(), nullptr, nullptr);
    }
    
    int height = (m_size_type == Size::Large) ? 96 : 56;
    int padding = (m_size_type == Size::Large) ? 32 : 20;
    int icon_space = m_icon ? 24 : 0;
    int gap = (m_icon && m_expanded && !m_caption.empty()) ? 12 : 0;
    
    int width = padding * 2 + icon_space + gap + static_cast<int>(text_width);
    
    return Vector2i(width, height);
}

void M3ExtendedFAB::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) { Button::draw(ctx); return; }

    float x = m_pos.x(), y = m_pos.y(), w = m_size.x(), h = m_size.y();
    float corner = theme->corner_radius(M3Theme::ShapeFamily::Large);

    nvgSave(ctx);

    Color bg, fg;
    if (!m_enabled) {
        bg = Color(theme->on_surface().r(), theme->on_surface().g(), theme->on_surface().b(), 0.12f);
        fg = Color(theme->on_surface().r(), theme->on_surface().g(), theme->on_surface().b(), 0.38f);
    } else {
        bg = theme->primary_container();
        fg = theme->on_primary_container();
    }

    // Background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, corner);
    nvgFillColor(ctx, bg);
    nvgFill(ctx);

    // Elevation
    if (m_enabled) {
        Color tint = theme->elevation_tint(M3Theme::Elevation::Level3);
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, w, h, corner);
        nvgFillColor(ctx, tint);
        nvgFill(ctx);
    }

    // State layer
    if ((m_mouse_focus || m_focused) && m_enabled) {
        Color state = theme->state_layer(fg, m_pushed ? 0.12f : 0.08f);
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, w, h, corner);
        nvgFillColor(ctx, state);
        nvgFill(ctx);
    }

    // Content
    float content_x = x + w * 0.5f;
    
    if (m_icon && m_expanded && !m_caption.empty()) {
        // Icon + text
        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, fg);
        nvgText(ctx, content_x - 6, y + h * 0.5f, utf8(m_icon).data(), nullptr);
        
        nvgFontSize(ctx, 14);
        nvgFontFace(ctx, "sans-bold");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, content_x + 6, y + h * 0.5f, m_caption.c_str(), nullptr);
    } else if (m_icon) {
        // Icon only
        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, fg);
        nvgText(ctx, content_x, y + h * 0.5f, utf8(m_icon).data(), nullptr);
    } else if (m_expanded && !m_caption.empty()) {
        // Text only
        nvgFontSize(ctx, 14);
        nvgFontFace(ctx, "sans-bold");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, fg);
        nvgText(ctx, content_x, y + h * 0.5f, m_caption.c_str(), nullptr);
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
