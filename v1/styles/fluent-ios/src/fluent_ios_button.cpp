#include <cmath>
#include <algorithm>

#include <nanogui/fluent_ios_button.h>
#include <nanogui/fluent_ios_theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

namespace {
static const FluentIOSTheme::TypographyToken kFallbackTypography[] = {
    {34, 41, 1}, // LargeTitle
    {28, 34, 0}, // Title1
    {22, 28, 0}, // Title2
    {20, 25, 0}, // Title3
    {17, 22, 1}, // Headline
    {17, 22, 0}, // Body
    {16, 21, 0}, // Callout
    {15, 20, 0}, // Subheadline
    {13, 18, 0}, // Footnote
    {12, 16, 0}, // Caption1
    {11, 13, 0}  // Caption2
};

inline Color mix(const Color &a, const Color &b, float t) {
    return Color(
        a.r() + (b.r() - a.r()) * t,
        a.g() + (b.g() - a.g()) * t,
        a.b() + (b.b() - a.b()) * t,
        a.w() + (b.w() - a.w()) * t
    );
}

inline Color with_alpha(const Color &c, float alpha) {
    return Color(c.r(), c.g(), c.b(), alpha);
}
}

FluentIOSButton::FluentIOSButton(Widget *parent,
                                 const std::string &title,
                                 Style style,
                                 bool destructive)
    : Button(parent, title), m_style(style), m_destructive(destructive) {
    set_icon_position(IconPosition::LeftCentered);
}

Vector2i FluentIOSButton::preferred_size_impl(NVGcontext *ctx) const {
    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());
    auto token_for = [&](FluentIOSTheme::TypographyStyle style) -> const FluentIOSTheme::TypographyToken & {
        return ios_theme ? ios_theme->typography(style)
                         : kFallbackTypography[static_cast<int>(style)];
    };

    const auto &type = token_for(font_style());

    float horizontal = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Large) : 20.f;
    float vertical = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::XSmall) : 12.f;
    float icon_gap = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Small) : 10.f;

    nvgFontSize(ctx, static_cast<float>(type.size));
    nvgFontFace(ctx, type.weight ? "sans-bold" : "sans");

    float text_width = nvgTextBounds(ctx, 0.f, 0.f, m_caption.c_str(), nullptr, nullptr);

    float width = text_width + horizontal * 2.f;
    if (m_icon)
        width += type.size + icon_gap;

    float height = type.line_height + vertical * 2.f;

    Vector2i result(static_cast<int>(std::ceil(width)), static_cast<int>(std::ceil(height)));
    result.x() = std::max(result.x(), 88);
    result.y() = std::max(result.y(), 44);
    return result;
}

void FluentIOSButton::draw(NVGcontext *ctx) {
    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());
    auto token_for = [&](FluentIOSTheme::TypographyStyle style) -> const FluentIOSTheme::TypographyToken & {
        return ios_theme ? ios_theme->typography(style)
                         : kFallbackTypography[static_cast<int>(style)];
    };

    const auto &type = token_for(font_style());

    float horizontal = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Large) : 20.f;
    float vertical = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::XSmall) : 12.f;
    float icon_gap = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Small) : 10.f;

    float radius = ios_theme ? ios_theme->corner_radius_large() : 14.f;

    Color accent = ios_theme
        ? ios_theme->color(m_destructive
                               ? FluentIOSTheme::SemanticColor::StatusDanger
                               : FluentIOSTheme::SemanticColor::AccentPrimary)
        : (m_destructive ? Color(0.984f, 0.290f, 0.304f, 1.f)
                         : Color(0.f, 0.478f, 1.f, 1.f));
    Color neutral = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::BackgroundSecondary)
        : Color(0.898f, 0.902f, 0.933f, 1.f);
    Color surface = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::Surface)
        : Color(1.f, 1.f, 1.f, 1.f);

    Color text_color = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::TextPrimary)
        : Color(0.09f, 0.09f, 0.11f, 1.f);
    Color disabled_text = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::TextMuted)
        : Color(text_color.r(), text_color.g(), text_color.b(), 0.4f);
    Color border_color = Color(0.f, 0.f, 0.f, 0.f);
    Color background;

    if (!m_enabled) {
        text_color = disabled_text;
        background = with_alpha(neutral, neutral.w() * 0.45f);
    } else {
        switch (m_style) {
        case Style::Filled:
            background = accent;
            if (m_pushed)
                background = mix(background, Color(0.f, 0.f, 0.f, background.w()), 0.12f);
            text_color = Color(1.f, 1.f, 1.f, 1.f);
            break;
        case Style::Gray:
            background = neutral;
            if (m_pushed)
                background = mix(background, Color(0.f, 0.f, 0.f, background.w()), 0.08f);
            break;
        case Style::Outline:
            background = with_alpha(surface, 0.f);
            border_color = accent;
            if (m_pushed)
                background = with_alpha(accent, accent.w() * 0.18f);
            break;
        }
    }

    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());

    nvgSave(ctx);

    auto draw_shadow_layer = [&](const FluentIOSTheme::ShadowComponent &layer, float target_radius) {
        if (layer.opacity <= 0.f || layer.blur_radius <= 0.f)
            return;
        float blur = std::max(layer.blur_radius, 0.5f);
        Color color(0.f, 0.f, 0.f, layer.opacity);
        NVGpaint paint = nvgBoxGradient(
            ctx,
            x + layer.x_offset,
            y + layer.y_offset,
            w,
            h,
            target_radius,
            blur,
            nvgRGBAf(color.r(), color.g(), color.b(), color.w()),
            nvgRGBAf(color.r(), color.g(), color.b(), 0.f));
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx,
                       x + layer.x_offset - blur,
                       y + layer.y_offset - blur,
                       w + blur * 2.f,
                       h + blur * 2.f,
                       target_radius + blur);
        nvgFillPaint(ctx, paint);
        nvgFill(ctx);
    };

    if (ios_theme && m_enabled && (m_style == Style::Filled || m_style == Style::Gray)) {
        auto level = (m_style == Style::Filled)
            ? FluentIOSTheme::ElevationLevel::Level3
            : FluentIOSTheme::ElevationLevel::Level2;
        const auto &shadow = ios_theme->elevation(level);
        draw_shadow_layer(shadow.ambient, radius);
        draw_shadow_layer(shadow.key, radius);
    }

    if (background.w() > 0.f) {
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, w, h, radius);
        nvgFillColor(ctx, nvgRGBAf(background.r(), background.g(), background.b(), background.w()));
        nvgFill(ctx);
    }

    if (border_color.w() > 0.f) {
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f, radius - 0.5f);
        nvgStrokeColor(ctx, nvgRGBAf(border_color.r(), border_color.g(), border_color.b(), border_color.w()));
        nvgStrokeWidth(ctx, 1.5f);
        nvgStroke(ctx);
    }

    const char *font_face = type.weight ? "sans-bold" : "sans";
    nvgFontSize(ctx, static_cast<float>(type.size));
    nvgFontFace(ctx, font_face);
    nvgFillColor(ctx, nvgRGBAf(text_color.r(), text_color.g(), text_color.b(), text_color.w()));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    float content_y = y + h * 0.5f;
    float text_x = x + horizontal;

    if (m_icon) {
        auto icon = utf8(m_icon);
        nvgFontFace(ctx, "icons");
        nvgFontSize(ctx, static_cast<float>(type.size));
        nvgText(ctx, text_x, content_y, icon.data(), nullptr);
        text_x += type.size + icon_gap;
        nvgFontFace(ctx, font_face);
    }

    nvgText(ctx, text_x, content_y, m_caption.c_str(), nullptr);

    nvgRestore(ctx);
    Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
