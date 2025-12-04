#include <flexui/progressbar.h>
#include <cssbox_internal.h>
#include <algorithm>

namespace flexui {

ProgressBar::ProgressBar(cssboxRenderer* renderer, const std::string& id, float progress,
                         const ProgressBarStyle& style)
    : Widget(renderer, id, "progressbar"), progress_(std::clamp(progress, 0.0f, 1.0f)), style_(style) {
}

void ProgressBar::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    NVGcolor bgColor = cssBackground(style_.bgColor);
    float borderRadius = cssBorderRadius(style_.borderRadius);

    // Background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, borderRadius);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);

    // Fill (progress)
    NVGcolor fillColor = cssColor(style_.fillColor);
    float fillWidth = w * progress_;
    if (fillWidth > 0) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, fillWidth, h, borderRadius);
        nvgFillColor(vg, fillColor);
        nvgFill(vg);
    }

    // Percentage text
    if (style_.showPercentage) {
        char text[16];
        snprintf(text, sizeof(text), "%.0f%%", progress_ * 100.0f);
        float fontSize = cssFontSize(style_.fontSize);

        nvgFontSize(vg, fontSize);
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
