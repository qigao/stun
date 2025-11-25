#pragma once

#include <nanovg.h>
#include <string>

namespace flexui {

struct TooltipStyle {
    float padding = 8;
    float borderRadius = 4;
    NVGcolor bgColor = nvgRGBA(50, 50, 50, 230);
    NVGcolor textColor = nvgRGB(255, 255, 255);
    float fontSize = 12;
    float offsetX = 10;
    float offsetY = 10;
};

class Tooltip {
public:
    Tooltip(const std::string& text, const TooltipStyle& style = TooltipStyle());
    
    void draw(NVGcontext* vg, float mx, float my);
    void setText(const std::string& text) { text_ = text; }
    void setStyle(const TooltipStyle& style) { style_ = style; }

private:
    std::string text_;
    TooltipStyle style_;
};

} // namespace flexui
