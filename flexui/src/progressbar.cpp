#include <flexui/progressbar.h>
#include <nanovg_css_internal.h>
#include <algorithm>

namespace flexui {

ProgressBar::ProgressBar(NVGCSSRenderer* renderer, const std::string& id, float progress,
                         const ProgressBarStyle& style)
    : Widget(renderer, id, "progressbar"), progress_(std::clamp(progress, 0.0f, 1.0f)), style_(style) {
}

void ProgressBar::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    // Background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, style_.borderRadius);
    nvgFillColor(vg, style_.bgColor);
    nvgFill(vg);

    // Fill (progress)
    float fillWidth = w * progress_;
    if (fillWidth > 0) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, fillWidth, h, style_.borderRadius);
        nvgFillColor(vg, style_.fillColor);
        nvgFill(vg);
    }

    // Percentage text
    if (style_.showPercentage) {
        char text[16];
        snprintf(text, sizeof(text), "%.0f%%", progress_ * 100.0f);

        nvgFontSize(vg, style_.fontSize);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, style_.textColor);
        nvgText(vg, x + w / 2, y + h / 2, text, nullptr);
    }
}

void ProgressBar::setProgress(float progress) {
    progress_ = std::clamp(progress, 0.0f, 1.0f);
}

} // namespace flexui
