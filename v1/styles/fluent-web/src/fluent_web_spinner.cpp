#include <nanogui/fluent_web_spinner.h>

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

FluentWebSpinner::FluentWebSpinner(Widget *parent, Size size)
    : Widget(parent),
      m_size_category(size),
      m_radius(16.f),
      m_stroke_width(3.f) {
    m_start_time = std::chrono::steady_clock::now();
    update_dimensions();
}

void FluentWebSpinner::set_theme(Theme *theme) {
    Widget::set_theme(theme);
    update_dimensions();
}

void FluentWebSpinner::set_size(Size size) {
    if (m_size_category == size)
        return;
    m_size_category = size;
    update_dimensions();
}

void FluentWebSpinner::update_dimensions() {
    int widget_size = 32;
    switch (m_size_category) {
        case Size::XSmall:
            widget_size = 20;
            m_stroke_width = 2.2f;
            break;
        case Size::Small:
            widget_size = 28;
            m_stroke_width = 2.6f;
            break;
        case Size::Medium:
            widget_size = 36;
            m_stroke_width = 3.0f;
            break;
        case Size::Large:
            widget_size = 44;
            m_stroke_width = 3.4f;
            break;
    }
    set_fixed_size(Vector2i(widget_size, widget_size));
    m_radius = widget_size * 0.45f;
}

Vector2i FluentWebSpinner::preferred_size_impl(NVGcontext *ctx) const {
    return Vector2i(fixed_size().x(), fixed_size().y());
}

void FluentWebSpinner::draw(NVGcontext *ctx) {
    Widget::draw(ctx);

    auto *fluent = dynamic_cast<FluentWebTheme *>(this->theme());
    if (!fluent) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - m_start_time).count();

    float cx = static_cast<float>(m_pos.x()) + width() * 0.5f;
    float cy = static_cast<float>(m_pos.y()) + height() * 0.5f;

    Color track = fluent->color(FluentWebTheme::ColorToken::colorNeutralStrokeAlpha);
    Color accent = fluent->color(FluentWebTheme::ColorToken::colorBrandForegroundLink);

    nvgSave(ctx);

    // Track circle
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, m_radius);
    nvgStrokeWidth(ctx, m_stroke_width);
    nvgStrokeColor(ctx, to_nvg(track));
    nvgStroke(ctx);

    // Animated arc
    float rotation = (elapsed * 1.4f) * 2.f * NVG_PI;
    float arc_phase = std::sin(elapsed * 3.f);
    float arc_length = 0.25f + 0.45f * (0.5f + 0.5f * arc_phase);
    float start_angle = rotation;
    float end_angle = rotation + arc_length * 2.f * NVG_PI;

    nvgBeginPath(ctx);
    nvgArc(ctx, cx, cy, m_radius, start_angle, end_angle, NVG_CW);
    nvgStrokeWidth(ctx, m_stroke_width);
    nvgLineCap(ctx, NVG_ROUND);
    nvgStrokeColor(ctx, to_nvg(accent));
    nvgStroke(ctx);

    nvgRestore(ctx);

    if (auto *scr = screen())
        scr->redraw();
}

NAMESPACE_END(nanogui)

