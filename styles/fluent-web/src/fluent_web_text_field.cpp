#include <nanogui/fluent_web_text_field.h>

#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#include <algorithm>

NAMESPACE_BEGIN(nanogui)

namespace {

NVGcolor to_nvg(const Color &c, float override_alpha = -1.f) {
    float a = override_alpha >= 0.f ? override_alpha : c.w();
    return nvgRGBAf(c.r(), c.g(), c.b(), a);
}

} // namespace

FluentWebTextField::FluentWebTextField(Widget *parent,
                                       const std::string &value,
                                       const std::string &placeholder)
    : TextBox(parent, value) {
    set_placeholder(placeholder);
    update_metrics();
}

void FluentWebTextField::set_theme(Theme *theme) {
    TextBox::set_theme(theme);
    update_metrics();
}

void FluentWebTextField::update_metrics() {
    const auto *fluent =
        dynamic_cast<const FluentWebTheme *>(this->theme());
    if (!fluent)
        return;

    const auto &body = fluent->typography(
        FluentWebTheme::TypographyToken::Body2);
    set_font_size(static_cast<int>(std::round(body.font_size)));
    set_corner_radius(fluent->corner_radius(
        FluentWebTheme::RadiusToken::Medium));
}

Vector2i FluentWebTextField::preferred_size_impl(NVGcontext *ctx) const {
    return TextBox::preferred_size_impl(ctx);
}

void FluentWebTextField::draw(NVGcontext *ctx) {
    auto *fluent = dynamic_cast<FluentWebTheme *>(this->theme());
    if (!fluent) {
        TextBox::draw(ctx);
        return;
    }

    Widget::draw(ctx);

    auto c = [&](FluentWebTheme::ColorToken token) -> Color {
        return fluent->color(token);
    };

    const bool disabled = !m_enabled;
    const bool focus = focused();
    const bool hovered = m_mouse_focus && !disabled;
    const bool invalid = !m_valid_format && m_editable;

    Color background = c(FluentWebTheme::ColorToken::colorNeutralBackground1);
    Color border = c(FluentWebTheme::ColorToken::colorNeutralStroke1);
    Color text_color = c(FluentWebTheme::ColorToken::colorNeutralForeground1);
    Color placeholder_color = c(FluentWebTheme::ColorToken::colorNeutralForeground3);
    Color units_color = placeholder_color;
    Color focus_outer = c(FluentWebTheme::ColorToken::colorStrokeFocus1);
    Color focus_inner = c(FluentWebTheme::ColorToken::colorStrokeFocus2);

    if (disabled) {
        background = c(FluentWebTheme::ColorToken::colorNeutralBackgroundDisabled);
        border = c(FluentWebTheme::ColorToken::colorNeutralStrokeDisabled);
        text_color = c(FluentWebTheme::ColorToken::colorNeutralForegroundDisabled);
        placeholder_color = text_color;
        units_color = text_color;
    } else if (invalid) {
        background = c(FluentWebTheme::ColorToken::colorStatusDangerBackground2);
        border = c(FluentWebTheme::ColorToken::colorStatusDangerBorder2);
    } else if (focus) {
        border = c(FluentWebTheme::ColorToken::colorBrandStroke1);
    } else if (hovered) {
        background = c(FluentWebTheme::ColorToken::colorNeutralBackground1Hover);
        border = c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessibleHover);
    }

    const float radius = m_corner_radius;
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());

    if (focus && !disabled) {
        nvgSave(ctx);
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x - 2.5f, y - 2.5f, w + 5.f, h + 5.f, radius + 3.f);
        nvgStrokeWidth(ctx, 2.f);
        nvgStrokeColor(ctx, to_nvg(focus_outer));
        nvgStroke(ctx);

        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x - 1.5f, y - 1.5f, w + 3.f, h + 3.f, radius + 2.f);
        nvgStrokeWidth(ctx, 1.4f);
        nvgStrokeColor(ctx, to_nvg(focus_inner));
        nvgStroke(ctx);
        nvgRestore(ctx);
    }

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, radius);
    nvgFillColor(ctx, to_nvg(background));
    nvgFill(ctx);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx,
                   x + 0.5f,
                   y + 0.5f,
                   w - 1.f,
                   h - 1.f,
                   std::max(radius - 0.5f, 0.f));
    nvgStrokeWidth(ctx, 1.f);
    nvgStrokeColor(ctx, to_nvg(border));
    nvgStroke(ctx);

    nvgFontSize(ctx, static_cast<float>(font_size()));
    nvgFontFace(ctx, "sans");
    Vector2i draw_pos(m_pos.x(), m_pos.y() + m_size.y() * 0.5f + 1);

    float x_spacing = m_size.y() * 0.2f;
    float unit_width = 0.f;

    nvgFillColor(ctx, to_nvg(units_color));

    if (m_units_image > 0) {
        int iw, ih;
        nvgImageSize(ctx, m_units_image, &iw, &ih);
        float unit_height = m_size.y() * 0.4f;
        unit_width = iw * unit_height / ih;
        NVGpaint img_paint = nvgImagePattern(
            ctx, m_pos.x() + m_size.x() - x_spacing - unit_width,
            draw_pos.y() - unit_height * 0.5f, unit_width, unit_height, 0,
            m_units_image, disabled ? 0.35f : 0.75f);
        nvgBeginPath(ctx);
        nvgRect(ctx, m_pos.x() + m_size.x() - x_spacing - unit_width,
                draw_pos.y() - unit_height * 0.5f, unit_width, unit_height);
        nvgFillPaint(ctx, img_paint);
        nvgFill(ctx);
        unit_width += 2.f;
    } else if (!m_units.empty()) {
        unit_width = nvgTextBounds(ctx, 0.f, 0.f, m_units.c_str(), nullptr, nullptr);
        nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, m_pos.x() + m_size.x() - x_spacing, draw_pos.y(),
                m_units.c_str(), nullptr);
        unit_width += 2.f;
    }

    float spin_arrows_width = 0.f;
    if (m_spinnable && !focus) {
        spin_arrows_width = 14.f;

        nvgFontFace(ctx, "icons");
        nvgFontSize(ctx,
                    ((m_font_size < 0) ? m_theme->m_button_font_size : m_font_size) *
                        icon_scale());

        bool spinning = m_mouse_down_pos.x() != -1;

        auto draw_spin_icon = [&](SpinArea area, int icon_id) {
            bool hover = m_mouse_focus && spin_area(m_mouse_pos) == area;
            Color icon_color = (m_enabled && (hover || spinning))
                                   ? text_color
                                   : placeholder_color;
            nvgFillColor(ctx, to_nvg(icon_color));
            auto icon = utf8(icon_id);
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        };

        draw_spin_icon(SpinArea::Top, m_theme->m_text_box_up_icon);
        Vector2f icon_pos_top(m_pos.x() + 4.f,
                              m_pos.y() + m_size.y() / 2.f - x_spacing / 2.f);
        nvgText(ctx, icon_pos_top.x(), icon_pos_top.y(),
                utf8(m_theme->m_text_box_up_icon).data(), nullptr);

        draw_spin_icon(SpinArea::Bottom, m_theme->m_text_box_down_icon);
        Vector2f icon_pos_bottom(m_pos.x() + 4.f,
                                 m_pos.y() + m_size.y() / 2.f + x_spacing / 2.f + 1.5f);
        nvgText(ctx, icon_pos_bottom.x(), icon_pos_bottom.y(),
                utf8(m_theme->m_text_box_down_icon).data(), nullptr);

        nvgFontFace(ctx, "sans");
        nvgFontSize(ctx, static_cast<float>(font_size()));
    }

    switch (m_alignment) {
        case Alignment::Left:
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            draw_pos.x() += x_spacing + spin_arrows_width;
            break;
        case Alignment::Right:
            nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            draw_pos.x() += m_size.x() - unit_width - x_spacing;
            break;
        case Alignment::Center:
        default:
            nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            draw_pos.x() += m_size.x() * 0.5f;
            break;
    }

    Color final_text_color = disabled ? text_color : text_color;
    nvgFillColor(ctx, to_nvg(final_text_color));

    float clip_x = m_pos.x() + x_spacing + spin_arrows_width - 1.f;
    float clip_y = m_pos.y() + 1.f;
    float clip_width = m_size.x() - unit_width - spin_arrows_width - 2.f * x_spacing + 2.f;
    float clip_height = m_size.y() - 3.f;

    nvgSave(ctx);
    nvgIntersectScissor(ctx, clip_x, clip_y, clip_width, clip_height);

    Vector2i old_draw_pos(draw_pos);
    draw_pos.x() += m_text_offset;

    if (m_committed) {
        if (m_value.empty()) {
            nvgFillColor(ctx, to_nvg(placeholder_color));
            nvgText(ctx, draw_pos.x(), draw_pos.y(), m_placeholder.c_str(), nullptr);
        } else {
            nvgFillColor(ctx, to_nvg(final_text_color));
            nvgText(ctx, draw_pos.x(), draw_pos.y(), m_value.c_str(), nullptr);
        }
    } else {
        const int max_glyphs = 1024;
        NVGglyphPosition glyphs[max_glyphs];
        float text_bound[4];
        nvgFillColor(ctx, to_nvg(final_text_color));
        nvgTextBounds(ctx, draw_pos.x(), draw_pos.y(), m_value_temp.c_str(),
                      nullptr, text_bound);
        float lineh = text_bound[3] - text_bound[1];

        int nglyphs =
            nvgTextGlyphPositions(ctx, draw_pos.x(), draw_pos.y(),
                                  m_value_temp.c_str(), nullptr, glyphs, max_glyphs);
        update_cursor(ctx, text_bound[2], glyphs, nglyphs);

        int prev_cpos = m_cursor_pos > 0 ? m_cursor_pos - 1 : 0;
        int next_cpos = m_cursor_pos < nglyphs ? m_cursor_pos + 1 : nglyphs;
        float prev_cx = cursor_index_to_position(prev_cpos, text_bound[2], glyphs, nglyphs);
        float next_cx = cursor_index_to_position(next_cpos, text_bound[2], glyphs, nglyphs);

        if (next_cx > clip_x + clip_width)
            m_text_offset -= next_cx - (clip_x + clip_width) + 1;
        if (prev_cx < clip_x)
            m_text_offset += clip_x - prev_cx + 1;

        draw_pos.x() = old_draw_pos.x() + m_text_offset;

        nvgText(ctx, draw_pos.x(), draw_pos.y(), m_value_temp.c_str(), nullptr);
        nvgTextBounds(ctx, draw_pos.x(), draw_pos.y(), m_value_temp.c_str(),
                      nullptr, text_bound);
        nglyphs = nvgTextGlyphPositions(ctx, draw_pos.x(), draw_pos.y(),
                                        m_value_temp.c_str(), nullptr, glyphs, max_glyphs);

        if (m_cursor_pos > -1) {
            if (m_selection_pos > -1) {
                float caretx = cursor_index_to_position(m_cursor_pos, text_bound[2],
                                                        glyphs, nglyphs);
                float selx = cursor_index_to_position(m_selection_pos, text_bound[2],
                                                      glyphs, nglyphs);
                if (caretx > selx)
                    std::swap(caretx, selx);

                nvgBeginPath(ctx);
                Color selection = fluent->color(FluentWebTheme::ColorToken::colorBrandBackground2);
                nvgFillColor(ctx, to_nvg(selection, 0.35f));
                nvgRect(ctx, caretx, draw_pos.y() - lineh * 0.5f,
                        selx - caretx, lineh);
                nvgFill(ctx);
            }

            float caretx = cursor_index_to_position(m_cursor_pos, text_bound[2],
                                                    glyphs, nglyphs);
            nvgBeginPath(ctx);
            nvgMoveTo(ctx, caretx, draw_pos.y() - lineh * 0.5f);
            nvgLineTo(ctx, caretx, draw_pos.y() + lineh * 0.5f);
            nvgStrokeColor(ctx, to_nvg(focus ? focus_inner : final_text_color));
            nvgStrokeWidth(ctx, 1.0f);
            nvgStroke(ctx);
        }
    }
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)

