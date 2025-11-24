#pragma once

#include <nanogui/button.h>
#include <nanogui/fluent_web_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * Fluent 2 chip / pill control.
 *
 * Works as a small button styled with Fluent web tokens. Supports optional
 * filter behaviour (toggleable selection).
 */
class NANOGUI_EXPORT FluentWebChip : public Button {
public:
    enum class Kind {
        Assist,
        Suggestion,
        Filter
    };

    FluentWebChip(Widget *parent,
                  const std::string &caption = "Chip",
                  Kind kind = Kind::Assist);

    void set_kind(Kind kind);
    Kind kind() const { return m_kind; }

    void set_theme(Theme *theme) override;
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

private:
    void update_metrics();

    Kind m_kind;
};

NAMESPACE_END(nanogui)
