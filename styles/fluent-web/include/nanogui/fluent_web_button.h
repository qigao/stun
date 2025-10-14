#pragma once

#include <nanogui/button.h>
#include <nanogui/fluent_web_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * Fluent 2 style button.
 *
 * Provides Fluent web appearances (primary, secondary, subtle, outline,
 * transparent, danger) and consumes FluentWebTheme design tokens.
 */
class NANOGUI_EXPORT FluentWebButton : public Button {
public:
    enum class Appearance {
        Primary,
        Secondary,
        Outline,
        Subtle,
        Transparent,
        Danger
    };

    FluentWebButton(Widget *parent,
                    const std::string &caption = "Button",
                    Appearance appearance = Appearance::Primary);

    void set_appearance(Appearance appearance);
    Appearance appearance() const { return m_appearance; }

    void set_compact(bool compact);
    bool compact() const { return m_compact; }

    void draw(NVGcontext *ctx) override;
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void set_theme(Theme *theme) override;

private:
    void update_metrics();

    Appearance m_appearance;
    bool m_compact;
};

NAMESPACE_END(nanogui)

