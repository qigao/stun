#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <functional>
#include <string>

namespace flexui {

struct ButtonStyle {
    float borderRadius = 8;
    NVGcolor bgColor = nvgRGB(33, 150, 243);
    NVGcolor bgColorHover = nvgRGB(66, 165, 245);
    NVGcolor bgColorPressed = nvgRGB(21, 101, 192);
    NVGcolor textColor = nvgRGB(255, 255, 255);
    float fontSize = 18;
};

class Button : public Widget {
public:
    Button(cssboxRenderer* renderer, const std::string& id, const std::string& text,
           const ButtonStyle& style = ButtonStyle());

    void draw(NVGcontext* vg) override;

    void setButtonText(const std::string& text);
    std::string getButtonText() const { return text_; }

    void setButtonStyle(const ButtonStyle& style) { style_ = style; }

private:
    std::string text_;
    ButtonStyle style_;
    bool pressed_ = false;
};

} // namespace flexui
