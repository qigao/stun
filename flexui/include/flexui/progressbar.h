#pragma once

#include <flexui/widget.h>
#include <nanovg.h>

namespace flexui {

struct ProgressBarStyle {
    float borderRadius = 4;
    NVGcolor bgColor = nvgRGB(224, 224, 224);
    NVGcolor fillColor = nvgRGB(76, 175, 80);
    NVGcolor textColor = nvgRGB(255, 255, 255);
    float fontSize = 12;
    bool showPercentage = true;
};

class ProgressBar : public Widget {
public:
    ProgressBar(cssboxRenderer* renderer, const std::string& id, float progress = 0.0f,
                const ProgressBarStyle& style = ProgressBarStyle());

    void draw(NVGcontext* vg) override;

    void setProgress(float progress);
    float getProgress() const { return progress_; }

    void setProgressBarStyle(const ProgressBarStyle& style) { style_ = style; }

private:
    float progress_;  // 0.0 to 1.0
    ProgressBarStyle style_;
};

} // namespace flexui
