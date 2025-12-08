#include <flexui/chip.h>
#include <cssbox_internal.h>
namespace flexui {

Chip::Chip(cssboxRenderer* renderer, const std::string& id, const std::string& text,
           const ChipStyle& style)
    : Widget(renderer, id, "chip"), text_(text), style_(style) {
    // Set text_content for layout engine to calculate intrinsic size
    element()->text_content = text;
}

bool Chip::onClicked() {
    if (style_.closeable && closeCallback_) {
        closeCallback_();
    }
    return Widget::onClicked();
}

void Chip::draw(NVGcontext* vg) {
    // REMOVED: cssbox renders background + text_content
    // Only keep close icon rendering for closeable chips
    if (style_.closeable) {
        auto* el = element();
        float x = el->layout.x;
        float y = el->layout.y;
        float w = el->layout.width;
        float h = el->layout.height;
        NVGcolor textColor = cssColor(style_.textColor);
        
        float cx = x + w - 16;
        float cy = y + h / 2;
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx - 4, cy - 4);
        nvgLineTo(vg, cx + 4, cy + 4);
        nvgMoveTo(vg, cx + 4, cy - 4);
        nvgLineTo(vg, cx - 4, cy + 4);
        nvgStrokeColor(vg, textColor);
        nvgStrokeWidth(vg, 1.5f);
        nvgStroke(vg);
    }
}

} // namespace flexui
