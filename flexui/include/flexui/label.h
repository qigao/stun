#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <string>

namespace flexui {

struct LabelStyle {
    NVGcolor textColor = nvgRGB(51, 51, 51);
    float fontSize = 14;
    int textAlign = NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE;
};

class Label : public Widget {
public:
    Label(cssboxRenderer* renderer, const std::string& id, const std::string& text,
          const LabelStyle& style = LabelStyle());

    void draw(NVGcontext* vg) override;

    void setLabelText(const std::string& text);
    std::string getLabelText() const { return text_; }

    void setLabelStyle(const LabelStyle& style) { style_ = style; }

private:
    std::string text_;
    LabelStyle style_;
};

} // namespace flexui
