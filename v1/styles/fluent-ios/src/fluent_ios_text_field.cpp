#include <cmath>

#include <nanogui/fluent_ios_text_field.h>
#include <nanogui/fluent_ios_theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

namespace {
static const FluentIOSTheme::TypographyToken kFallbackBody{17, 22, 0};
}

FluentIOSTextField::FluentIOSTextField(Widget *parent, const std::string &value)
    : TextBox(parent, value) {
    set_alignment(Alignment::Left);
    set_editable(true);
    TextBox::set_placeholder("");
}

void FluentIOSTextField::set_placeholder(const std::string &placeholder) {
    m_placeholder = placeholder;
    TextBox::set_placeholder("");
    preferred_size_changed();
}

Vector2i FluentIOSTextField::preferred_size_impl(NVGcontext *ctx) const {
    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());
    auto token = ios_theme ? ios_theme->typography(FluentIOSTheme::TypographyStyle::Body) : kFallbackBody;
    Vector2i base = TextBox::preferred_size_impl(ctx);
    float vertical = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::XSmall) : 12.f;
    base.y() = static_cast<int>(std::ceil(token.line_height + vertical * 2.f));
    return base;
}

void FluentIOSTextField::draw(NVGcontext *ctx) {
    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());

    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());

    Color fill = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::Surface)
        : Color(1.f, 1.f, 1.f, 1.f);
    if (!m_enabled)
        fill = Color(fill.r(), fill.g(), fill.b(), fill.w() * 0.85f);

    float desired_radius = ios_theme ? ios_theme->corner_radius_large() : std::min(h * 0.25f, 12.f);

    auto draw_shadow_layer = [&](const FluentIOSTheme::ShadowComponent &layer) {
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
            desired_radius,
            blur,
            nvgRGBAf(color.r(), color.g(), color.b(), color.w()),
            nvgRGBAf(color.r(), color.g(), color.b(), 0.f));
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx,
                       x + layer.x_offset - blur,
                       y + layer.y_offset - blur,
                       w + blur * 2.f,
                       h + blur * 2.f,
                       desired_radius + blur);
        nvgFillPaint(ctx, paint);
        nvgFill(ctx);
    };

    if (ios_theme && m_enabled) {
        nvgSave(ctx);
        auto level = m_focused
            ? FluentIOSTheme::ElevationLevel::Level2
            : FluentIOSTheme::ElevationLevel::Level1;
        const auto &shadow = ios_theme->elevation(level);
        draw_shadow_layer(shadow.ambient);
        draw_shadow_layer(shadow.key);
        nvgRestore(ctx);
    }

    Color previous_fill = solid_color();
    float previous_radius = corner_radius();

    set_solid_color(fill);
    set_corner_radius(desired_radius);
    TextBox::draw(ctx);
    set_solid_color(previous_fill);
    set_corner_radius(previous_radius);

    Color border = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::Separator)
        : Color(0.82f, 0.85f, 0.88f, 1.f);
    Color focus = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::AccentPrimary)
        : Color(0.f, 0.478f, 1.f, 1.f);

    nvgSave(ctx);
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, desired_radius);
    Color stroke = m_focused ? focus : border;
    nvgStrokeColor(ctx, nvgRGBAf(stroke.r(), stroke.g(), stroke.b(), 1.f));
    nvgStrokeWidth(ctx, m_focused ? 2.f : 1.f);
    nvgStroke(ctx);

    if (!m_placeholder.empty() && value().empty() && !m_focused) {
        auto token = ios_theme ? ios_theme->typography(FluentIOSTheme::TypographyStyle::Subheadline) : kFallbackBody;
        nvgFontSize(ctx, static_cast<float>(token.size));
        nvgFontFace(ctx, token.weight ? "sans-bold" : "sans");
        Color placeholder = ios_theme
            ? ios_theme->color(FluentIOSTheme::SemanticColor::TextMuted)
            : Color(0.f, 0.f, 0.f, 0.45f);
        nvgFillColor(ctx, nvgRGBAf(placeholder.r(), placeholder.g(), placeholder.b(), placeholder.w()));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        float horizontal = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Large) : 20.f;
        nvgText(ctx, x + horizontal, y + h * 0.5f, m_placeholder.c_str(), nullptr);
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
