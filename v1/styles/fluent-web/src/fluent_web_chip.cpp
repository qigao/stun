#include <nanogui/fluent_web_chip.h>

#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#include <algorithm>

NAMESPACE_BEGIN(nanogui)

namespace {

struct ChipState {
    Color background;
    Color border;
    Color text;
    Color icon;
};

struct ChipPalette {
    ChipState assist_normal;
    ChipState assist_hover;
    ChipState assist_pressed;
    ChipState assist_disabled;

    ChipState filter_normal;
    ChipState filter_hover;
    ChipState filter_pressed;
    ChipState filter_selected;
    ChipState filter_selected_hover;
    ChipState filter_selected_pressed;
    ChipState filter_disabled;

    Color focus_inner;
    Color focus_outer;
};

ChipPalette make_palette(const FluentWebTheme &theme) {
    auto c = [&](FluentWebTheme::ColorToken token) {
        return theme.color(token);
    };

    ChipPalette palette{};
    palette.assist_normal = {
        c(FluentWebTheme::ColorToken::colorSubtleBackground),
        c(FluentWebTheme::ColorToken::colorNeutralStroke2),
        c(FluentWebTheme::ColorToken::colorNeutralForeground2),
        c(FluentWebTheme::ColorToken::colorNeutralForeground2)
    };
    palette.assist_hover = {
        c(FluentWebTheme::ColorToken::colorSubtleBackgroundHover),
        c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessibleHover),
        c(FluentWebTheme::ColorToken::colorNeutralForeground1),
        c(FluentWebTheme::ColorToken::colorNeutralForeground1)
    };
    palette.assist_pressed = {
        c(FluentWebTheme::ColorToken::colorSubtleBackgroundPressed),
        c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessiblePressed),
        c(FluentWebTheme::ColorToken::colorNeutralForeground1),
        c(FluentWebTheme::ColorToken::colorNeutralForeground1)
    };
    palette.assist_disabled = {
        c(FluentWebTheme::ColorToken::colorNeutralBackgroundDisabled),
        c(FluentWebTheme::ColorToken::colorNeutralStrokeDisabled),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundDisabled),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundDisabled)
    };

    palette.filter_normal = {
        c(FluentWebTheme::ColorToken::colorSubtleBackground),
        c(FluentWebTheme::ColorToken::colorNeutralStroke2),
        c(FluentWebTheme::ColorToken::colorNeutralForeground2),
        c(FluentWebTheme::ColorToken::colorNeutralForeground2)
    };
    palette.filter_hover = {
        c(FluentWebTheme::ColorToken::colorSubtleBackgroundHover),
        c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessibleHover),
        c(FluentWebTheme::ColorToken::colorNeutralForeground1),
        c(FluentWebTheme::ColorToken::colorNeutralForeground1)
    };
    palette.filter_pressed = {
        c(FluentWebTheme::ColorToken::colorSubtleBackgroundPressed),
        c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessiblePressed),
        c(FluentWebTheme::ColorToken::colorNeutralForeground1),
        c(FluentWebTheme::ColorToken::colorNeutralForeground1)
    };
    palette.filter_selected = {
        c(FluentWebTheme::ColorToken::colorBrandBackground),
        c(FluentWebTheme::ColorToken::colorBrandStroke1),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand)
    };
    palette.filter_selected_hover = {
        c(FluentWebTheme::ColorToken::colorBrandBackgroundHover),
        c(FluentWebTheme::ColorToken::colorBrandStroke1),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand)
    };
    palette.filter_selected_pressed = {
        c(FluentWebTheme::ColorToken::colorBrandBackgroundPressed),
        c(FluentWebTheme::ColorToken::colorBrandStroke1),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand)
    };
    palette.filter_disabled = palette.assist_disabled;

    palette.focus_outer =
        c(FluentWebTheme::ColorToken::colorStrokeFocus1);
    palette.focus_inner =
        c(FluentWebTheme::ColorToken::colorStrokeFocus2);

    return palette;
}

NVGcolor to_nvg(const Color &c) {
    return nvgRGBAf(c.r(), c.g(), c.b(), c.w());
}

} // namespace

FluentWebChip::FluentWebChip(Widget *parent,
                             const std::string &caption,
                             Kind kind)
    : Button(parent, caption),
      m_kind(kind) {
    if (m_kind == Kind::Filter)
        set_flags((flags() | Button::ToggleButton));
    set_icon_position(IconPosition::LeftCentered);
    update_metrics();
}

void FluentWebChip::set_kind(Kind kind) {
    if (kind == m_kind)
        return;
    m_kind = kind;
    if (m_kind == Kind::Filter)
        set_flags(flags() | Button::ToggleButton);
    else
        set_flags(flags() & ~Button::ToggleButton);
    update_metrics();
    preferred_size_changed();
}

void FluentWebChip::set_theme(Theme *theme) {
    Button::set_theme(theme);
    update_metrics();
    preferred_size_changed();
}

void FluentWebChip::update_metrics() {
    auto *fluent = dynamic_cast<FluentWebTheme *>(this->theme());
    if (!fluent)
        return;

    float horizontal = fluent->spacing(FluentWebTheme::SpaceToken::M);
    float vertical = fluent->spacing(FluentWebTheme::SpaceToken::S);
    set_padding(Vector2i(static_cast<int>(std::round(horizontal)),
                         static_cast<int>(std::round(vertical))));

    const auto &body = fluent->typography(
        FluentWebTheme::TypographyToken::Body2);
    m_font_size = static_cast<int>(std::round(body.font_size));
}

Vector2i FluentWebChip::preferred_size_impl(NVGcontext *ctx) const {
    return Button::preferred_size_impl(ctx);
}

void FluentWebChip::draw(NVGcontext *ctx) {
    auto *fluent = dynamic_cast<FluentWebTheme *>(this->theme());
    if (!fluent) {
        Button::draw(ctx);
        return;
    }

    Widget::draw(ctx);

    const ChipPalette palette = make_palette(*fluent);

    const bool disabled = !m_enabled;
    const bool hovered = m_mouse_focus && !disabled;
    const bool pressed = m_pushed && (flags() & Button::ToggleButton) == 0 && !disabled;
    const bool selected = (flags() & Button::ToggleButton) != 0 && m_pushed;

    ChipState state{};

    if (disabled) {
        state = (m_kind == Kind::Filter)
            ? palette.filter_disabled
            : palette.assist_disabled;
    } else if (m_kind == Kind::Filter && selected) {
        state = pressed ? palette.filter_selected_pressed
                        : (hovered ? palette.filter_selected_hover
                                   : palette.filter_selected);
    } else if (pressed) {
        state = (m_kind == Kind::Filter)
            ? palette.filter_pressed
            : palette.assist_pressed;
    } else if (hovered) {
        state = (m_kind == Kind::Filter)
            ? palette.filter_hover
            : palette.assist_hover;
    } else {
        state = (m_kind == Kind::Filter)
            ? palette.filter_normal
            : palette.assist_normal;
    }

    const float radius = fluent->corner_radius(
        FluentWebTheme::RadiusToken::Circular);

    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());

    // Draw chip background first
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, radius);
    nvgFillColor(ctx, to_nvg(state.background));
    nvgFill(ctx);

    if (state.border.w() > 0.f) {
        nvgStrokeWidth(ctx, 1.f);
        nvgStrokeColor(ctx, to_nvg(state.border));
        nvgStroke(ctx);
    }

    // Draw focus ring on top
    if (m_focused && !disabled) {
        nvgSave(ctx);
        nvgResetScissor(ctx);  // Ensure focus ring isn't clipped
        
        // Outer focus ring
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx,
                       x - 2.5f,
                       y - 2.5f,
                       w + 5.f,
                       h + 5.f,
                       radius + 2.5f);
        nvgStrokeWidth(ctx, 2.5f);
        nvgStrokeColor(ctx, to_nvg(palette.focus_outer));
        nvgStroke(ctx);

        // Inner focus ring
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx,
                       x - 0.75f,
                       y - 0.75f,
                       w + 1.5f,
                       h + 1.5f,
                       radius + 0.75f);
        nvgStrokeWidth(ctx, 1.5f);
        nvgStrokeColor(ctx, to_nvg(palette.focus_inner));
        nvgStroke(ctx);
        
        nvgRestore(ctx);
    }

    // Icon rendering
    float font_size = static_cast<float>(m_font_size == -1 ? m_theme->m_button_font_size : m_font_size);
    float icon_width = 0.f;
    if (m_icon) {
        if (nvg_is_font_icon(m_icon)) {
            float ih = font_size;
            ih *= icon_scale();
            nvgFontSize(ctx, ih);
            nvgFontFace(ctx, "icons");
            icon_width = nvgTextBounds(ctx, 0.f, 0.f, utf8(m_icon).data(), nullptr, nullptr);
        } else {
            int iw, ih;
            nvgImageSize(ctx, m_icon, &iw, &ih);
            icon_width = iw * font_size / ih;
        }
        icon_width += font_size * 0.2f;
    }

    nvgFontSize(ctx, static_cast<float>(m_font_size == -1 ? m_theme->m_button_font_size : m_font_size));
    nvgFontFace(ctx, "sans");
    float tw = 0.f;
    if (!m_caption.empty()) {
        tw = nvgTextBounds(ctx, 0.f, 0.f, m_caption.c_str(), nullptr, nullptr);
    }

    float content_width = tw + icon_width;
    Vector2f center = Vector2f(m_pos) + Vector2f(m_size) * 0.5f;
    float label_x = center.x() - content_width * 0.5f;
    float label_y = center.y();

    if (m_icon) {
        float icon_x = label_x;
        float icon_y = label_y;
        nvgFillColor(ctx, to_nvg(state.icon));
        if (nvg_is_font_icon(m_icon)) {
            float ih = font_size;
            ih *= icon_scale();
            nvgFontSize(ctx, ih);
            nvgFontFace(ctx, "icons");
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(ctx, icon_x, icon_y + 1.f, utf8(m_icon).data(), nullptr);
        } else {
            float ih = font_size;
            int orig_w, orig_h;
            nvgImageSize(ctx, m_icon, &orig_w, &orig_h);
            float draw_w = ih * orig_w / orig_h;
            NVGpaint img = nvgImagePattern(ctx,
                                           icon_x,
                                           icon_y - ih * 0.5f,
                                           draw_w,
                                           ih,
                                           0.f,
                                           m_icon,
                                           disabled ? 0.35f : 0.9f);
            nvgBeginPath(ctx);
            nvgRect(ctx, icon_x, icon_y - ih * 0.5f, draw_w, ih);
            nvgFillPaint(ctx, img);
            nvgFill(ctx);
        }
        label_x += icon_width;
    }

    if (!m_caption.empty()) {
        nvgFontFace(ctx, "sans");
        nvgFontSize(ctx, static_cast<float>(m_font_size == -1 ? m_theme->m_button_font_size : m_font_size));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, to_nvg(state.text));
        nvgText(ctx, label_x, label_y + 1.f, m_caption.c_str(), nullptr);
    }
}

NAMESPACE_END(nanogui)
