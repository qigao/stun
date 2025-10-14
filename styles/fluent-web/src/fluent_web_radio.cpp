#include <nanogui/fluent_web_radio.h>

#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#include <algorithm>

NAMESPACE_BEGIN(nanogui)

namespace {

struct RadioVisual {
    Color outer_fill;
    Color outer_border;
    Color inner_fill;
};

struct RadioPalette {
    RadioVisual off_normal;
    RadioVisual off_hover;
    RadioVisual off_pressed;
    RadioVisual on_normal;
    RadioVisual on_hover;
    RadioVisual on_pressed;
    RadioVisual disabled_off;
    RadioVisual disabled_on;
    Color focus_inner;
    Color focus_outer;
    Color label_enabled;
    Color label_disabled;
};

RadioPalette make_palette(const FluentWebTheme &theme) {
    auto c = [&](FluentWebTheme::ColorToken token) {
        return theme.color(token);
    };

    RadioPalette palette{};
    palette.off_normal = {
        c(FluentWebTheme::ColorToken::colorSubtleBackground),
        c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessible),
        Color(0.f, 0.f, 0.f, 0.f)
    };
    palette.off_hover = {
        c(FluentWebTheme::ColorToken::colorSubtleBackgroundHover),
        c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessibleHover),
        Color(0.f, 0.f, 0.f, 0.f)
    };
    palette.off_pressed = {
        c(FluentWebTheme::ColorToken::colorSubtleBackgroundPressed),
        c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessiblePressed),
        Color(0.f, 0.f, 0.f, 0.f)
    };

    palette.on_normal = {
        c(FluentWebTheme::ColorToken::colorBrandBackground),
        c(FluentWebTheme::ColorToken::colorBrandStroke1),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand)
    };
    palette.on_hover = {
        c(FluentWebTheme::ColorToken::colorBrandBackgroundHover),
        c(FluentWebTheme::ColorToken::colorBrandStroke1),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand)
    };
    palette.on_pressed = {
        c(FluentWebTheme::ColorToken::colorBrandBackgroundPressed),
        c(FluentWebTheme::ColorToken::colorBrandStroke1),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand)
    };

    Color disabled_fill =
        c(FluentWebTheme::ColorToken::colorNeutralBackgroundDisabled);
    Color disabled_border =
        c(FluentWebTheme::ColorToken::colorNeutralStrokeDisabled);
    Color disabled_inner =
        c(FluentWebTheme::ColorToken::colorNeutralForegroundDisabled);

    palette.disabled_off = {disabled_fill, disabled_border,
                            Color(0.f, 0.f, 0.f, 0.f)};
    palette.disabled_on = {disabled_fill, disabled_border, disabled_inner};

    palette.focus_inner =
        c(FluentWebTheme::ColorToken::colorStrokeFocus2);
    palette.focus_outer =
        c(FluentWebTheme::ColorToken::colorStrokeFocus1);
    palette.label_enabled =
        c(FluentWebTheme::ColorToken::colorNeutralForeground1);
    palette.label_disabled =
        c(FluentWebTheme::ColorToken::colorNeutralForegroundDisabled);

    return palette;
}

NVGcolor to_nvg(const Color &c) {
    return nvgRGBAf(c.r(), c.g(), c.b(), c.w());
}

} // namespace

FluentWebRadio::FluentWebRadio(Widget *parent,
                               const std::string &caption,
                               bool selected)
    : Button(parent, caption) {
    set_flags(Flags::RadioButton);
    set_pushed(selected);
    update_metrics();
}

void FluentWebRadio::set_theme(Theme *theme) {
    Button::set_theme(theme);
    update_metrics();
    preferred_size_changed();
}

void FluentWebRadio::update_metrics() {
    const auto *fluent =
        dynamic_cast<const FluentWebTheme *>(this->theme());
    if (!fluent)
        return;

    float base = fluent->spacing(FluentWebTheme::SpaceToken::M);
    m_outer_radius = base * 0.75f;
    m_inner_radius = base * 0.45f;

    const auto &body = fluent->typography(
        FluentWebTheme::TypographyToken::Body2);
    m_font_override = static_cast<int>(std::round(body.font_size));
    m_font_size = m_font_override;
}

Vector2i FluentWebRadio::preferred_size_impl(NVGcontext *ctx) const {
    const auto *fluent =
        dynamic_cast<const FluentWebTheme *>(this->theme());
    if (!fluent) {
        return Button::preferred_size_impl(ctx);
    }

    float label_width = 0.f;
    if (!m_caption.empty()) {
        nvgFontFace(ctx, "sans");
        nvgFontSize(ctx, static_cast<float>(m_font_override));
        label_width = nvgTextBounds(ctx, 0.f, 0.f, m_caption.c_str(), nullptr, nullptr);
    }

    float spacing = m_caption.empty()
        ? 0.f
        : fluent->spacing(FluentWebTheme::SpaceToken::S);

    float diameter = m_outer_radius * 2.f;
    int width = static_cast<int>(std::round(diameter + spacing + label_width));
    float height = std::max(diameter,
                            static_cast<float>(m_font_override));
    height += fluent->spacing(FluentWebTheme::SpaceToken::XS);
    return Vector2i(width, static_cast<int>(std::round(height)));
}

void FluentWebRadio::draw(NVGcontext *ctx) {
    auto *fluent = dynamic_cast<FluentWebTheme *>(this->theme());
    if (!fluent) {
        Button::draw(ctx);
        return;
    }

    Widget::draw(ctx);

    const RadioPalette palette = make_palette(*fluent);

    const bool disabled = !m_enabled;
    const bool hovered = m_mouse_focus && !disabled;
    const bool pressed = m_pushed && !disabled && (m_flags & ToggleButton) == 0;
    const bool selected = m_pushed;

    const RadioVisual *state = nullptr;
    if (disabled) {
        state = selected ? &palette.disabled_on : &palette.disabled_off;
    } else if (selected) {
        state = pressed ? &palette.on_pressed
                        : (hovered ? &palette.on_hover
                                   : &palette.on_normal);
    } else {
        state = pressed ? &palette.off_pressed
                        : (hovered ? &palette.off_hover
                                   : &palette.off_normal);
    }

    const float diameter = m_outer_radius * 2.f;
    const float circle_x = static_cast<float>(m_pos.x());
    const float circle_y =
        static_cast<float>(m_pos.y()) + (m_size.y() - diameter) * 0.5f;
    const float center_x = circle_x + m_outer_radius;
    const float center_y = circle_y + m_outer_radius;

    if (m_focused && !disabled) {
        nvgSave(ctx);
        nvgBeginPath(ctx);
        nvgCircle(ctx, center_x, center_y, m_outer_radius + 3.f);
        nvgStrokeWidth(ctx, 2.f);
        nvgStrokeColor(ctx, to_nvg(palette.focus_outer));
        nvgStroke(ctx);

        nvgBeginPath(ctx);
        nvgCircle(ctx, center_x, center_y, m_outer_radius + 1.5f);
        nvgStrokeWidth(ctx, 1.4f);
        nvgStrokeColor(ctx, to_nvg(palette.focus_inner));
        nvgStroke(ctx);
        nvgRestore(ctx);
    }

    nvgBeginPath(ctx);
    nvgCircle(ctx, center_x, center_y, m_outer_radius);
    nvgFillColor(ctx, to_nvg(state->outer_fill));
    nvgFill(ctx);

    if (state->outer_border.w() > 0.f) {
        nvgStrokeWidth(ctx, 1.0f);
        nvgStrokeColor(ctx, to_nvg(state->outer_border));
        nvgStroke(ctx);
    }

    if (state->inner_fill.w() > 0.f) {
        nvgBeginPath(ctx);
        nvgCircle(ctx, center_x, center_y, m_inner_radius);
        nvgFillColor(ctx, to_nvg(state->inner_fill));
        nvgFill(ctx);
    }

    if (!m_caption.empty()) {
        nvgFontFace(ctx, "sans");
        nvgFontSize(ctx, static_cast<float>(m_font_override));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, to_nvg(disabled ? palette.label_disabled
                                          : palette.label_enabled));
        float label_x = circle_x + diameter +
            fluent->spacing(FluentWebTheme::SpaceToken::S);
        float label_y = static_cast<float>(m_pos.y()) + m_size.y() * 0.5f;
        nvgText(ctx, label_x, label_y, m_caption.c_str(), nullptr);
    }
}

NAMESPACE_END(nanogui)

