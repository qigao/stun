/*
    src/m3_icon_button.cpp -- M3 Icon Button implementation
*/

#include <nanogui/m3_icon_button.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3IconButton::M3IconButton(Widget *parent, int icon, Variant variant)
    : Button(parent, "", icon), m_variant(variant) {
    set_fixed_size(Vector2i(40, 40));
}

M3Theme *M3IconButton::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

Vector2i M3IconButton::preferred_size_impl(NVGcontext *) const {
    return Vector2i(40, 40);
}

void M3IconButton::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) { Button::draw(ctx); return; }

    float x = m_pos.x(), y = m_pos.y(), w = m_size.x(), h = m_size.y();
    nvgSave(ctx);

    Color bg, fg;
    if (!m_enabled) {
        bg = Color(theme->on_surface().r(), theme->on_surface().g(), theme->on_surface().b(), 0.12f);
        fg = Color(theme->on_surface().r(), theme->on_surface().g(), theme->on_surface().b(), 0.38f);
    } else {
        switch (m_variant) {
            case Variant::Filled:
                bg = theme->primary(); fg = theme->on_primary(); break;
            case Variant::Tonal:
                bg = theme->secondary_container(); fg = theme->on_secondary_container(); break;
            case Variant::Outlined:
                bg = Color(0,0,0,0); fg = theme->on_surface_variant(); break;
            default:
                bg = Color(0,0,0,0); fg = theme->on_surface_variant(); break;
        }
    }

    if (bg.a() > 0) {
        nvgBeginPath(ctx);
        nvgCircle(ctx, x + w*0.5f, y + h*0.5f, w*0.5f);
        nvgFillColor(ctx, bg);
        nvgFill(ctx);
    }

    if (m_variant == Variant::Outlined && m_enabled) {
        nvgBeginPath(ctx);
        nvgCircle(ctx, x + w*0.5f, y + h*0.5f, w*0.5f - 0.5f);
        nvgStrokeWidth(ctx, 1.0f);
        nvgStrokeColor(ctx, theme->outline());
        nvgStroke(ctx);
    }

    if ((m_mouse_focus || m_focused) && m_enabled) {
        Color state = theme->state_layer(fg, m_pushed ? 0.12f : 0.08f);
        nvgBeginPath(ctx);
        nvgCircle(ctx, x + w*0.5f, y + h*0.5f, w*0.5f);
        nvgFillColor(ctx, state);
        nvgFill(ctx);
    }

    nvgFontSize(ctx, 24);
    nvgFontFace(ctx, "icons");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, fg);
    nvgText(ctx, x + w*0.5f, y + h*0.5f, utf8(m_icon).data(), nullptr);

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
