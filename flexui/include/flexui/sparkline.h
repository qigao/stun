#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <vector>

namespace flexui {

struct SparklineStyle {
    NVGcolor lineColor = nvgRGB(59, 130, 246);
    NVGcolor fillColor = nvgRGBA(59, 130, 246, 30);
    NVGcolor pointColor = nvgRGB(59, 130, 246);
    float lineWidth = 1.5f;
    bool showFill = true;
    bool showEndPoint = true;
    bool smooth = true;
};

class Sparkline : public Widget {
public:
    Sparkline(cssboxRenderer* renderer, const std::string& id,
              const std::vector<float>& values = {},
              const SparklineStyle& style = SparklineStyle());

    void draw(NVGcontext* vg) override;

    void setValues(const std::vector<float>& values);
    void addValue(float value);
    void clearValues();
    
    void setStyle(const SparklineStyle& style) { style_ = style; }
    void setMaxPoints(size_t max) { maxPoints_ = max; }

private:
    std::vector<float> values_;
    SparklineStyle style_;
    size_t maxPoints_ = 50;
    float minValue_ = 0.0f;
    float maxValue_ = 0.0f;
    
    void updateRange();
};

} // namespace flexui
