#include <flexui/badge.h>
#include <nanovg_css_internal.h>
namespace flexui {

Badge::Badge(NVGCSSRenderer* renderer, const std::string& id, const std::string& text,
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

    // Draw badge background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, style_.borderRadius);
    nvgFillColor(vg, style_.bgColor);
    nvgFill(vg);

    // Draw badge text
    nvgFontSize(vg, style_.fontSize);
    nvgFontFace(vg, "sans-serif");
    nvgFillColor(vg, style_.textColor);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + w / 2, y + h / 2, text_.c_str(), nullptr);
}

} // namespace flexui
