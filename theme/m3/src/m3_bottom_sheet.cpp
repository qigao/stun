/*
    src/m3_bottom_sheet.cpp -- M3 Bottom Sheet implementation
*/

#include <nanogui/m3_bottom_sheet.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3BottomSheet::M3BottomSheet(Widget *parent, const std::string &title)
    : Popup(parent), m_title(title) {
    set_modal(true);
}

M3Theme *M3BottomSheet::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3BottomSheet::show() {
    if (m_parent) {
        int width = m_parent->width();
        int height = m_parent->height() / 2;
        set_position(Vector2i(0, m_parent->height() - height));
        set_size(Vector2i(width, height));
    }
    set_visible(true);
}

void M3BottomSheet::hide() {
    set_visible(false);
}

Vector2i M3BottomSheet::preferred_size(NVGcontext *) const {
    return Vector2i(400, 300);
}

void M3BottomSheet::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) { Popup::draw(ctx); return; }

    float x = m_pos.x(), y = m_pos.y(), w = m_size.x(), h = m_size.y();
    float corner = theme->corner_radius(M3Theme::ShapeFamily::ExtraLarge);

    nvgSave(ctx);

    if (m_modal) {
        nvgBeginPath(ctx);
        nvgRect(ctx, 0, 0, m_parent->width(), m_parent->height());
        nvgFillColor(ctx, Color(theme->scrim().r(), theme->scrim().g(), theme->scrim().b(), 0.32f));
        nvgFill(ctx);
    }

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x, y + corner);
    nvgArcTo(ctx, x, y, x + corner, y, corner);
    nvgLineTo(ctx, x + w - corner, y);
    nvgArcTo(ctx, x + w, y, x + w, y + corner, corner);
    nvgLineTo(ctx, x + w, y + h);
    nvgLineTo(ctx, x, y + h);
    nvgClosePath(ctx);
    nvgFillColor(ctx, theme->surface());
    nvgFill(ctx);

    Color tint = theme->elevation_tint(M3Theme::Elevation::Level1);
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x, y + corner);
    nvgArcTo(ctx, x, y, x + corner, y, corner);
    nvgLineTo(ctx, x + w - corner, y);
    nvgArcTo(ctx, x + w, y, x + w, y + corner, corner);
    nvgLineTo(ctx, x + w, y + h);
    nvgLineTo(ctx, x, y + h);
    nvgClosePath(ctx);
    nvgFillColor(ctx, tint);
    nvgFill(ctx);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x + w*0.5f - 16, y + 12, 32, 4, 2);
    nvgFillColor(ctx, theme->on_surface_variant());
    nvgFill(ctx);

    if (!m_title.empty()) {
        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "sans-bold");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(ctx, theme->on_surface());
        nvgText(ctx, x + w*0.5f, y + 32, m_title.c_str(), nullptr);
    }

    nvgRestore(ctx);
    Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
