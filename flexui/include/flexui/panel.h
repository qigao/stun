#pragma once

#include <nanovg.h>

namespace flexui {

// DEPRECATED: Use Card instead
// Panel does not inherit from Widget and cannot use CSS styling.
// Card provides the same functionality with full CSS support.
// 
// Migration:
//   Panel panel(x, y, style);
//   panel.draw(vg);
// 
// Replace with:
//   <card id="my-card" style="background: #fff; border-radius: 8px;"/>

struct PanelStyle {
    float width = 300;
    float height = 200;
    float borderRadius = 8;
    NVGcolor bgColor = nvgRGBA(255, 255, 255, 230);
    NVGcolor borderColor = nvgRGB(224, 224, 224);
    float borderWidth = 1;
    float shadowBlur = 10;
    NVGcolor shadowColor = nvgRGBA(0, 0, 0, 30);
};

class Panel {
public:
    Panel(float x, float y, const PanelStyle& style = PanelStyle());
    
    void draw(NVGcontext* vg);
    
    void setStyle(const PanelStyle& style) { style_ = style; }
    
    float getX() const { return x_; }
    float getY() const { return y_; }
    float getWidth() const { return style_.width; }
    float getHeight() const { return style_.height; }

private:
    float x_, y_;
    PanelStyle style_;
};

} // namespace flexui
