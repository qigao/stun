#include <flexui/label.h>
#include <nanovg_css_internal.h>

namespace flexui {

Label::Label(NVGCSSRenderer* renderer, const std::string& id, const std::string& text,
             const LabelStyle& style)
    : Widget(renderer, id, "label"), text_(text), style_(style) {
}

void Label::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float h = el->computed.height;

    float fontSize = cssFontSize(style_.fontSize);
    NVGcolor textColor = cssColor(style_.textColor);

    nvgFontSize(vg, fontSize);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, style_.textAlign);
    nvgFillColor(vg, textColor);

    // Center vertically in the element's height
    float textY = y + h / 2;
    nvgText(vg, x + 5, textY, text_.c_str(), nullptr);
}

} // namespace flexui
