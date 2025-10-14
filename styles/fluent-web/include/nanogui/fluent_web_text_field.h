#pragma once

#include <nanogui/textbox.h>
#include <nanogui/fluent_web_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * Fluent 2 styled text field.
 *
 * Extends TextBox to apply Fluent web colors, border, focus ring, and
 * typography tokens.
 */
class NANOGUI_EXPORT FluentWebTextField : public TextBox {
public:
    FluentWebTextField(Widget *parent,
                       const std::string &value = "",
                       const std::string &placeholder = "Placeholder");

    void set_theme(Theme *theme) override;
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

private:
    void update_metrics();
};

NAMESPACE_END(nanogui)

