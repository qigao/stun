#include <flexui/spinner.h>
#include <cmath>
#include <cssbox_internal.h>
namespace flexui {

Spinner::Spinner(cssboxRenderer* renderer, const std::string& id, const SpinnerStyle& style)
    : Widget(renderer, id, "spinner"), style_(style) {
    start_time_ = std::chrono::steady_clock::now();
}

void Spinner::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->layout.x;
    float y = el->layout.y;
    float w = el->layout.width;
    float h = el->layout.height;

    if (w == 0 || h == 0) return;

    NVGcolor color = cssColor(style_.color);

    // Calculate elapsed time for animation
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - start_time_).count();
    float angle = elapsed * style_.speed;

    float cx = x + w / 2;
    float cy = y + h / 2;
    float radius = style_.size / 2;

    nvgSave(vg);
    nvgTranslate(vg, cx, cy);
    nvgRotate(vg, angle);

    // Draw spinning arc
    nvgBeginPath(vg);
    nvgArc(vg, 0, 0, radius, 0, NVG_PI * 1.5f, NVG_CW);
    nvgStrokeColor(vg, color);
    nvgStrokeWidth(vg, style_.thickness);
    nvgStroke(vg);

    nvgRestore(vg);

    // Request next frame repaint for continuous animation
    cssboxInvalidatePaint(renderer());
}

} // namespace flexui
