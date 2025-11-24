#include <nanogui/fluent_web_switch.h>

#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#include <algorithm>

NAMESPACE_BEGIN(nanogui)

namespace {

struct TrackThumbState {
    Color track;
    Color border;
    Color thumb;
    Color thumb_border;
};

struct SwitchPalette {
    TrackThumbState off_normal;
    TrackThumbState off_hover;
    TrackThumbState off_pressed;
    TrackThumbState on_normal;
    TrackThumbState on_hover;
    TrackThumbState on_pressed;
    TrackThumbState disabled_off;
    TrackThumbState disabled_on;
    Color focus_inner;
    Color focus_outer;
    Color label_enabled;
    Color label_disabled;
};

SwitchPalette make_palette(const FluentWebTheme &theme) {
    auto c = [&](FluentWebTheme::ColorToken token) {
        return theme.color(token);
    };

    SwitchPalette palette{};
    palette.off_normal = {
        c(FluentWebTheme::ColorToken::colorSubtleBackground),
        c(FluentWebTheme::ColorToken::colorNeutralStroke2),
        c(FluentWebTheme::ColorToken::colorNeutralForeground3),
        c(FluentWebTheme::ColorToken::colorNeutralStroke2)
    };
    palette.off_hover = {
        c(FluentWebTheme::ColorToken::colorSubtleBackgroundHover),
        c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessibleHover),
        c(FluentWebTheme::ColorToken::colorNeutralForeground2),
        c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessibleHover)
    };
    palette.off_pressed = {
        c(FluentWebTheme::ColorToken::colorSubtleBackgroundPressed),
        c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessiblePressed),
        c(FluentWebTheme::ColorToken::colorNeutralForeground1),
        c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessiblePressed)
    };

    palette.on_normal = {
        c(FluentWebTheme::ColorToken::colorBrandBackground),
        c(FluentWebTheme::ColorToken::colorBrandStroke1),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand),
        c(FluentWebTheme::ColorToken::colorBrandStroke1)
    };
    palette.on_hover = {
        c(FluentWebTheme::ColorToken::colorBrandBackgroundHover),
        c(FluentWebTheme::ColorToken::colorBrandStroke1),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand),
        c(FluentWebTheme::ColorToken::colorBrandStroke1)
    };
    palette.on_pressed = {
        c(FluentWebTheme::ColorToken::colorBrandBackgroundPressed),
        c(FluentWebTheme::ColorToken::colorBrandStroke1),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand),
        c(FluentWebTheme::ColorToken::colorBrandStroke1)
    };

    palette.disabled_off = {
        c(FluentWebTheme::ColorToken::colorNeutralBackgroundDisabled),
        c(FluentWebTheme::ColorToken::colorNeutralStrokeDisabled),
        c(FluentWebTheme::ColorToken::colorNeutralForegroundDisabled),
        c(FluentWebTheme::ColorToken::colorNeutralStrokeDisabled)
    };
    palette.disabled_on = palette.disabled_off;

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

FluentWebSwitch::FluentWebSwitch(Widget *parent,
                                 const std::string &caption,
                                 const std::function<void(bool)> &callback)
    : CheckBox(parent, caption, callback) {
    update_metrics();
}

void FluentWebSwitch::set_theme(Theme *theme) {
    CheckBox::set_theme(theme);
    update_metrics();
    preferred_size_changed();
}

void FluentWebSwitch::update_metrics() {
    const auto *fluent =
        dynamic_cast<const FluentWebTheme *>(this->theme());
    if (!fluent)
        return;

    m_track_width = fluent->spacing(FluentWebTheme::SpaceToken::XXL);
    m_track_height = fluent->spacing(FluentWebTheme::SpaceToken::L);
    m_thumb_diameter = fluent->spacing(FluentWebTheme::SpaceToken::M);
    float min_width = std::max(m_track_width, m_thumb_diameter + fluent->spacing(FluentWebTheme::SpaceToken::S));
    m_track_width = std::max(m_track_width, min_width);

    if (m_font_override == -1) {
        const auto &body = fluent->typography(
            FluentWebTheme::TypographyToken::Body2);
        m_font_override = static_cast<int>(std::round(body.font_size));
        m_font_size = m_font_override;
    }
}

Vector2i FluentWebSwitch::preferred_size_impl(NVGcontext *ctx) const {
    const auto *fluent =
        dynamic_cast<const FluentWebTheme *>(this->theme());
    if (!fluent) {
        return CheckBox::preferred_size_impl(ctx);
    }

    float label_width = 0.f;
    if (!m_caption.empty()) {
        nvgFontFace(ctx, "sans");
        nvgFontSize(ctx, static_cast<float>(m_font_override > 0 ? m_font_override : m_font_size));
        label_width = nvgTextBounds(ctx, 0.f, 0.f, m_caption.c_str(), nullptr, nullptr);
    }

    float spacing = m_caption.empty()
        ? 0.f
        : fluent->spacing(FluentWebTheme::SpaceToken::S);
    int width = static_cast<int>(std::round(m_track_width + spacing + label_width));
    int height = static_cast<int>(std::round(
        std::max(m_track_height,
                 static_cast<float>(m_font_override > 0 ? m_font_override : m_font_size))));
    height += static_cast<int>(std::round(fluent->spacing(FluentWebTheme::SpaceToken::XS)));
    return Vector2i(width, height);
}

void FluentWebSwitch::draw(NVGcontext *ctx) {
    auto *fluent = dynamic_cast<FluentWebTheme *>(this->theme());
    if (!fluent) {
        CheckBox::draw(ctx);
        return;
    }

    Widget::draw(ctx);

    const SwitchPalette palette = make_palette(*fluent);
    const bool disabled = !m_enabled;
    const bool hovered = m_mouse_focus && !disabled;
    const bool pressed = m_pushed && !disabled;
    const bool on = m_checked;

    const TrackThumbState *state = nullptr;
    if (disabled) {
        state = on ? &palette.disabled_on : &palette.disabled_off;
    } else if (on) {
        state = pressed ? &palette.on_pressed
                        : (hovered ? &palette.on_hover
                                   : &palette.on_normal);
    } else {
        state = pressed ? &palette.off_pressed
                        : (hovered ? &palette.off_hover
                                   : &palette.off_normal);
    }

    const float track_x = static_cast<float>(m_pos.x());
    const float track_y =
        static_cast<float>(m_pos.y()) + (m_size.y() - m_track_height) * 0.5f;

    const float radius = m_track_height * 0.5f;

    if (m_focused && !disabled) {
        nvgSave(ctx);
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx,
                       track_x - 3.f,
                       track_y - 3.f,
                       m_track_width + 6.f,
                       m_track_height + 6.f,
                       radius + 3.f);
        nvgStrokeWidth(ctx, 2.f);
        nvgStrokeColor(ctx, to_nvg(palette.focus_outer));
        nvgStroke(ctx);

        nvgBeginPath(ctx);
        nvgRoundedRect(ctx,
                       track_x - 1.5f,
                       track_y - 1.5f,
                       m_track_width + 3.f,
                       m_track_height + 3.f,
                       radius + 2.f);
        nvgStrokeWidth(ctx, 1.4f);
        nvgStrokeColor(ctx, to_nvg(palette.focus_inner));
        nvgStroke(ctx);
        nvgRestore(ctx);
    }

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, track_x, track_y,
                   m_track_width, m_track_height, radius);
    nvgFillColor(ctx, to_nvg(state->track));
    nvgFill(ctx);

    if (state->border.w() > 0.f) {
        nvgStrokeWidth(ctx, 1.0f);
        nvgStrokeColor(ctx, to_nvg(state->border));
        nvgStroke(ctx);
    }

    const float thumb_min = track_x + radius;
    const float thumb_max = track_x + m_track_width - radius;
    float thumb_center_x = on ? thumb_max : thumb_min;

    nvgBeginPath(ctx);
    nvgCircle(ctx, thumb_center_x,
              track_y + radius, m_thumb_diameter * 0.5f);
    nvgFillColor(ctx, to_nvg(state->thumb));
    nvgFill(ctx);

    if (state->thumb_border.w() > 0.f) {
        nvgStrokeWidth(ctx, 1.0f);
        nvgStrokeColor(ctx, to_nvg(state->thumb_border));
        nvgStroke(ctx);
    }

    if (!m_caption.empty()) {
        nvgFontFace(ctx, "sans");
        nvgFontSize(ctx, static_cast<float>(m_font_override > 0 ? m_font_override : m_font_size));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, to_nvg(disabled ? palette.label_disabled
                                          : palette.label_enabled));
        float label_x = track_x + m_track_width +
            fluent->spacing(FluentWebTheme::SpaceToken::S);
        float label_y = static_cast<float>(m_pos.y()) + m_size.y() * 0.5f;
        nvgText(ctx, label_x, label_y, m_caption.c_str(), nullptr);
    }
}

NAMESPACE_END(nanogui)

