#include <flexui/alert.h>
#include <nanovg_css_internal.h>
namespace flexui {

Alert::Alert(NVGCSSRenderer* renderer, const std::string& id, const std::string& message,
             AlertType type)
    : Widget(renderer, id, "alert"), message_(message), type_(type) {
    updateStyleForType();
}

void Alert::updateStyleForType() {
    switch (type_) {
        case AlertType::Success:
            style_.bgColor = nvgRGB(237, 247, 237);
            style_.textColor = nvgRGB(30, 70, 32);
            style_.borderColor = nvgRGB(129, 199, 132);
            break;
        case AlertType::Warning:
            style_.bgColor = nvgRGB(255, 244, 229);
            style_.textColor = nvgRGB(102, 60, 0);
            style_.borderColor = nvgRGB(255, 183, 77);
            break;
        case AlertType::Error:
            style_.bgColor = nvgRGB(253, 237, 237);
            style_.textColor = nvgRGB(95, 33, 32);
            style_.borderColor = nvgRGB(239, 83, 80);
            break;
        default: // Info
            style_.bgColor = nvgRGB(229, 246, 253);
            style_.textColor = nvgRGB(1, 67, 97);
            style_.borderColor = nvgRGB(144, 202, 249);
            break;
    }
}

void Alert::setType(AlertType type) {
    type_ = type;
    updateStyleForType();
}

void Alert::draw(NVGcontext* vg) {
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

    // Draw background with border
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, borderRadius);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);
    nvgStrokeColor(vg, style_.borderColor);
    nvgStrokeWidth(vg, style_.borderWidth);
    nvgStroke(vg);

    // Draw message text
    nvgFontSize(vg, fontSize);
    nvgFontFace(vg, "sans-serif");
    nvgFillColor(vg, textColor);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgTextBox(vg, x + style_.padding, y + style_.padding,
               w - style_.padding * 2, message_.c_str(), nullptr);
}

} // namespace flexui
