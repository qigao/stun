/*
    src/m3_top_app_bar.cpp -- M3 Top App Bar implementation
*/

#include <nanogui/m3_top_app_bar.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3TopAppBar::M3TopAppBar(Widget *parent, const std::string &title, Type type)
    : Widget(parent), m_title(title), m_type(type) {
    set_type(type);
}

M3Theme *M3TopAppBar::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3TopAppBar::set_type(Type type) {
    m_type = type;
    int height = (type == Type::Small) ? 64 : (type == Type::Medium) ? 112 : 152;
    set_fixed_height(height);
}

Vector2i M3TopAppBar::preferred_size(NVGcontext *) const {
    int height = (m_type == Type::Small) ? 64 : (m_type == Type::Medium) ? 112 : 152;
    return Vector2i(0, height);
}

void M3TopAppBar::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) { Widget::draw(ctx); return; }

    float x = m_pos.x(), y = m_pos.y(), w = m_size.x(), h = m_size.y();

    nvgSave(ctx);

    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, theme->surface());
    nvgFill(ctx);

    float title_y = (m_type == Type::Small) ? y + h*0.5f : y + h - 28;
    float title_x = x + (m_leading_icon ? 72 : 16);

    if (m_leading_icon) {
        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, theme->on_surface());
        nvgText(ctx, x + 36, y + 32, utf8(m_leading_icon).data(), nullptr);
    }

    int title_size = (m_type == Type::Large) ? 28 : (m_type == Type::Medium) ? 24 : 22;
    nvgFontSize(ctx, title_size);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, theme->on_surface());
    nvgText(ctx, title_x, title_y, m_title.c_str(), nullptr);

    if (m_trailing_icon) {
        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, theme->on_surface_variant());
        nvgText(ctx, x + w - 36, y + 32, utf8(m_trailing_icon).data(), nullptr);
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
