#pragma once

#include <nanogui/button.h>
#include <nanogui/fluent_web_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * Fluent 2 styled radio button.
 *
 * Renders the Fluent dot-in-circle radio visuals while leveraging Button's
 * built-in radio/toggle behaviour.
 */
class NANOGUI_EXPORT FluentWebRadio : public Button {
public:
    FluentWebRadio(Widget *parent,
                   const std::string &caption = "Option",
                   bool selected = false);

    void set_theme(Theme *theme) override;
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

private:
    void update_metrics();

    float m_outer_radius = 9.f;
    float m_inner_radius = 5.f;
    int m_font_override = -1;
};

NAMESPACE_END(nanogui)

