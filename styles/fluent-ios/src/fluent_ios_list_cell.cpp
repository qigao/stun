#include <cmath>
#include <algorithm>

#include <nanogui/fluent_ios_list_cell.h>
#include <nanogui/fluent_ios_theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

namespace {
static const FluentIOSTheme::TypographyToken kFallbackTitle{17, 22, 1};
static const FluentIOSTheme::TypographyToken kFallbackSubtitle{13, 18, 0};
}

FluentIOSListCell::FluentIOSListCell(Widget *parent,
                                     std::string title,
                                     std::string subtitle,
                                     Accessory accessory)
    : Widget(parent), m_title(std::move(title)), m_subtitle(std::move(subtitle)), m_accessory(accessory) {}

void FluentIOSListCell::set_title(const std::string &title) {
    m_title = title;
}

void FluentIOSListCell::set_subtitle(const std::string &subtitle) {
    m_subtitle = subtitle;
}

Vector2i FluentIOSListCell::preferred_size_impl(NVGcontext *ctx) const {
    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());
    auto title_token = ios_theme ? ios_theme->typography(FluentIOSTheme::TypographyStyle::Headline)
                                 : kFallbackTitle;
    auto subtitle_token = ios_theme ? ios_theme->typography(FluentIOSTheme::TypographyStyle::Subheadline)
                                    : kFallbackSubtitle;

    float vertical = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Medium) : 16.f;
    float horizontal = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Large) : 20.f;
    float gap = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Small) : 10.f;

    nvgFontSize(ctx, static_cast<float>(title_token.size));
    nvgFontFace(ctx, title_token.weight ? "sans-bold" : "sans");

    float width = horizontal * 2.f;
    if (m_leading_icon)
        width += title_token.size + gap;

    width += nvgTextBounds(ctx, 0.f, 0.f, m_title.c_str(), nullptr, nullptr);

    if (!m_subtitle.empty()) {
        nvgFontSize(ctx, static_cast<float>(subtitle_token.size));
        nvgFontFace(ctx, subtitle_token.weight ? "sans-bold" : "sans");
        width = std::max(width, horizontal * 2.f + (m_leading_icon ? title_token.size + gap : 0.f) +
                               nvgTextBounds(ctx, 0.f, 0.f, m_subtitle.c_str(), nullptr, nullptr));
    }

    if (m_accessory == Accessory::Chevron)
        width += title_token.size + gap;

    float height = title_token.line_height;
    if (!m_subtitle.empty())
        height += subtitle_token.line_height * 0.9f;
    height += vertical * 2.f;

    return Vector2i(static_cast<int>(std::ceil(width)), static_cast<int>(std::ceil(height)));
}

void FluentIOSListCell::draw(NVGcontext *ctx) {
    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());
    auto title_token = ios_theme ? ios_theme->typography(FluentIOSTheme::TypographyStyle::Headline)
                                 : kFallbackTitle;
    auto subtitle_token = ios_theme ? ios_theme->typography(FluentIOSTheme::TypographyStyle::Subheadline)
                                    : kFallbackSubtitle;

    float vertical = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Medium) : 16.f;
    float horizontal = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Large) : 20.f;
    float gap = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Small) : 10.f;

    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());

    Color base = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::BackgroundSecondary)
        : Color(0.965f, 0.969f, 0.98f, 1.f);
    Color highlight = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::AccentTertiary)
        : Color(0.88f, 0.93f, 1.f, 1.f);
    Color text_primary = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::TextPrimary)
        : Color(0.f, 0.f, 0.f, 1.f);
    Color text_secondary = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::TextSecondary)
        : Color(text_primary.r(), text_primary.g(), text_primary.b(), 0.65f);

    nvgSave(ctx);

    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, nvgRGBAf((m_selected ? highlight : base).r(),
                               (m_selected ? highlight : base).g(),
                               (m_selected ? highlight : base).b(),
                               (m_selected ? highlight : base).w()));
    nvgFill(ctx);

    float cursor_x = x + horizontal;
    float baseline = y + vertical + title_token.line_height * 0.5f;

    if (m_leading_icon) {
        nvgFontFace(ctx, "icons");
        nvgFontSize(ctx, static_cast<float>(title_token.size));
        nvgFillColor(ctx, nvgRGBAf(text_secondary.r(), text_secondary.g(), text_secondary.b(), text_secondary.w()));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, cursor_x, baseline, utf8(m_leading_icon).data(), nullptr);
        cursor_x += title_token.size + gap;
    }

    nvgFontSize(ctx, static_cast<float>(title_token.size));
    nvgFontFace(ctx, title_token.weight ? "sans-bold" : "sans");
    nvgFillColor(ctx, nvgRGBAf(text_primary.r(), text_primary.g(), text_primary.b(), text_primary.w()));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(ctx, cursor_x, baseline, m_title.c_str(), nullptr);

    if (!m_subtitle.empty()) {
        float subtitle_y = baseline + subtitle_token.line_height * 0.9f;
        nvgFontSize(ctx, static_cast<float>(subtitle_token.size));
        nvgFontFace(ctx, subtitle_token.weight ? "sans-bold" : "sans");
        nvgFillColor(ctx, nvgRGBAf(text_secondary.r(), text_secondary.g(), text_secondary.b(), text_secondary.w()));
        nvgText(ctx, cursor_x, subtitle_y, m_subtitle.c_str(), nullptr);
    }

    if (m_accessory == Accessory::Chevron) {
        float chevron_x = x + w - horizontal;
        float chevron_y = y + h * 0.5f;
        nvgBeginPath(ctx);
        nvgMoveTo(ctx, chevron_x - 6.f, chevron_y - 8.f);
        nvgLineTo(ctx, chevron_x, chevron_y);
        nvgLineTo(ctx, chevron_x - 6.f, chevron_y + 8.f);
        nvgStrokeWidth(ctx, 2.f);
        nvgStrokeColor(ctx, nvgRGBAf(text_secondary.r(), text_secondary.g(), text_secondary.b(), text_secondary.w()));
        nvgLineCap(ctx, NVG_ROUND);
        nvgStroke(ctx);
    }

    nvgRestore(ctx);

    Widget::draw(ctx);
}

bool FluentIOSListCell::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    if (button != GLFW_MOUSE_BUTTON_1 || !down)
        return Widget::mouse_button_event(p, button, down, modifiers);

    Vector2i local = p - m_pos;
    if (local.x() < 0 || local.x() > m_size.x() || local.y() < 0 || local.y() > m_size.y())
        return false;

    set_selected(true);
    if (m_callback)
        m_callback();
    return true;
}

NAMESPACE_END(nanogui)
