#pragma once

#include <nanogui/combobox.h>
#include <vector>
#include <string>
#include <nanogui/fluent_web_theme.h>

NAMESPACE_BEGIN(nanogui)

class FluentWebComboBox : public ComboBox {
public:
    FluentWebComboBox(Widget *parent);
    FluentWebComboBox(Widget *parent, const std::vector<std::string> &items);
    FluentWebComboBox(Widget *parent, const std::vector<std::string> &items, const std::vector<std::string> &items_short);

    void set_theme(Theme *theme) override;
    void draw(NVGcontext *ctx) override;

private:
    const FluentWebTheme *fluent_theme() const;
};

NAMESPACE_END(nanogui)
