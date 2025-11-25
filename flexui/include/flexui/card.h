#pragma once

#include <flexui/widget.h>
#include <nanovg.h>

namespace flexui {

struct CardStyle {
    float borderRadius = 8;
    float padding = 16;
    NVGcolor bgColor = nvgRGB(255, 255, 255);
    NVGcolor shadowColor = nvgRGBA(0, 0, 0, 50);
    float shadowBlur = 10;
};

class Card : public Widget {
public:
    Card(NVGCSSRenderer* renderer, const std::string& id,
         const CardStyle& style = CardStyle());

    void draw(NVGcontext* vg) override;
    void setCardStyle(const CardStyle& style) { style_ = style; }

private:
    CardStyle style_;
};

} // namespace flexui
