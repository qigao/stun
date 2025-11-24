#include <nanogui/fluent_web_toast.h>

#include <nanogui/fluent_icons.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#include <algorithm>

NAMESPACE_BEGIN(nanogui)

namespace {

NVGcolor to_nvg(const Color &c) {
    return nvgRGBAf(c.r(), c.g(), c.b(), c.w());
}

int severity_icon(FluentWebToast::Severity severity) {
    switch (severity) {
        case FluentWebToast::Severity::Success:
            return FLUENT_ICON_CHECKMARK_CIRCLE;
        case FluentWebToast::Severity::Warning:
            return FLUENT_ICON_WARNING;
        case FluentWebToast::Severity::Error:
            return FLUENT_ICON_DISMISS_CIRCLE;
        case FluentWebToast::Severity::Informational:
        default:
            return FLUENT_ICON_INFO;
    }
}

} // namespace

FluentWebToast::FluentWebToast(Widget *parent,
                               const std::string &message,
                               Severity severity)
    : Widget(parent),
      m_message(message),
      m_action_label(""),
      m_severity(severity),
      m_visible(false),
      m_duration(std::chrono::milliseconds(4000)),
      m_animation_progress(0.f) {
    set_visible(false);
}

void FluentWebToast::set_message(const std::string &message) {
    if (m_message == message)
        return;
    m_message = message;
    preferred_size_changed();
}

void FluentWebToast::set_severity(Severity severity) {
    if (m_severity == severity)
        return;
    m_severity = severity;
}

void FluentWebToast::set_action(const std::string &label,
                                const std::function<void()> &callback) {
    m_action_label = label;
    m_action_callback = callback;
    preferred_size_changed();
}

void FluentWebToast::clear_action() {
    m_action_label.clear();
    m_action_callback = nullptr;
    preferred_size_changed();
}

void FluentWebToast::show() {
    m_visible = true;
    m_animation_progress = 0.f;
    m_show_time = std::chrono::steady_clock::now();
    set_visible(true);
    reposition();
    Screen *scr = dynamic_cast<Screen *>(parent());
    if (!scr)
        scr = Widget::screen();
    if (scr)
        scr->redraw();
}

void FluentWebToast::hide() {
    m_visible = false;
    set_visible(false);
    Screen *scr = dynamic_cast<Screen *>(parent());
    if (!scr)
        scr = Widget::screen();
    if (scr)
        scr->redraw();
}

FluentWebToast::Palette
FluentWebToast::resolve_palette(const FluentWebTheme &theme) const {
    auto c = [&](FluentWebTheme::ColorToken token) -> Color {
        return theme.color(token);
    };

    Palette palette{};
    switch (m_severity) {
        case Severity::Success:
            palette.background = c(FluentWebTheme::ColorToken::colorStatusSuccessBackground3);
            palette.text = c(FluentWebTheme::ColorToken::colorStatusSuccessForeground3);
            palette.icon = palette.text;
            palette.action = palette.text;
            break;
        case Severity::Warning:
            palette.background = c(FluentWebTheme::ColorToken::colorStatusWarningBackground3);
            palette.text = c(FluentWebTheme::ColorToken::colorStatusWarningForeground3);
            palette.icon = palette.text;
            palette.action = palette.text;
            break;
        case Severity::Error:
            palette.background = c(FluentWebTheme::ColorToken::colorStatusDangerBackground3);
            palette.text = c(FluentWebTheme::ColorToken::colorStatusDangerForeground3);
            palette.icon = palette.text;
            palette.action = palette.text;
            break;
        case Severity::Informational:
        default:
            palette.background = c(FluentWebTheme::ColorToken::colorBrandBackground);
            palette.text = c(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand);
            palette.icon = palette.text;
            palette.action = c(FluentWebTheme::ColorToken::colorBrandForegroundLink);
            break;
    }
    return palette;
}

void FluentWebToast::reposition() {
    if (!m_parent)
        return;

    Screen *scr = dynamic_cast<Screen *>(m_parent);
    Widget *container = m_parent;
    if (!scr) {
        scr = screen();
    }
    if (!scr)
        return;

    NVGcontext *ctx = scr->nvg_context();
    Vector2i pref = preferred_size(ctx);
    Vector2i parent_size = container->size();
    int x = (parent_size.x() - pref.x()) / 2;
    int y = parent_size.y() - pref.y() - 24;
    set_position(Vector2i(x, y));
    set_size(pref);
}

Vector2i FluentWebToast::preferred_size_impl(NVGcontext *ctx) const {
    int font_size = m_font_size == -1 ? 16 : m_font_size;
    nvgFontFace(ctx, "sans");
    nvgFontSize(ctx, static_cast<float>(font_size));

    float message_width = nvgTextBounds(ctx, 0.f, 0.f, m_message.c_str(), nullptr, nullptr);

    float action_width = 0.f;
    if (!m_action_label.empty()) {
        nvgFontFace(ctx, "sans-bold");
        action_width = nvgTextBounds(ctx, 0.f, 0.f, m_action_label.c_str(), nullptr, nullptr);
    }

    float padding = 24.f;
    float icon_space = 24.f;
    float total_width = padding + icon_space + message_width + padding;
    if (!m_action_label.empty())
        total_width += action_width + padding;

    total_width = std::min(std::max(total_width, 320.f), 560.f);
    float height = 48.f;

    return Vector2i(static_cast<int>(std::round(total_width)),
                    static_cast<int>(std::round(height)));
}

void FluentWebToast::draw(NVGcontext *ctx) {
    if (!m_visible)
        return;

    auto *fluent = dynamic_cast<FluentWebTheme *>(this->theme());
    if (!fluent) {
        Widget::draw(ctx);
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_show_time);

    if (elapsed > m_duration) {
        hide();
        return;
    }

    const float anim_in_ms = 200.f;
    float t = static_cast<float>(elapsed.count());
    if (t < anim_in_ms)
        m_animation_progress = t / anim_in_ms;
    else
        m_animation_progress = 1.f;

    Widget::draw(ctx);

    const Palette palette = resolve_palette(*fluent);

    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());

    float offset = (1.f - m_animation_progress) * 30.f;
    y += offset;

    nvgSave(ctx);

    NVGcolor shadow_outer = nvgRGBAf(0.f, 0.f, 0.f, 0.30f * m_animation_progress);
    NVGcolor shadow_inner = nvgRGBAf(0.f, 0.f, 0.f, 0.f);
    NVGpaint shadow = nvgBoxGradient(ctx, x, y, w, h, 8.f, 16.f,
                                     shadow_outer, shadow_inner);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x - 4.f, y - 4.f, w + 8.f, h + 8.f, 10.f);
    nvgFillPaint(ctx, shadow);
    nvgFill(ctx);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, fluent->corner_radius(FluentWebTheme::RadiusToken::Large));
    nvgFillColor(ctx, to_nvg(palette.background));
    nvgFill(ctx);

    // Icon
    nvgFontFace(ctx, "fluent-icons");
    nvgFontSize(ctx, 20.f);
    nvgFillColor(ctx, to_nvg(palette.icon));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    float icon_x = x + 20.f;
    float icon_y = y + h * 0.5f;
    nvgText(ctx, icon_x, icon_y, utf8(severity_icon(m_severity)).data(), nullptr);

    // Message
    nvgFontFace(ctx, "sans");
    int font_size = m_font_size == -1 ? 16 : m_font_size;
    nvgFontSize(ctx, static_cast<float>(font_size));
    nvgFillColor(ctx, to_nvg(palette.text));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    float text_x = icon_x + 28.f;
    float text_y = icon_y;
    nvgText(ctx, text_x, text_y, m_message.c_str(), nullptr);

    m_action_min = Vector2f(0.f, 0.f);
    m_action_max = Vector2f(0.f, 0.f);

    if (!m_action_label.empty()) {
        float action_width = nvgTextBounds(ctx, 0.f, 0.f,
                                           m_action_label.c_str(), nullptr, nullptr);
        float action_padding = 16.f;
        float action_right = x + w - action_padding;
        float action_left = action_right - action_width;
        nvgFontFace(ctx, "sans-bold");
        nvgFillColor(ctx, to_nvg(palette.action));
        nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, action_right, text_y, m_action_label.c_str(), nullptr);

        m_action_min = {action_left - 4.f, y};
        m_action_max = {action_right + 4.f, y + h};
    }

    nvgRestore(ctx);
}

bool FluentWebToast::mouse_button_event(const Vector2i &p, int button,
                                        bool down, int modifiers) {
    if (!m_visible || button != GLFW_MOUSE_BUTTON_1 || !down)
        return false;

    if (!m_action_label.empty()) {
        Vector2f local = Vector2f(p - m_pos);
        if (local.x() >= m_action_min.x() && local.x() <= m_action_max.x() &&
            local.y() >= m_action_min.y() && local.y() <= m_action_max.y()) {
            if (m_action_callback)
                m_action_callback();
            hide();
            return true;
        }
    }

    hide();
    return true;
}

NAMESPACE_END(nanogui)
