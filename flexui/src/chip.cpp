#include <flexui/chip.h>
#include <nanovg_css_internal.h>
namespace flexui {

Chip::Chip(NVGCSSRenderer* renderer, const std::string& id, const std::string& text,
           const ChipStyle& style)
    : Widget(renderer, id, "chip"), text_(text), style_(style) {

    if (style_.closeable) {
        setClickCallback([this](Widget* w) {
            if (closeCallback_) {
                closeCallback_();
            }
            return true;
        });
    }
}

void Chip::draw(NVGcontext* vg) {
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

    // Draw chip background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, borderRadius);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);

    // Draw text
    nvgFontSize(vg, fontSize);
    nvgFontFace(vg, "sans-serif");
    nvgFillColor(vg, textColor);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + style_.padding, y + h / 2, text_.c_str(), nullptr);

    // Draw close icon if closeable
    if (style_.closeable) {
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
