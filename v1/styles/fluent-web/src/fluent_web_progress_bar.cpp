#include <nanogui/fluent_web_progress_bar.h>

#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#include <algorithm>

NAMESPACE_BEGIN(nanogui)

namespace {

NVGcolor to_nvg(const Color &c, float override_alpha = -1.f) {
    float alpha = override_alpha >= 0.f ? override_alpha : c.w();
    return nvgRGBAf(c.r(), c.g(), c.b(), alpha);
}

} // namespace

FluentWebProgressBar::FluentWebProgressBar(Widget *parent)
    : ProgressBar(parent), m_indeterminate(false), m_track_height(6.f) {
    m_start_time = std::chrono::steady_clock::now();
}

void FluentWebProgressBar::set_indeterminate(bool indeterminate) {
    if (m_indeterminate == indeterminate)
        return;
    m_indeterminate = indeterminate;
    ensure_animation_started();
}

void FluentWebProgressBar::set_theme(Theme *theme) {
    ProgressBar::set_theme(theme);
    auto *fluent = dynamic_cast<FluentWebTheme *>(theme);
    if (fluent) {
        m_track_height = fluent->spacing(FluentWebTheme::SpaceToken::S);
    }
}

Vector2i FluentWebProgressBar::preferred_size_impl(NVGcontext *ctx) const {
    int height = static_cast<int>(std::round(std::max(m_track_height, 6.f)));
    return Vector2i(240, height);
}

void FluentWebProgressBar::ensure_animation_started() {
    if (m_indeterminate) {
        m_start_time = std::chrono::steady_clock::now();
        if (auto *scr = screen())
            scr->redraw();
    }
}

void FluentWebProgressBar::draw(NVGcontext *ctx) {
    auto *fluent = dynamic_cast<FluentWebTheme *>(this->theme());
    if (!fluent) {
        ProgressBar::draw(ctx);
        return;
    }

    Widget::draw(ctx);

    const float x = static_cast<float>(m_pos.x());
    const float y = static_cast<float>(m_pos.y());
    const float w = static_cast<float>(m_size.x());
    const float h = static_cast<float>(m_size.y());

    const float radius = h * 0.5f;

    Color track = fluent->color(FluentWebTheme::ColorToken::colorNeutralBackground2);
    Color track_border = fluent->color(FluentWebTheme::ColorToken::colorNeutralStroke2);
    Color fill = fluent->color(FluentWebTheme::ColorToken::colorBrandBackground);
    Color shimmer = fluent->color(FluentWebTheme::ColorToken::colorBrandForeground1);

    // Track
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, radius);
    nvgFillColor(ctx, to_nvg(track));
    nvgFill(ctx);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f, std::max(radius - 0.5f, 0.f));
    nvgStrokeWidth(ctx, 1.f);
    nvgStrokeColor(ctx, to_nvg(track_border));
    nvgStroke(ctx);

    if (!m_indeterminate) {
        float progress = std::clamp(m_value, 0.f, 1.f);
        float fill_width = std::max(0.f, (w - 2.f) * progress);
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x + 1.f, y + 1.f, fill_width, h - 2.f, std::max(radius - 1.f, 0.f));
        nvgFillColor(ctx, to_nvg(fill));
        nvgFill(ctx);
    } else {
        ensure_animation_started();
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - m_start_time).count();
        float cycle = std::fmod(elapsed, 2.2f);

        // Two animated segments like Fluent progress indicators.
        float segment1_t = std::fmod(cycle, 1.1f) / 1.1f;
        float segment2_t = std::fmod(std::max(0.f, cycle - 1.1f), 1.1f) / 1.1f;

        auto draw_segment = [&](float t, bool second) {
            float width = w * 0.3f;
            float start = (second ? 0.5f : 0.f) * w;
            float pos = start + (w - width) * t;
            nvgBeginPath(ctx);
            nvgRoundedRect(ctx, pos, y + 1.f, width, h - 2.f, std::max(radius - 1.f, 0.f));
            nvgFillColor(ctx, to_nvg(fill, 0.9f));
            nvgFill(ctx);
            NVGpaint glow = nvgLinearGradient(ctx,
                                              pos,
                                              y,
                                              pos + width,
                                              y + h,
                                              to_nvg(fill, 0.3f),
                                              to_nvg(shimmer, 0.0f));
            nvgBeginPath(ctx);
            nvgRoundedRect(ctx, pos, y + 1.f, width, h - 2.f, std::max(radius - 1.f, 0.f));
            nvgFillPaint(ctx, glow);
            nvgFill(ctx);
        };

        draw_segment(segment1_t, false);
        if (cycle > 1.1f)
            draw_segment(segment2_t, true);

        if (auto *scr = screen())
            scr->redraw();
    }
}

NAMESPACE_END(nanogui)
