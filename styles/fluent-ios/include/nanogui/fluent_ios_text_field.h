#pragma once

#include <nanogui/textbox.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT FluentIOSTextField : public TextBox {
public:
    FluentIOSTextField(Widget *parent, const std::string &value = std::string());

    void set_placeholder(const std::string &placeholder);
    const std::string &placeholder() const { return m_placeholder; }

protected:
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

private:
    std::string m_placeholder;
};

NAMESPACE_END(nanogui)
