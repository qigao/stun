#pragma once

#include <flexui/widget.h>
#include <nanovg.h>

namespace flexui {

struct DividerStyle {
    NVGcolor color = nvgRGB(224, 224, 224);
    float thickness = 1;
    bool vertical = false;  // false = horizontal, true = vertical
};

class Divider : public Widget {
public:
    Divider(cssboxRenderer* renderer, const std::string& id,
            bool vertical = false, const DividerStyle& style = DividerStyle());

    void draw(NVGcontext* vg) override;

    void setDividerStyle(const DividerStyle& style) { style_ = style; }

private:
    DividerStyle style_;
};

} // namespace flexui
