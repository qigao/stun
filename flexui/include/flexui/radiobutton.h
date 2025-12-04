#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <string>

namespace flexui {

struct RadioButtonStyle {
    NVGcolor bgColor = nvgRGB(255, 255, 255);
    NVGcolor borderColor = nvgRGB(204, 204, 204);
    NVGcolor borderColorChecked = nvgRGB(33, 150, 243);
    NVGcolor dotColor = nvgRGB(33, 150, 243);
    float borderWidth = 2;
    float dotRadiusRatio = 0.5f;  // Ratio of dot to outer circle (0.5 = 50%)
};

class RadioButton : public Widget {
public:
    RadioButton(cssboxRenderer* renderer, const std::string& id,
                const std::string& group, const std::string& value,
                bool checked = false, const RadioButtonStyle& style = RadioButtonStyle());

    void draw(NVGcontext* vg) override;

    void setChecked(bool checked);
    bool isChecked() const { return checked_; }

    const std::string& getGroup() const { return group_; }
    const std::string& getValue() const { return value_; }

    void setRadioButtonStyle(const RadioButtonStyle& style) { style_ = style; }

    // Set Screen pointer for group management
    void setScreen(class Screen* screen) { screen_ = screen; }

protected:
    bool onClicked() override;

private:
    std::string group_;   // Radio group name (like HTML name attribute)
    std::string value_;   // Value when selected
    bool checked_;
    RadioButtonStyle style_;
    class Screen* screen_ = nullptr;
};

} // namespace flexui
