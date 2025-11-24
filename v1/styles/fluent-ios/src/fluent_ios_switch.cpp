#include <algorithm>
#include <cmath>

#include <nanogui/fluent_ios_switch.h>
#include <nanogui/fluent_ios_theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

namespace {

constexpr float kFallbackTrackWidth = 52.f;
constexpr float kFallbackTrackHeight = 32.f;

struct SwitchMetrics {
    float track_width;
    float track_height;
    float knob_size;
    float knob_min;
    float knob_max;
    float left;
    float top;
};

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline Color mix(const Color &a, const Color &b, float t) {
    return Color(
        a.r() + (b.r() - a.r()) * t,
        a.g() + (b.g() - a.g()) * t,
        a.b() + (b.b() - a.b()) * t,
        a.w() + (b.w() - a.w()) * t
    );
}

SwitchMetrics compute_metrics(const FluentIOSSwitch &control,
                              const FluentIOSTheme *theme) {
    Vector2i size = control.size();

    float track_height = size.y() > 0
        ? static_cast<float>(size.y())
        : (theme ? theme->spacing(FluentIOSTheme::SpacingToken::XLarge)
                 : kFallbackTrackHeight);

    float track_width = size.x() > 0
        ? static_cast<float>(size.x())
        : (theme ? theme->spacing(FluentIOSTheme::SpacingToken::XLarge) * 2.f
                 : kFallbackTrackWidth);

    float min_width = track_height * 1.65f;
    if (track_width < min_width)
        track_width = min_width;

    float padding = theme
        ? theme->spacing(FluentIOSTheme::SpacingToken::Small) * 0.4f
        : track_height * 0.18f;
    padding = std::min(padding, track_height * 0.35f);

    float knob_size = std::max(track_height - padding * 2.f, track_height * 0.55f);
    float knob_min = padding;
    float knob_max = std::max(track_width - knob_size - padding, knob_min);

    float container_width = static_cast<float>(size.x());
    float container_height = static_cast<float>(size.y());
    float left = static_cast<float>(control.position().x());
    float top = static_cast<float>(control.position().y());

    if (container_width > track_width)
        left += (container_width - track_width) * 0.5f;
    if (container_height > track_height)
        top += (container_height - track_height) * 0.5f;

    return {track_width, track_height, knob_size, knob_min, knob_max, left, top};
}

} // namespace

FluentIOSSwitch::FluentIOSSwitch(Widget *parent, bool state)
    : Widget(parent), m_state(state) {
    m_drag_fraction = m_state ? 1.f : 0.f;
}

void FluentIOSSwitch::set_state(bool state, bool emit) {
    if (m_state != state) {
        m_state = state;
        if (emit && m_callback)
            m_callback(m_state);
    }
    m_drag_fraction = m_state ? 1.f : 0.f;
}

Vector2i FluentIOSSwitch::preferred_size_impl(NVGcontext *) const {
    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());
    float height = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::XLarge)
                             : kFallbackTrackHeight;
    float width = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::XLarge) * 2.f
                            : kFallbackTrackWidth;
    width = std::max(width, height * 1.65f);
    return Vector2i(static_cast<int>(std::ceil(width)), static_cast<int>(std::ceil(height)));
}

void FluentIOSSwitch::draw(NVGcontext *ctx) {
    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());
    SwitchMetrics metrics = compute_metrics(*this, ios_theme);

    float track_left = metrics.left;
    float track_top = metrics.top;
    float track_width = metrics.track_width;
    float track_height = metrics.track_height;

    float knob_min = track_left + metrics.knob_min;
    float knob_max = track_left + metrics.knob_max;
    float knob_range = std::max(knob_max - knob_min, 1.f);

    float fraction = m_dragging ? m_drag_fraction : (m_state ? 1.f : 0.f);
    fraction = std::clamp(fraction, 0.f, 1.f);

    Color off_track = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::Surface)
        : Color(0.85f, 0.87f, 0.89f, 1.f);
    Color on_track = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::AccentPrimary)
        : Color(0.f, 0.478f, 1.f, 1.f);
    Color disabled_off = ios_theme
        ? mix(off_track, ios_theme->color(FluentIOSTheme::SemanticColor::TextMuted), 0.6f)
        : Color(0.82f, 0.84f, 0.86f, 0.42f);
    Color disabled_on = ios_theme
        ? mix(on_track, ios_theme->color(FluentIOSTheme::SemanticColor::TextMuted), 0.5f)
        : Color(0.48f, 0.62f, 0.90f, 0.38f);

    Color track_color = mix(off_track, on_track, fraction);
    if (!m_enabled) {
        track_color = mix(disabled_off, disabled_on, fraction);
    }

    Color knob_color = Color(1.f, 1.f, 1.f, m_enabled ? 1.f : 0.7f);

    float knob_x = knob_min + knob_range * fraction;
    float knob_y = track_top + (track_height - metrics.knob_size) * 0.5f;
    float knob_radius = metrics.knob_size * 0.5f;
    float knob_center_x = knob_x + knob_radius;
    float knob_center_y = knob_y + knob_radius;

    nvgSave(ctx);

    if (focused()) {
        Color focus_color = ios_theme
            ? ios_theme->color(FluentIOSTheme::SemanticColor::AccentSecondary)
            : Color(0.f, 0.478f, 1.f, 0.6f);
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx,
                       track_left - 2.f,
                       track_top - 2.f,
                       track_width + 4.f,
                       track_height + 4.f,
                       (track_height + 4.f) * 0.5f);
        nvgStrokeColor(ctx, nvgRGBAf(focus_color.r(), focus_color.g(), focus_color.b(), 0.65f));
        nvgStrokeWidth(ctx, 1.5f);
        nvgStroke(ctx);
    }

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, track_left, track_top, track_width, track_height, track_height * 0.5f);
    nvgFillColor(ctx, nvgRGBAf(track_color.r(), track_color.g(), track_color.b(), track_color.w()));
    nvgFill(ctx);

    auto draw_shadow_layer = [&](const FluentIOSTheme::ShadowComponent &layer) {
        if (layer.opacity <= 0.f || layer.blur_radius <= 0.f)
            return;
        float inner_radius = knob_radius;
        float outer_radius = knob_radius + layer.blur_radius;
        if (outer_radius <= inner_radius + 0.5f)
            outer_radius = inner_radius + 0.5f;
        Color layer_color(0.f, 0.f, 0.f, layer.opacity);
        NVGpaint paint = nvgRadialGradient(
            ctx,
            knob_center_x + layer.x_offset,
            knob_center_y + layer.y_offset,
            inner_radius,
            outer_radius,
            nvgRGBAf(layer_color.r(), layer_color.g(), layer_color.b(), layer_color.w()),
            nvgRGBAf(layer_color.r(), layer_color.g(), layer_color.b(), 0.f));
        float extent = outer_radius * 2.f;
        nvgBeginPath(ctx);
        nvgRect(ctx,
                knob_center_x + layer.x_offset - outer_radius,
                knob_center_y + layer.y_offset - outer_radius,
                extent,
                extent);
        nvgFillPaint(ctx, paint);
        nvgFill(ctx);
    };

    if (ios_theme) {
        const auto &shadow = ios_theme->elevation(FluentIOSTheme::ElevationLevel::Level2);
        draw_shadow_layer(shadow.ambient);
        draw_shadow_layer(shadow.key);
    } else {
        FluentIOSTheme::ShadowComponent fallback{0.f, 0.5f, knob_radius * 0.45f, 0.20f};
        draw_shadow_layer(fallback);
    }

    nvgBeginPath(ctx);
    nvgCircle(ctx, knob_center_x, knob_center_y, knob_radius);
    if (m_pressed && m_enabled) {
        knob_color = mix(knob_color, Color(0.92f, 0.92f, 0.92f, knob_color.w()), 0.35f);
    }
    nvgFillColor(ctx, nvgRGBAf(knob_color.r(), knob_color.g(), knob_color.b(), knob_color.w()));
    nvgFill(ctx);

    nvgRestore(ctx);

    Widget::draw(ctx);
}

bool FluentIOSSwitch::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    if (button != GLFW_MOUSE_BUTTON_1)
        return Widget::mouse_button_event(p, button, down, modifiers);

    if (!m_enabled)
        return false;

    if (down) {
        Vector2i local = p - m_pos;
        if (local.x() < 0 || local.x() > m_size.x() ||
            local.y() < 0 || local.y() > m_size.y())
            return false;

        request_focus();
        m_pressed = true;
        m_dragging = false;
        m_drag_fraction = m_state ? 1.f : 0.f;
        return true;
    }

    if (!m_pressed)
        return Widget::mouse_button_event(p, button, down, modifiers);

    bool inside = p.x() >= m_pos.x() && p.x() <= m_pos.x() + m_size.x() &&
                  p.y() >= m_pos.y() && p.y() <= m_pos.y() + m_size.y();

    bool new_state;
    if (m_dragging)
        new_state = m_drag_fraction >= 0.5f;
    else
        new_state = inside ? !m_state : m_state;

    m_pressed = false;
    m_dragging = false;
    set_state(new_state);
    return true;
}

bool FluentIOSSwitch::mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) {
    if (!m_enabled || !m_pressed || button != GLFW_MOUSE_BUTTON_1)
        return Widget::mouse_drag_event(p, rel, button, modifiers);

    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());
    SwitchMetrics metrics = compute_metrics(*this, ios_theme);
    float knob_range = std::max(metrics.knob_max - metrics.knob_min, 1.f);

    m_dragging = true;
    m_drag_fraction = std::clamp(m_drag_fraction + static_cast<float>(rel.x()) / knob_range, 0.f, 1.f);
    return true;
}

bool FluentIOSSwitch::keyboard_event(int key, int scancode, int action, int modifiers) {
    if (!m_enabled)
        return Widget::keyboard_event(key, scancode, action, modifiers);

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_SPACE || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            set_state(!m_state);
            return true;
        }
    }

    return Widget::keyboard_event(key, scancode, action, modifiers);
}

NAMESPACE_END(nanogui)
