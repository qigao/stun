#include <nanogui/fluent_web_message_bar.h>

#include <nanogui/fluent_icons.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#include <algorithm>

NAMESPACE_BEGIN(nanogui)

namespace {

NVGcolor to_nvg(const Color &c) {
    return nvgRGBAf(c.r(), c.g(), c.b(), c.w());
}

int severity_icon(FluentWebMessageBar::Severity severity) {
    switch (severity) {
        case FluentWebMessageBar::Severity::Success:
            return FLUENT_ICON_CHECKMARK_CIRCLE;
        case FluentWebMessageBar::Severity::Warning:
            return FLUENT_ICON_WARNING;
        case FluentWebMessageBar::Severity::Error:
            return FLUENT_ICON_DISMISS_CIRCLE;
        case FluentWebMessageBar::Severity::Informational:
        default:
            return FLUENT_ICON_INFO;
    }
}

} // namespace

FluentWebMessageBar::FluentWebMessageBar(Widget *parent,
                                         const std::string &message,
                                         Severity severity)
    : Widget(parent),
      m_message(message),
      m_severity(severity),
      m_closable(true) {}

void FluentWebMessageBar::set_message(const std::string &message) {
    if (m_message == message)
        return;
    m_message = message;
    preferred_size_changed();
}

void FluentWebMessageBar::set_severity(Severity severity) {
    if (m_severity == severity)
        return;
    m_severity = severity;
    preferred_size_changed();
}

void FluentWebMessageBar::set_action(const std::string &label,
                                     const std::function<void()> &callback) {
    m_action_label = label;
    m_action_callback = callback;
    preferred_size_changed();
}

void FluentWebMessageBar::clear_action() {
    m_action_label.clear();
    m_action_callback = nullptr;
    preferred_size_changed();
}

void FluentWebMessageBar::set_closable(bool closable) {
    if (m_closable == closable)
        return;
    m_closable = closable;
    preferred_size_changed();
}

FluentWebMessageBar::Palette
FluentWebMessageBar::resolve_palette(const FluentWebTheme &theme) const {
    auto c = [&](FluentWebTheme::ColorToken token) -> Color {
        return theme.color(token);
    };

    Palette palette{};
    switch (m_severity) {
        case Severity::Success:
            palette.accent = c(FluentWebTheme::ColorToken::colorStatusSuccessBorder2);
            palette.border = c(FluentWebTheme::ColorToken::colorStatusSuccessBorder1);
            palette.background = c(FluentWebTheme::ColorToken::colorStatusSuccessBackground2);
            palette.text = c(FluentWebTheme::ColorToken::colorStatusSuccessForeground2);
            palette.icon = c(FluentWebTheme::ColorToken::colorStatusSuccessForeground3);
            palette.action = c(FluentWebTheme::ColorToken::colorStatusSuccessForeground3);
            break;
        case Severity::Warning:
            palette.accent = c(FluentWebTheme::ColorToken::colorStatusWarningBorder2);
            palette.border = c(FluentWebTheme::ColorToken::colorStatusWarningBorder1);
            palette.background = c(FluentWebTheme::ColorToken::colorStatusWarningBackground2);
            palette.text = c(FluentWebTheme::ColorToken::colorStatusWarningForeground2);
            palette.icon = c(FluentWebTheme::ColorToken::colorStatusWarningForeground3);
            palette.action = c(FluentWebTheme::ColorToken::colorStatusWarningForeground3);
            break;
        case Severity::Error:
            palette.accent = c(FluentWebTheme::ColorToken::colorStatusDangerBorder2);
            palette.border = c(FluentWebTheme::ColorToken::colorStatusDangerBorder1);
            palette.background = c(FluentWebTheme::ColorToken::colorStatusDangerBackground2);
            palette.text = c(FluentWebTheme::ColorToken::colorStatusDangerForeground2);
            palette.icon = c(FluentWebTheme::ColorToken::colorStatusDangerForeground3);
            palette.action = c(FluentWebTheme::ColorToken::colorStatusDangerForeground3);
            break;
        case Severity::Informational:
        default:
            palette.accent = c(FluentWebTheme::ColorToken::colorBrandBackground);
            palette.border = c(FluentWebTheme::ColorToken::colorBrandStroke1);
            palette.background = c(FluentWebTheme::ColorToken::colorNeutralBackground1);
            palette.text = c(FluentWebTheme::ColorToken::colorNeutralForeground1);
            palette.icon = c(FluentWebTheme::ColorToken::colorBrandForeground1);
            palette.action = c(FluentWebTheme::ColorToken::colorBrandForegroundLink);
            break;
    }

    return palette;
}

Vector2i FluentWebMessageBar::preferred_size_impl(NVGcontext *ctx) const {
    int base_height = 48;
    int font_size = m_font_size == -1 ? 16 : m_font_size;
    nvgFontFace(ctx, "sans");
    nvgFontSize(ctx, static_cast<float>(font_size));
    float text_width = nvgTextBounds(ctx, 0.f, 0.f, m_message.c_str(), nullptr, nullptr);

    float width = text_width + base_height * 1.5f; // paddings + icon + actions
    if (!m_action_label.empty()) {
        width += nvgTextBounds(ctx, 0.f, 0.f,
                               m_action_label.c_str(), nullptr, nullptr) + 24.f;
    }
    if (m_closable)
        width += 32.f;

    return Vector2i(static_cast<int>(std::round(width)),
                    base_height);
}

void FluentWebMessageBar::draw(NVGcontext *ctx) {
    auto *fluent = dynamic_cast<FluentWebTheme *>(this->theme());
    if (!fluent) {
        Widget::draw(ctx);
        return;
    }

    Widget::draw(ctx);

    const Palette palette = resolve_palette(*fluent);

    const float accent_width = fluent->spacing(FluentWebTheme::SpaceToken::XS);
    const float horizontal_padding = fluent->spacing(FluentWebTheme::SpaceToken::M);
    const float vertical_padding = fluent->spacing(FluentWebTheme::SpaceToken::S);

    const float x = static_cast<float>(m_pos.x());
    const float y = static_cast<float>(m_pos.y());
    const float w = static_cast<float>(m_size.x());
    const float h = static_cast<float>(m_size.y());
    const float radius = fluent->corner_radius(FluentWebTheme::RadiusToken::Large);

    // Background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, radius);
    nvgFillColor(ctx, to_nvg(palette.background));
    nvgFill(ctx);

    // Border
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f, std::max(radius - 0.5f, 0.f));
    nvgStrokeWidth(ctx, 1.f);
    nvgStrokeColor(ctx, to_nvg(palette.border));
    nvgStroke(ctx);

    // Accent strip
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, accent_width, h, std::min(radius, accent_width));
    nvgFillColor(ctx, to_nvg(palette.accent));
    nvgFill(ctx);

    // Icon
    int icon = severity_icon(m_severity);
    float icon_size = 18.f;
    nvgFontFace(ctx, "fluent-icons");
    nvgFontSize(ctx, icon_size);
    nvgFillColor(ctx, to_nvg(palette.icon));
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    float icon_x = x + accent_width + horizontal_padding;
    float icon_y = y + h / 2.f;
    nvgText(ctx, icon_x, icon_y, utf8(icon).data(), nullptr);

    // Message text
    float text_start = icon_x + icon_size * 0.9f + horizontal_padding * 0.5f;
    nvgFontFace(ctx, "sans");
    int font_size = m_font_size == -1 ? 16 : m_font_size;
    nvgFontSize(ctx, static_cast<float>(font_size));
    nvgFillColor(ctx, to_nvg(palette.text));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    float text_y = y + h / 2.f;
    nvgText(ctx, text_start, text_y, m_message.c_str(), nullptr);

    float cursor_x = text_start + nvgTextBounds(ctx, 0.f, 0.f,
                                                m_message.c_str(), nullptr, nullptr);

    // Action button
    m_action_bounds = RectF{};
    if (!m_action_label.empty()) {
        float action_padding = fluent->spacing(FluentWebTheme::SpaceToken::S);
        cursor_x += action_padding;
        nvgFontFace(ctx, "sans-bold");
        nvgFontSize(ctx, static_cast<float>(font_size - 1));
        nvgFillColor(ctx, to_nvg(palette.action));
        float action_width = nvgTextBounds(ctx, 0.f, 0.f,
                                           m_action_label.c_str(),
                                           nullptr, nullptr);

        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, cursor_x, text_y, m_action_label.c_str(), nullptr);

        m_action_bounds.min = {cursor_x, text_y - (font_size)};
        m_action_bounds.max = {cursor_x + action_width, text_y + (font_size)};
        cursor_x += action_width;
    } else {
        m_action_bounds = RectF{};
    }

    // Close button
    m_close_bounds = RectF{};
    if (m_closable) {
        float close_padding = fluent->spacing(FluentWebTheme::SpaceToken::M);
        float close_size = 16.f;

        float close_center_x = x + w - close_padding;
        float close_center_y = text_y;

        nvgFontFace(ctx, "fluent-icons");
        nvgFontSize(ctx, close_size);
        nvgFillColor(ctx, to_nvg(palette.icon));
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(ctx, close_center_x, close_center_y,
                utf8(FLUENT_ICON_DISMISS).data(), nullptr);

        m_close_bounds.min = {close_center_x - close_size * 0.6f,
                              close_center_y - close_size * 0.6f};
        m_close_bounds.max = {close_center_x + close_size * 0.6f,
                              close_center_y + close_size * 0.6f};
    } else {
        m_close_bounds = RectF{};
    }
}

bool FluentWebMessageBar::mouse_button_event(const Vector2i &p, int button,
                                             bool down, int modifiers) {
    if (!Widget::mouse_button_event(p, button, down, modifiers))
        ; // intentionally fall through

    if (button != GLFW_MOUSE_BUTTON_1 || !down)
        return false;

    Vector2i local = p - m_pos;

    if (!m_action_label.empty() && m_action_bounds.contains(local)) {
        if (m_action_callback)
            m_action_callback();
        return true;
    }

    if (m_closable && m_close_bounds.contains(local)) {
        if (m_close_callback)
            m_close_callback();
        set_visible(false);
        return true;
    }
    return false;
}

NAMESPACE_END(nanogui)

