#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <functional>
#include <string>

namespace flexui {

struct IconButtonStyle {
    float size = 40;
    float borderRadius = 20;
    NVGcolor bgColor = nvgRGBA(0, 0, 0, 0);
    NVGcolor bgColorHover = nvgRGBA(0, 0, 0, 10);
    NVGcolor bgColorPressed = nvgRGBA(0, 0, 0, 20);
    NVGcolor iconColor = nvgRGB(100, 100, 100);
    float iconSize = 20;
};

class IconButton : public Widget {
public:
    using ClickCallback = std::function<void()>;

    IconButton(NVGCSSRenderer* renderer, const std::string& id,
               const std::string& icon, const IconButtonStyle& style = IconButtonStyle());

    void draw(NVGcontext* vg) override;
    bool handleMouseDown(float mx, float my) override;
    bool handleMouseMove(float mx, float my) override;
    bool handleMouseUp(float mx, float my) override;

    void setIcon(const std::string& icon) { icon_ = icon; }
    void setStyle(const IconButtonStyle& style) { style_ = style; }

private:
    std::string icon_;
    IconButtonStyle style_;
    bool hovered_ = false;
    bool pressed_ = false;
};

} // namespace flexui
