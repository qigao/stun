#include <flexui/tooltip.h>

namespace flexui {

Tooltip::Tooltip(const std::string& text, const TooltipStyle& style)
    : text_(text), style_(style) {
}

void Tooltip::draw(NVGcontext* vg, float mx, float my) {
    if (text_.empty()) return;
    
    nvgFontSize(vg, style_.fontSize);
    nvgFontFace(vg, "sans-serif");
    
    float bounds[4];
    nvgTextBounds(vg, 0, 0, text_.c_str(), nullptr, bounds);
    float tw = bounds[2] - bounds[0];
    float th = bounds[3] - bounds[1];
    
    float x = mx + style_.offsetX;
    float y = my + style_.offsetY;
    float w = tw + style_.padding * 2;
    float h = th + style_.padding * 2;
    
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, style_.borderRadius);
    nvgFillColor(vg, style_.bgColor);
    nvgFill(vg);
    
    nvgFillColor(vg, style_.textColor);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgText(vg, x + style_.padding, y + style_.padding, text_.c_str(), nullptr);
}

} // namespace flexui
