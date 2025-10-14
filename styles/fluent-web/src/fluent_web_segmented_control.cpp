#include <nanogui/fluent_web_segmented_control.h>

#include <nanogui/opengl.h>

#include <algorithm>

NAMESPACE_BEGIN(nanogui)

namespace {

NVGcolor to_nvg(const Color &c, float alpha_override = -1.f) {
    float alpha = alpha_override >= 0.f ? alpha_override : c.w();
    return nvgRGBAf(c.r(), c.g(), c.b(), alpha);
}

} // namespace

FluentWebSegmentedControl::FluentWebSegmentedControl(Widget *parent, SelectMode mode)
    : Widget(parent), m_mode(mode), m_segment_padding(12.f) {}

void FluentWebSegmentedControl::add_segment(const std::string &label, int icon) {
    m_segments.push_back({label, icon, false});
    preferred_size_changed();
}

void FluentWebSegmentedControl::set_selected(int index, bool selected) {
    if (index < 0 || index >= static_cast<int>(m_segments.size()))
        return;

    if (m_mode == SelectMode::Single && selected) {
        for (auto &seg : m_segments)
            seg.selected = false;
    }

    m_segments[index].selected = selected;

    if (m_callback)
        m_callback(selected_indices());
}

std::vector<int> FluentWebSegmentedControl::selected_indices() const {
    std::vector<int> indices;
    for (size_t i = 0; i < m_segments.size(); ++i) {
        if (m_segments[i].selected)
            indices.push_back(static_cast<int>(i));
    }
    return indices;
}

void FluentWebSegmentedControl::set_mode(SelectMode mode) {
    m_mode = mode;
    preferred_size_changed();
}

void FluentWebSegmentedControl::set_theme(Theme *theme) {
    Widget::set_theme(theme);
    auto *fluent = dynamic_cast<FluentWebTheme *>(theme);
    if (fluent)
        m_segment_padding = fluent->spacing(FluentWebTheme::SpaceToken::S);
}

Vector2i FluentWebSegmentedControl::preferred_size_impl(NVGcontext *ctx) const {
    if (m_segments.empty())
        return Vector2i(0, 32);

    nvgFontFace(ctx, "sans");
    nvgFontSize(ctx, 14.f);

    float total_width = 0.f;
    for (const auto &seg : m_segments) {
        float text_width = nvgTextBounds(ctx, 0.f, 0.f, seg.label.c_str(), nullptr, nullptr);
        total_width += text_width + m_segment_padding * 2.f;
    }

    return Vector2i(static_cast<int>(std::round(total_width)), 36);
}

bool FluentWebSegmentedControl::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);

    if (!down || button != GLFW_MOUSE_BUTTON_1 || m_segments.empty())
        return false;

    float segment_width = static_cast<float>(m_size.x()) / std::max<size_t>(1, m_segments.size());
    int index = static_cast<int>((p.x() - m_pos.x()) / segment_width);

    if (index >= 0 && index < static_cast<int>(m_segments.size())) {
        bool currently_selected = m_segments[index].selected;
        set_selected(index, m_mode == SelectMode::Multi ? !currently_selected : true);
        return true;
    }

    return false;
}

void FluentWebSegmentedControl::draw(NVGcontext *ctx) {
    Widget::draw(ctx);

    auto *fluent = dynamic_cast<FluentWebTheme *>(this->theme());
    if (!fluent || m_segments.empty())
        return;

    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float width = static_cast<float>(m_size.x());
    float height = static_cast<float>(m_size.y());
    float segment_width = width / std::max<size_t>(1, m_segments.size());

    Color background = fluent->color(FluentWebTheme::ColorToken::colorNeutralBackground1);
    Color border = fluent->color(FluentWebTheme::ColorToken::colorNeutralStroke1);
    Color selected_background = fluent->color(FluentWebTheme::ColorToken::colorBrandBackground);
    Color selected_text = fluent->color(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand);
    Color text_color = fluent->color(FluentWebTheme::ColorToken::colorNeutralForeground1);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, width, height,
                   fluent->corner_radius(FluentWebTheme::RadiusToken::Large));
    nvgFillColor(ctx, to_nvg(background));
    nvgFill(ctx);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x + 0.5f, y + 0.5f,
                   width - 1.f, height - 1.f,
                   fluent->corner_radius(FluentWebTheme::RadiusToken::Large));
    nvgStrokeWidth(ctx, 1.f);
    nvgStrokeColor(ctx, to_nvg(border));
    nvgStroke(ctx);

    for (size_t i = 0; i < m_segments.size(); ++i) {
        float seg_x = x + i * segment_width;
        const auto &seg = m_segments[i];

        if (seg.selected) {
            nvgBeginPath(ctx);
            nvgRoundedRect(ctx, seg_x + 1.f, y + 1.f, segment_width - 2.f, height - 2.f,
                           fluent->corner_radius(FluentWebTheme::RadiusToken::Large) - 2.f);
            nvgFillColor(ctx, to_nvg(selected_background));
            nvgFill(ctx);
        }

        nvgFontFace(ctx, "sans");
        nvgFontSize(ctx, 14.f);
        nvgFillColor(ctx, to_nvg(seg.selected ? selected_text : text_color));
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(ctx, seg_x + segment_width * 0.5f, y + height * 0.5f,
                seg.label.c_str(), nullptr);

        if (i < m_segments.size() - 1) {
            nvgBeginPath(ctx);
            nvgMoveTo(ctx, seg_x + segment_width, y + 4.f);
            nvgLineTo(ctx, seg_x + segment_width, y + height - 4.f);
            nvgStrokeWidth(ctx, 1.f);
            nvgStrokeColor(ctx, to_nvg(border, 0.6f));
            nvgStroke(ctx);
        }
    }
}

NAMESPACE_END(nanogui)
