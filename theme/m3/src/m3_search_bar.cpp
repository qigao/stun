/*
    src/m3_search_bar.cpp -- M3 Search Bar implementation
*/

#include <nanogui/m3_search_bar.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3SearchBar::M3SearchBar(Widget *parent, const std::string &placeholder)
    : TextBox(parent), m_placeholder(placeholder) {
    set_fixed_height(56);
}

M3Theme *M3SearchBar::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

Vector2i M3SearchBar::preferred_size(NVGcontext *ctx) const {
    nvgFontSize(ctx, 16);
    nvgFontFace(ctx, "sans");
    float tw = nvgTextBounds(ctx, 0, 0, m_placeholder.c_str(), nullptr, nullptr);
    return Vector2i(static_cast<int>(tw) + 120, 56);
}

void M3SearchBar::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) { TextBox::draw(ctx); return; }

    float x = m_pos.x(), y = m_pos.y(), w = m_size.x(), h = m_size.y();
    float corner = theme->corner_radius(M3Theme::ShapeFamily::ExtraLarge);

    nvgSave(ctx);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, corner);
    nvgFillColor(ctx, theme->surface_variant());
    nvgFill(ctx);

    if (m_focused) {
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x+1, y+1, w-2, h-2, corner-1);
        nvgStrokeWidth(ctx, 2.0f);
        nvgStrokeColor(ctx, theme->primary());
        nvgStroke(ctx);
    }

    float content_x = x + 16;
    if (m_leading_icon) {
        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, theme->on_surface_variant());
        nvgText(ctx, content_x, y + h*0.5f, utf8(m_leading_icon).data(), nullptr);
        content_x += 40;
    }

    std::string display_text = m_value.empty() ? m_placeholder : m_value;
    Color text_color = m_value.empty() ? theme->on_surface_variant() : theme->on_surface();

    nvgFontSize(ctx, 16);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, text_color);
    nvgText(ctx, content_x, y + h*0.5f, display_text.c_str(), nullptr);

    if (m_focused && !m_value.empty()) {
        float cursor_x = content_x + nvgTextBounds(ctx, 0, 0, m_value.c_str(), nullptr, nullptr);
        nvgBeginPath(ctx);
        nvgRect(ctx, cursor_x, y + h*0.25f, 1, h*0.5f);
        nvgFillColor(ctx, theme->primary());
        nvgFill(ctx);
    }

    if (m_trailing_icon) {
        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, theme->on_surface_variant());
        nvgText(ctx, x + w - 16, y + h*0.5f, utf8(m_trailing_icon).data(), nullptr);
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
