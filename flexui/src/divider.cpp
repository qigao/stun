#include <flexui/divider.h>
#include <cssbox_internal.h>

namespace flexui {

Divider::Divider(cssboxRenderer* renderer, const std::string& id,
                 bool vertical, const DividerStyle& style)
    : Widget(renderer, id, "hr"), style_(style) {
    style_.vertical = vertical;
}

void Divider::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    NVGcolor color = cssColor(style_.color);

    nvgBeginPath(vg);

    if (style_.vertical) {
        // Vertical line in the center
        float cx = x + w / 2;
        nvgMoveTo(vg, cx, y);
        nvgLineTo(vg, cx, y + h);
    } else {
        // Horizontal line in the center
        float cy = y + h / 2;
        nvgMoveTo(vg, x, cy);
        nvgLineTo(vg, x + w, cy);
    }

    nvgStrokeColor(vg, color);
    nvgStrokeWidth(vg, style_.thickness);
    nvgStroke(vg);
}

} // namespace flexui
