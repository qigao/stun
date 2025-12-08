#include <flexui/switch.h>
#include <cssbox.h>
#include <cssbox_internal.h>
namespace flexui {

Switch::Switch(cssboxRenderer* renderer, const std::string& id, bool initialState,
               const SwitchStyle& style)
    : Widget(renderer, id, "input"), on_(initialState), style_(style) {

    if (on_) {
        addClass("checked");
    }
    setSize(40, 20); // Default size
}

void Switch::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->layout.x;
    float y = el->layout.y;
    float w = el->layout.width;
    float h = el->layout.height;

    if (w == 0 || h == 0) return;

    float cx = x + w / 2;
    float cy = y + h / 2;

    // Use computed width/height as track dimensions
    float trackWidth = w > 0 ? w : style_.trackWidth;
    float trackHeight = h > 0 ? h : style_.trackHeight;

    float trackX = cx - trackWidth / 2;
    float trackY = cy - trackHeight / 2;

    float borderRadius = cssBorderRadius(style_.borderRadius);
    NVGcolor fallbackTrack = on_ ? style_.trackColorOn : style_.trackColorOff;
    NVGcolor trackColor = cssBackground(fallbackTrack);

    // Draw track
    nvgBeginPath(vg);
    nvgRoundedRect(vg, trackX, trackY, trackWidth, trackHeight, borderRadius);
    nvgFillColor(vg, trackColor);
    nvgFill(vg);

    // Draw thumb
    float thumbSize = trackHeight > 0 ? trackHeight - 4 : style_.thumbSize;
    NVGcolor thumbColor = cssColor(style_.thumbColor);

    float thumbX = on_ ? trackX + trackWidth - thumbSize - 2 : trackX + 2;
    float thumbY = trackY + (trackHeight - thumbSize) / 2;

    // Thumb shadow
    NVGpaint shadowPaint = nvgBoxGradient(vg, thumbX, thumbY + 1, thumbSize, thumbSize,
                                           thumbSize / 2, style_.thumbShadowBlur,
                                           nvgRGBA(0, 0, 0, 64), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(vg);
    nvgRect(vg, thumbX - 5, thumbY - 5, thumbSize + 10, thumbSize + 10);
    nvgCircle(vg, thumbX + thumbSize / 2, thumbY + thumbSize / 2, thumbSize / 2);
    nvgPathWinding(vg, NVG_HOLE);
    nvgFillPaint(vg, shadowPaint);
    nvgFill(vg);

    // Thumb
    nvgBeginPath(vg);
    nvgCircle(vg, thumbX + thumbSize / 2, thumbY + thumbSize / 2, thumbSize / 2);
    nvgFillColor(vg, thumbColor);
    nvgFill(vg);
}

bool Switch::onClicked() {
    // Toggle switch state
    on_ = !on_;
    if (on_) addClass("checked"); else removeClass("checked");

    // Trigger change callback
    if (change_callback_) change_callback_(on_);

    // Call base to trigger user click callback
    return Widget::onClicked();
}

} // namespace flexui
