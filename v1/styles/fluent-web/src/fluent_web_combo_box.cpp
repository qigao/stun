#include <nanogui/fluent_web_combo_box.h>

#include <algorithm>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

namespace {

NVGcolor to_nvg(const Color &c) {
    return nvgRGBAf(c.r(), c.g(), c.b(), c.w());
}

} // namespace

FluentWebComboBox::FluentWebComboBox(Widget *parent)
    : ComboBox(parent) {}

FluentWebComboBox::FluentWebComboBox(Widget *parent, const std::vector<std::string> &items)
    : ComboBox(parent, items) {}

FluentWebComboBox::FluentWebComboBox(Widget *parent,
                                     const std::vector<std::string> &items,
                                     const std::vector<std::string> &items_short)
    : ComboBox(parent, items, items_short) {}

const FluentWebTheme *FluentWebComboBox::fluent_theme() const {
    return dynamic_cast<const FluentWebTheme *>(m_theme.get());
}

void FluentWebComboBox::set_theme(Theme *theme) {
    ComboBox::set_theme(theme);
    if (auto *fluent = fluent_theme()) {
        popup()->set_theme(theme);
    }
}

void FluentWebComboBox::draw(NVGcontext *ctx) {
    const FluentWebTheme *fluent = fluent_theme();
    if (!fluent) {
        PopupButton::draw(ctx);
        return;
    }

    if (!m_enabled && m_pushed)
        m_pushed = false;

    popup()->set_visible(m_pushed);

    Widget::draw(ctx);

    const float x = static_cast<float>(m_pos.x());
    const float y = static_cast<float>(m_pos.y());
    const float w = static_cast<float>(m_size.x());
    const float h = static_cast<float>(m_size.y());

    const float radius = fluent->corner_radius(FluentWebTheme::RadiusToken::Medium);
    const float padding = fluent->spacing(FluentWebTheme::SpaceToken::S);

    Color bg = fluent->color(FluentWebTheme::ColorToken::colorNeutralBackground1);
    Color border = fluent->color(FluentWebTheme::ColorToken::colorNeutralStroke1);

    if (!m_enabled) {
        bg = fluent->color(FluentWebTheme::ColorToken::colorNeutralBackgroundDisabled);
        border = fluent->color(FluentWebTheme::ColorToken::colorNeutralStrokeDisabled);
    } else if (m_pushed) {
        bg = fluent->color(FluentWebTheme::ColorToken::colorNeutralBackground1Pressed);
        border = fluent->color(FluentWebTheme::ColorToken::colorNeutralStrokeAccessiblePressed);
    } else if (m_mouse_focus) {
        bg = fluent->color(FluentWebTheme::ColorToken::colorNeutralBackground1Hover);
        border = fluent->color(FluentWebTheme::ColorToken::colorNeutralStrokeAccessibleHover);
    }

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, radius);
    nvgFillColor(ctx, to_nvg(bg));
    nvgFill(ctx);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f, std::max(radius - 0.5f, 0.f));
    nvgStrokeWidth(ctx, 1.f);
    nvgStrokeColor(ctx, to_nvg(border));
    nvgStroke(ctx);

    const Color text_color = m_enabled
                                 ? fluent->color(FluentWebTheme::ColorToken::colorNeutralForeground1)
                                 : fluent->color(FluentWebTheme::ColorToken::colorNeutralForegroundDisabled);

    const float center_y = y + h * 0.5f;
    const float text_x = x + padding;

    nvgFontFace(ctx, "sans");
    nvgFontSize(ctx, static_cast<float>(m_font_size));
    nvgFillColor(ctx, to_nvg(text_color));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    const std::string value = std::string(caption());
    nvgText(ctx, text_x, center_y, value.c_str(), nullptr);

    const float arrow_x = x + w - padding * 1.5f;
    const float arrow_size = 4.5f;
    const Color arrow_color = m_enabled
                                  ? fluent->color(FluentWebTheme::ColorToken::colorNeutralForeground2)
                                  : fluent->color(FluentWebTheme::ColorToken::colorNeutralForegroundDisabled);

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, arrow_x - arrow_size, center_y - arrow_size * 0.6f);
    nvgLineTo(ctx, arrow_x, center_y + arrow_size * 0.6f);
    nvgLineTo(ctx, arrow_x + arrow_size, center_y - arrow_size * 0.6f);
    nvgFillColor(ctx, to_nvg(arrow_color));
    nvgFill(ctx);
}

NAMESPACE_END(nanogui)
