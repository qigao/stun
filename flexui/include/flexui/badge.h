#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <string>

namespace flexui {

struct BadgeStyle {
    float padding = 6;
    float borderRadius = 12;
    NVGcolor bgColor = nvgRGB(244, 67, 54);
    NVGcolor textColor = nvgRGB(255, 255, 255);
    float fontSize = 11;
};

class Badge : public Widget {
public:
    Badge(cssboxRenderer* renderer, const std::string& id, const std::string& text,
          const BadgeStyle& style = BadgeStyle());

    void draw(NVGcontext* vg) override;
    void setBadgeText(const std::string& text) { text_ = text; }
    const std::string& getBadgeText() const { return text_; }
    void setBadgeStyle(const BadgeStyle& style) { style_ = style; }

private:
    std::string text_;
    BadgeStyle style_;
};

} // namespace flexui
