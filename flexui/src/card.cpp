#include <flexui/card.h>
#include <nanovg_css_internal.h>
namespace flexui {

Card::Card(NVGCSSRenderer* renderer, const std::string& id, const CardStyle& style)
    : Widget(renderer, id, "card"), style_(style) {
}

void Card::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w == 0 || h == 0) return;

    NVGcolor bgColor = cssBackground(style_.bgColor);
    float borderRadius = cssBorderRadius(style_.borderRadius);

    nvgSave(vg);

    // Shadow
    NVGpaint shadowPaint = nvgBoxGradient(vg, x, y + 2, w, h,
                                          borderRadius, style_.shadowBlur,
                                          style_.shadowColor, nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(vg);
    nvgRect(vg, x - style_.shadowBlur, y - style_.shadowBlur,
            w + style_.shadowBlur * 2, h + style_.shadowBlur * 2);
    nvgRoundedRect(vg, x, y, w, h, borderRadius);
    nvgPathWinding(vg, NVG_HOLE);
    nvgFillPaint(vg, shadowPaint);
    nvgFill(vg);

    // Card background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, borderRadius);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);

    nvgRestore(vg);
}

} // namespace flexui
