/*
    src/m3_side_sheet.cpp -- M3 Side Sheet implementation
*/

#include <nanogui/m3_side_sheet.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3SideSheet::M3SideSheet(Widget *parent, const std::string &title, Side side)
    : Popup(parent), m_title(title), m_side(side) {
    set_modal(true);
}

M3Theme *M3SideSheet::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3SideSheet::show() {
    if (m_parent) {
        int width = 400;
        int height = m_parent->height();
        int x_pos = (m_side == Side::Right) ? (m_parent->width() - width) : 0;
        set_position(Vector2i(x_pos, 0));
        set_size(Vector2i(width, height));
    }
    set_visible(true);
}

void M3SideSheet::hide() {
    set_visible(false);
}

Vector2i M3SideSheet::preferred_size_impl(NVGcontext *) const {
    return Vector2i(400, 600);
}

void M3SideSheet::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) { Popup::draw(ctx); return; }

    float x = m_pos.x(), y = m_pos.y(), w = m_size.x(), h = m_size.y();

    nvgSave(ctx);

    // Scrim
    if (m_modal) {
        nvgBeginPath(ctx);
        nvgRect(ctx, 0, 0, m_parent->width(), m_parent->height());
        nvgFillColor(ctx, Color(theme->scrim().r(), theme->scrim().g(), theme->scrim().b(), 0.32f));
        nvgFill(ctx);
    }

    // Sheet background
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, theme->surface());
    nvgFill(ctx);

    // Elevation tint
    Color tint = theme->elevation_tint(M3Theme::Elevation::Level1);
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, tint);
    nvgFill(ctx);

    // Title
    if (!m_title.empty()) {
        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "sans-bold");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(ctx, theme->on_surface());
        nvgText(ctx, x + 24, y + 24, m_title.c_str(), nullptr);
    }

    nvgRestore(ctx);
    Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
