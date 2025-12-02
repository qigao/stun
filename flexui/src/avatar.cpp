#include <flexui/avatar.h>
#include <nanovg_css_internal.h>
namespace flexui {

Avatar::Avatar(NVGCSSRenderer* renderer, const std::string& id, const std::string& initials,
               const AvatarStyle& style)
    : Widget(renderer, id, "avatar"), initials_(initials), style_(style) {
}

void Avatar::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w == 0 || h == 0) return;

    NVGcolor bgColor = cssBackground(style_.bgColor);
    float fontSize = cssFontSize(style_.fontSize);
    NVGcolor textColor = cssColor(style_.textColor);

    float cx = x + w / 2;
    float cy = y + h / 2;
    float radius = (w < h ? w : h) / 2;

    // Draw circle background
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, radius);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);

    // Draw initials
    nvgFontSize(vg, fontSize);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, textColor);
    nvgText(vg, cx, cy, initials_.c_str(), nullptr);
}

} // namespace flexui
