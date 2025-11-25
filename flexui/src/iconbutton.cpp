#include <flexui/iconbutton.h>
#include <nanovg_css_internal.h>

namespace flexui {

IconButton::IconButton(NVGCSSRenderer* renderer, const std::string& id,
                       const std::string& icon, const IconButtonStyle& style)
    : Widget(renderer, id, "iconbutton"), icon_(icon), style_(style) {
}

void IconButton::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w == 0 || h == 0) return;

    NVGcolor bgColor = pressed_ ? style_.bgColorPressed :
                       (hovered_ ? style_.bgColorHover : style_.bgColor);

    float cx = x + w / 2;
    float cy = y + h / 2;
    float radius = style_.size / 2;

    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, radius);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);

    nvgFontSize(vg, style_.iconSize);
    nvgFontFace(vg, "icons");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, style_.iconColor);
    nvgText(vg, cx, cy, icon_.c_str(), nullptr);
}

bool IconButton::handleMouseMove(float mx, float my) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    float cx = x + w / 2;
    float cy = y + h / 2;
    float dx = mx - cx;
    float dy = my - cy;
    float dist = dx * dx + dy * dy;
    float radius = style_.size / 2;

    hovered_ = (dist <= radius * radius);
    return false;
}

bool IconButton::handleMouseDown(float mx, float my) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    float cx = x + w / 2;
    float cy = y + h / 2;
    float dx = mx - cx;
    float dy = my - cy;
    float dist = dx * dx + dy * dy;
    float radius = style_.size / 2;

    if (dist <= radius * radius) {
        pressed_ = true;
        return true;
    }
    return false;
}

bool IconButton::handleMouseUp(float mx, float my) {
    pressed_ = false;
    return false;
}

} // namespace flexui
