#include <flexui/switch.h>
#include <nanovg_css.h>
#include <nanovg_css_internal.h>
namespace flexui {

Switch::Switch(NVGCSSRenderer* renderer, const std::string& id, bool initialState,
               const SwitchStyle& style)
    : Widget(renderer, id, "switch"), on_(initialState), style_(style) {

    setClickCallback([this](Widget* w) {
        on_ = !on_;
        if (change_callback_) {
            change_callback_(on_);
        }
        return true;
    });
}

void Switch::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w == 0 || h == 0) return;

    float cx = x + w / 2;
    float cy = y + h / 2;
    float trackX = cx - style_.trackWidth / 2;
    float trackY = cy - style_.trackHeight / 2;

    // Draw track
    NVGcolor trackColor = on_ ? style_.trackColorOn : style_.trackColorOff;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, trackX, trackY, style_.trackWidth, style_.trackHeight, style_.borderRadius);
    nvgFillColor(vg, trackColor);
    nvgFill(vg);

    // Draw thumb
    float thumbX = on_ ? trackX + style_.trackWidth - style_.thumbSize - 2 : trackX + 2;
    float thumbY = trackY + (style_.trackHeight - style_.thumbSize) / 2;

    // Thumb shadow
    NVGpaint shadowPaint = nvgBoxGradient(vg, thumbX, thumbY + 1, style_.thumbSize, style_.thumbSize,
                                           style_.thumbSize / 2, style_.thumbShadowBlur,
                                           nvgRGBA(0, 0, 0, 64), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(vg);
    nvgRect(vg, thumbX - 5, thumbY - 5, style_.thumbSize + 10, style_.thumbSize + 10);
    nvgCircle(vg, thumbX + style_.thumbSize / 2, thumbY + style_.thumbSize / 2, style_.thumbSize / 2);
    nvgPathWinding(vg, NVG_HOLE);
    nvgFillPaint(vg, shadowPaint);
    nvgFill(vg);

    // Thumb
    nvgBeginPath(vg);
    nvgCircle(vg, thumbX + style_.thumbSize / 2, thumbY + style_.thumbSize / 2, style_.thumbSize / 2);
    nvgFillColor(vg, style_.thumbColor);
    nvgFill(vg);
}

bool Switch::handleSwitchClick(float mx, float my) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

} // namespace flexui
