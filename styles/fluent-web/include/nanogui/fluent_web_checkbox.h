#pragma once

#include <nanogui/checkbox.h>
#include <nanogui/fluent_web_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * Fluent 2 styled checkbox.
 *
 * Reuses NanoGUI's CheckBox behaviour while rendering with Fluent web design
 * tokens for colors, spacing, focus, and typography.
 */
class NANOGUI_EXPORT FluentWebCheckbox : public CheckBox {
public:
    FluentWebCheckbox(Widget *parent,
                      const std::string &caption = "Option",
                      const std::function<void(bool)> &callback = {});

    void set_theme(Theme *theme) override;
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

private:
    void update_metrics();

    float m_box_size = 16.f;
    float m_label_spacing = 8.f;
    int m_typography_size = -1;
};

NAMESPACE_END(nanogui)

