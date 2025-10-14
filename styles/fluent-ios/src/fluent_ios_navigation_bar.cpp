#include <algorithm>
#include <cmath>

#include <nanogui/fluent_ios_navigation_bar.h>
#include <nanogui/fluent_ios_theme.h>
#include <nanogui/layout.h>
#include <nanogui/opengl.h>
#include <nanogui/button.h>
#include <nanogui/screen.h>

NAMESPACE_BEGIN(nanogui)

namespace {
static const FluentIOSTheme::TypographyToken kFallbackHeadline{17, 22, 1};
static const FluentIOSTheme::TypographyToken kFallbackSubheadline{15, 20, 0};
}

FluentIOSNavigationBar::FluentIOSNavigationBar(Widget *parent, const std::string &title)
    : Widget(parent), m_title(title), m_show_large_title(false), m_back_button(false) {
    m_trailing_container = new Widget(this);
    m_trailing_container->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 8));
}

void FluentIOSNavigationBar::set_title(const std::string &title) {
    m_title = title;
    if (auto sc = screen())
        sc->perform_layout();
}

void FluentIOSNavigationBar::set_subtitle(const std::string &subtitle) {
    m_subtitle = subtitle;
    if (auto sc = screen())
        sc->perform_layout();
}

Vector2i FluentIOSNavigationBar::preferred_size_impl(NVGcontext *) const {
    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());
    float base = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::XLarge) : 28.f;
    float height = m_show_large_title ? base * 3.f : base * 2.f;
    return Vector2i(m_parent ? m_parent->width() : 320, static_cast<int>(std::ceil(height)));
}

void FluentIOSNavigationBar::perform_layout(NVGcontext *ctx) {
    Widget::perform_layout(ctx);

    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());
    float horizontal = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Large) : 20.f;
    float top = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Small) : 16.f;

    Vector2i trailing_pref = m_trailing_container->preferred_size(ctx);
    int trailing_x = m_size.x() - trailing_pref.x() - static_cast<int>(horizontal);
    int trailing_y = m_show_large_title ? static_cast<int>(top)
                                        : static_cast<int>((m_size.y() - trailing_pref.y()) * 0.5f);

    m_trailing_container->set_position(Vector2i(std::max(16, trailing_x), std::max(8, trailing_y)));
    m_trailing_container->set_size(trailing_pref);
    m_trailing_container->perform_layout(ctx);
}

void FluentIOSNavigationBar::draw(NVGcontext *ctx) {
    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());

    Color background = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::SurfaceElevated)
        : Color(0.96f, 0.97f, 0.99f, 0.95f);
    Color separator = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::Separator)
        : Color(0.82f, 0.85f, 0.88f, 1.f);

    float horizontal = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Large) : 20.f;
    float vertical = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Small) : 16.f;

    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());

    nvgSave(ctx);

    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, nvgRGBAf(background.r(), background.g(), background.b(), background.w()));
    nvgFill(ctx);

    if (ios_theme) {
        const auto &shadow = ios_theme->elevation(FluentIOSTheme::ElevationLevel::Level3);
        auto draw_layer = [&](const FluentIOSTheme::ShadowComponent &layer) {
            if (layer.opacity <= 0.f || layer.blur_radius <= 0.f)
                return;
            float blur = std::max(layer.blur_radius, 0.5f);
            float rect_y = y + h + layer.y_offset;
            float rect_height = blur * 2.f;
            Color layer_color(0.f, 0.f, 0.f, layer.opacity);
            NVGpaint paint = nvgBoxGradient(
                ctx,
                x + layer.x_offset,
                rect_y,
                w,
                rect_height,
                0.f,
                blur,
                nvgRGBAf(layer_color.r(), layer_color.g(), layer_color.b(), layer_color.w()),
                nvgRGBAf(layer_color.r(), layer_color.g(), layer_color.b(), 0.f));
            nvgBeginPath(ctx);
            nvgRect(ctx,
                    x + layer.x_offset - blur,
                    rect_y - blur,
                    w + blur * 2.f,
                    rect_height + blur * 2.f);
            nvgFillPaint(ctx, paint);
            nvgFill(ctx);
        };

        draw_layer(shadow.ambient);
        draw_layer(shadow.key);
    }

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x, y + h - 0.5f);
    nvgLineTo(ctx, x + w, y + h - 0.5f);
    nvgStrokeWidth(ctx, 1.f);
    nvgStrokeColor(ctx, nvgRGBAf(separator.r(), separator.g(), separator.b(), separator.w()));
    nvgStroke(ctx);

    Color text_primary = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::TextPrimary)
        : Color(0.1f, 0.1f, 0.12f, 1.f);
    Color text_secondary = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::TextSecondary)
        : Color(text_primary.r(), text_primary.g(), text_primary.b(), 0.6f);

    auto font_for = [&](FluentIOSTheme::TypographyStyle style,
                        const FluentIOSTheme::TypographyToken &fallback) {
        return ios_theme ? ios_theme->typography(style) : fallback;
    };

    auto title_token = font_for(m_show_large_title ? FluentIOSTheme::TypographyStyle::LargeTitle
                                                   : FluentIOSTheme::TypographyStyle::Title2,
                                kFallbackHeadline);
    auto subtitle_token = font_for(FluentIOSTheme::TypographyStyle::Subheadline, kFallbackSubheadline);

    auto apply_font = [&](const FluentIOSTheme::TypographyToken &token) {
        nvgFontSize(ctx, static_cast<float>(token.size));
        nvgFontFace(ctx, token.weight ? "sans-bold" : "sans");
    };

    float text_left = x + horizontal + (m_back_button ? title_token.size + horizontal * 0.35f : 0.f);
    float inline_baseline = y + h * 0.5f;

    if (!m_show_large_title && !m_subtitle.empty()) {
        apply_font(subtitle_token);
        nvgFillColor(ctx, nvgRGBAf(text_secondary.r(), text_secondary.g(), text_secondary.b(), text_secondary.w()));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, text_left, inline_baseline - subtitle_token.line_height * 0.55f, m_subtitle.c_str(), nullptr);
    }

    apply_font(title_token);
    nvgFillColor(ctx, nvgRGBAf(text_primary.r(), text_primary.g(), text_primary.b(), text_primary.w()));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    float title_y = m_show_large_title ? (y + h - vertical - title_token.line_height * 0.5f)
                                       : inline_baseline;
    nvgText(ctx, text_left, title_y, m_title.c_str(), nullptr);

    if (m_back_button) {
        NVGcolor arrow_color = nvgRGBAf(text_primary.r(), text_primary.g(), text_primary.b(), text_primary.w());
        float arrow_x = x + horizontal * 0.6f;
        float arrow_y = m_show_large_title ? (y + vertical + title_token.line_height * 0.4f)
                                           : inline_baseline;
        nvgBeginPath(ctx);
        nvgMoveTo(ctx, arrow_x + 8.f, arrow_y - 9.f);
        nvgLineTo(ctx, arrow_x, arrow_y);
        nvgLineTo(ctx, arrow_x + 8.f, arrow_y + 9.f);
        nvgStrokeColor(ctx, arrow_color);
        nvgStrokeWidth(ctx, 2.5f);
        nvgLineCap(ctx, NVG_ROUND);
        nvgStroke(ctx);
    }

    nvgRestore(ctx);

    Widget::draw(ctx);
}

bool FluentIOSNavigationBar::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    if (m_back_button && button == GLFW_MOUSE_BUTTON_1 && down) {
        Vector2i local = p - m_pos;
        if (local.x() >= 0 && local.x() <= 64 && local.y() >= 0 && local.y() <= m_size.y()) {
            if (m_back_callback)
                m_back_callback();
            return true;
        }
    }
    return Widget::mouse_button_event(p, button, down, modifiers);
}

NAMESPACE_END(nanogui)
