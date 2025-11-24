#include <nanogui/fluent_web_slider.h>

#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#include <algorithm>

NAMESPACE_BEGIN(nanogui)

namespace {

NVGcolor to_nvg(const Color &c, float alpha_override = -1.f) {
    float alpha = alpha_override >= 0.f ? alpha_override : c.w();
    return nvgRGBAf(c.r(), c.g(), c.b(), alpha);
}

} // namespace

FluentWebSlider::FluentWebSlider(Widget *parent)
    : Slider(parent), m_track_height(4.f), m_thumb_radius(8.f) {}

void FluentWebSlider::set_theme(Theme *theme) {
    Slider::set_theme(theme);
    auto *fluent = dynamic_cast<FluentWebTheme *>(theme);
    if (fluent) {
        m_track_height = fluent->spacing(FluentWebTheme::SpaceToken::XS);
        m_thumb_radius = fluent->spacing(FluentWebTheme::SpaceToken::S) * 0.75f;
    }
}

void FluentWebSlider::draw(NVGcontext *ctx) {
    auto *fluent = dynamic_cast<FluentWebTheme *>(this->theme());
    if (!fluent) {
        Slider::draw(ctx);
        return;
    }

    Widget::draw(ctx);

    float slider_value = value();
    slider_value = std::clamp(slider_value, 0.f, 1.f);

    const float x = static_cast<float>(m_pos.x());
    const float y = static_cast<float>(m_pos.y());
    const float w = static_cast<float>(m_size.x());
    const float h = static_cast<float>(m_size.y());

    const float cy = y + h * 0.5f;
    const float track_radius = m_track_height * 0.5f;
    const float thumb_center_x = x + slider_value * (w - 2.f * m_thumb_radius) + m_thumb_radius;

    Color track_bg = fluent->color(FluentWebTheme::ColorToken::colorNeutralBackground2);
    Color track_fill = fluent->color(FluentWebTheme::ColorToken::colorBrandBackground);
    Color track_border = fluent->color(FluentWebTheme::ColorToken::colorNeutralStroke2);
    Color thumb_fill = fluent->color(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand);
    Color thumb_border = fluent->color(FluentWebTheme::ColorToken::colorBrandStroke1);

    if (!m_enabled) {
        track_bg = fluent->color(FluentWebTheme::ColorToken::colorNeutralBackgroundDisabled);
        track_fill = fluent->color(FluentWebTheme::ColorToken::colorNeutralStrokeDisabled);
        thumb_fill = fluent->color(FluentWebTheme::ColorToken::colorNeutralForegroundDisabled);
        thumb_border = fluent->color(FluentWebTheme::ColorToken::colorNeutralStrokeDisabled);
    } else if (m_mouse_focus) {
        track_fill = fluent->color(FluentWebTheme::ColorToken::colorBrandBackgroundHover);
    }

    // Track background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, cy - track_radius, w, m_track_height, track_radius);
    nvgFillColor(ctx, to_nvg(track_bg));
    nvgFill(ctx);

    // Track border
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, cy - track_radius, w, m_track_height, track_radius);
    nvgStrokeWidth(ctx, 1.f);
    nvgStrokeColor(ctx, to_nvg(track_border, 0.6f));
    nvgStroke(ctx);

    float fill_width = thumb_center_x - m_thumb_radius - x;
    fill_width = std::max(fill_width, 0.f);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, cy - track_radius, fill_width, m_track_height, track_radius);
    nvgFillColor(ctx, to_nvg(track_fill));
    nvgFill(ctx);

    // Thumb focus ring if focused
    if (m_focused && m_enabled) {
        Color focus_outer = fluent->color(FluentWebTheme::ColorToken::colorStrokeFocus1);
        Color focus_inner = fluent->color(FluentWebTheme::ColorToken::colorStrokeFocus2);
        nvgBeginPath(ctx);
        nvgCircle(ctx, thumb_center_x, cy, m_thumb_radius + 3.f);
        nvgStrokeWidth(ctx, 2.f);
        nvgStrokeColor(ctx, to_nvg(focus_outer));
        nvgStroke(ctx);

        nvgBeginPath(ctx);
        nvgCircle(ctx, thumb_center_x, cy, m_thumb_radius + 1.5f);
        nvgStrokeWidth(ctx, 1.5f);
        nvgStrokeColor(ctx, to_nvg(focus_inner));
        nvgStroke(ctx);
    }

    // Thumb
    nvgBeginPath(ctx);
    nvgCircle(ctx, thumb_center_x, cy, m_thumb_radius);
    nvgFillColor(ctx, to_nvg(thumb_fill));
    nvgFill(ctx);

    nvgBeginPath(ctx);
    nvgCircle(ctx, thumb_center_x, cy, m_thumb_radius);
    nvgStrokeWidth(ctx, 1.f);
    nvgStrokeColor(ctx, to_nvg(thumb_border));
    nvgStroke(ctx);
}

NAMESPACE_END(nanogui)

