#include <cmath>
#include <algorithm>

#include <nanogui/fluent_ios_segmented_control.h>
#include <nanogui/fluent_ios_theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentIOSSegmentedControl::FluentIOSSegmentedControl(Widget *parent, std::vector<std::string> items)
    : Widget(parent), m_items(std::move(items)) {
    if (m_items.empty())
        m_items = {"First", "Second"};
}

void FluentIOSSegmentedControl::set_items(const std::vector<std::string> &items) {
    m_items = items;
    if (m_selected >= (int)m_items.size())
        m_selected = std::max(0, (int)m_items.size() - 1);
}

void FluentIOSSegmentedControl::set_selected_index(int index, bool emit) {
    if (index < 0 || index >= (int)m_items.size() || index == m_selected)
        return;
    m_selected = index;
    if (emit && m_callback)
        m_callback(m_selected);
}

Vector2i FluentIOSSegmentedControl::preferred_size_impl(NVGcontext *ctx) const {
    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());
    auto token = ios_theme ? ios_theme->typography(FluentIOSTheme::TypographyStyle::Body)
                           : FluentIOSTheme::TypographyToken{17, 22, 0};

    float horizontal = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Large) : 20.f;
    float vertical = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::XSmall) : 12.f;
    float gap = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Small) : 10.f;

    nvgFontSize(ctx, static_cast<float>(token.size));
    nvgFontFace(ctx, token.weight ? "sans-bold" : "sans");

    float total_width = horizontal;
    for (size_t i = 0; i < m_items.size(); ++i) {
        total_width += nvgTextBounds(ctx, 0.f, 0.f, m_items[i].c_str(), nullptr, nullptr) + horizontal;
        if (i + 1 < m_items.size())
            total_width += gap;
    }

    float height = token.line_height + vertical * 2.f;
    return Vector2i(static_cast<int>(std::ceil(total_width)), static_cast<int>(std::ceil(height)));
}

void FluentIOSSegmentedControl::draw(NVGcontext *ctx) {
    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());
    auto token = ios_theme ? ios_theme->typography(FluentIOSTheme::TypographyStyle::Body)
                           : FluentIOSTheme::TypographyToken{17, 22, 0};

    float horizontal = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Large) : 20.f;
    float vertical = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::XSmall) : 12.f;
    float gap = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Small) : 10.f;

    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());

    Color track = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::BackgroundSecondary)
        : Color(0.9f, 0.93f, 0.96f, 1.f);
    Color thumb = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::Surface)
        : Color(1.f, 1.f, 1.f, 1.f);
    Color accent = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::AccentPrimary)
        : Color(0.f, 0.478f, 1.f, 1.f);
    Color text_color = ios_theme
        ? ios_theme->color(FluentIOSTheme::SemanticColor::TextSecondary)
        : Color(0.f, 0.f, 0.f, 1.f);

    float radius = ios_theme ? ios_theme->corner_radius_large() : 14.f;

    nvgSave(ctx);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, radius);
    nvgFillColor(ctx, nvgRGBAf(track.r(), track.g(), track.b(), track.w()));
    nvgFill(ctx);

    size_t count = std::max<size_t>(1, m_items.size());
    float segment_width = (w - gap * (count - 1)) / count;
    float thumb_x = x + (segment_width + gap) * m_selected;

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, thumb_x, y, segment_width, h, radius - 2.f);
    nvgFillColor(ctx, nvgRGBAf(thumb.r(), thumb.g(), thumb.b(), thumb.w()));
    nvgFill(ctx);

    nvgFontSize(ctx, static_cast<float>(token.size));
    nvgFontFace(ctx, token.weight ? "sans-bold" : "sans");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    for (size_t i = 0; i < m_items.size(); ++i) {
        float label_x = x + i * (segment_width + gap) + segment_width * 0.5f;
        float label_y = y + h * 0.5f;
        Color color = (int)i == m_selected ? accent : text_color;
        nvgFillColor(ctx, nvgRGBAf(color.r(), color.g(), color.b(), color.w()));
        nvgText(ctx, label_x, label_y, m_items[i].c_str(), nullptr);
    }

    nvgRestore(ctx);

    Widget::draw(ctx);
}

bool FluentIOSSegmentedControl::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    if (button != GLFW_MOUSE_BUTTON_1 || !down || m_items.empty())
        return Widget::mouse_button_event(p, button, down, modifiers);

    Vector2i local = p - m_pos;
    if (local.x() < 0 || local.x() > m_size.x() || local.y() < 0 || local.y() > m_size.y())
        return false;

    const auto *ios_theme = dynamic_cast<const FluentIOSTheme *>(m_theme.get());
    float gap = ios_theme ? ios_theme->spacing(FluentIOSTheme::SpacingToken::Small) : 10.f;
    size_t count = std::max<size_t>(1, m_items.size());
    float segment_width = (m_size.x() - gap * (count - 1)) / count;

    int index = static_cast<int>((local.x() + gap * 0.5f) / (segment_width + gap));
    index = std::clamp(index, 0, static_cast<int>(m_items.size()) - 1);

    set_selected_index(index);
    return true;
}

NAMESPACE_END(nanogui)
