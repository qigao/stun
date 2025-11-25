#include <flexui/panel.h>

namespace flexui {

Panel::Panel(float x, float y, const PanelStyle& style)
    : x_(x), y_(y), style_(style) {
}

void Panel::draw(NVGcontext* vg) {
    // Shadow
    NVGpaint shadowPaint = nvgBoxGradient(vg, x_, y_ + 2, style_.width, style_.height,
                                          style_.borderRadius, style_.shadowBlur, 
                                          style_.shadowColor, nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(vg);
    nvgRect(vg, x_ - 10, y_ - 10, style_.width + 20, style_.height + 30);
    nvgRoundedRect(vg, x_, y_, style_.width, style_.height, style_.borderRadius);
    nvgPathWinding(vg, NVG_HOLE);
    nvgFillPaint(vg, shadowPaint);
    nvgFill(vg);
    
    // Background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x_, y_, style_.width, style_.height, style_.borderRadius);
    nvgFillColor(vg, style_.bgColor);
    nvgFill(vg);
    
    // Border
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x_, y_, style_.width, style_.height, style_.borderRadius);
    nvgStrokeColor(vg, style_.borderColor);
    nvgStrokeWidth(vg, style_.borderWidth);
    nvgStroke(vg);
}

} // namespace flexui
