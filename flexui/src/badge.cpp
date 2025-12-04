#include <flexui/badge.h>
#include <cssbox_internal.h>
namespace flexui {

Badge::Badge(cssboxRenderer* renderer, const std::string& id, const std::string& text,
             const BadgeStyle& style)
    : Widget(renderer, id, "badge"), text_(text), style_(style) {
}

void Badge::draw(NVGcontext* vg) {
    if (text_.empty()) return;

    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w == 0 || h == 0) return;

    NVGcolor bgColor = cssBackground(style_.bgColor);
    float borderRadius = cssBorderRadius(style_.borderRadius);
    float fontSize = cssFontSize(style_.fontSize);
    NVGcolor textColor = cssColor(style_.textColor);

    // Draw badge background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, borderRadius);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);

    // Draw badge text
    nvgFontSize(vg, fontSize);
    nvgFontFace(vg, "sans-serif");
    nvgFillColor(vg, textColor);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + w / 2, y + h / 2, text_.c_str(), nullptr);
}

} // namespace flexui
