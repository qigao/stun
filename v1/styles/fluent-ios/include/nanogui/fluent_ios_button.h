#pragma once

#include <nanogui/button.h>
#include <nanogui/fluent_ios_theme.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT FluentIOSButton : public Button {
public:
    enum class Style {
        Filled,
        Gray,
        Outline
    };

    FluentIOSButton(Widget *parent,
                    const std::string &title,
                    Style style = Style::Filled,
                    bool destructive = false);

    void set_style(Style style) { m_style = style; }
    Style style() const { return m_style; }

    void set_destructive(bool destructive) { m_destructive = destructive; }
    bool destructive() const { return m_destructive; }

    void set_font_style(FluentIOSTheme::TypographyStyle style) { m_typography = style; }
    FluentIOSTheme::TypographyStyle font_style() const { return m_typography; }

protected:
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

    float horizontal_padding() const { return 20.f; }
    float vertical_padding() const { return 12.f; }

private:
    FluentIOSTheme::TypographyStyle m_typography = FluentIOSTheme::TypographyStyle::Body;
    Style m_style;
    bool m_destructive;
};

NAMESPACE_END(nanogui)
