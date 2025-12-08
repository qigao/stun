#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderErrorBarSeries(NVGcontext* vg, const SeriesData& series,
                          float x, float y, float w, float h,
                          const std::vector<std::string>& labels,
                          double minVal, double maxVal, Color color,
                          int hoveredIdx) {
    if (series.errorBarData.empty()) return;
    
    size_t n = series.errorBarData.size();
    double range = maxVal - minVal;
    if (range <= 0) range = 100;
    
    float barW = w / n;
    float capW = 10.0f;
    
    for (size_t i = 0; i < n; i++) {
        const auto& e = series.errorBarData[i];
        float cx = x + (i + 0.5f) * barW;
        
        float valueY = y + h - h * (e.value - minVal) / range;
        float lowY = y + h - h * (e.value - e.errorLow - minVal) / range;
        float highY = y + h - h * (e.value + e.errorHigh - minVal) / range;
        
        bool hovered = ((int)i == hoveredIdx);
        float pointR = hovered ? 6.0f : 4.0f;
        
        // Draw error bar line
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx, lowY);
        nvgLineTo(vg, cx, highY);
        nvgStrokeColor(vg, color.toNVG());
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);
        
        // Draw caps
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx - capW / 2, lowY);
        nvgLineTo(vg, cx + capW / 2, lowY);
        nvgMoveTo(vg, cx - capW / 2, highY);
        nvgLineTo(vg, cx + capW / 2, highY);
        nvgStroke(vg);
        
        // Draw center point
        nvgBeginPath(vg);
        nvgCircle(vg, cx, valueY, pointR);
        nvgFillColor(vg, hovered ?
            nvgRGBA(std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30), 255) :
            color.toNVG());
        nvgFill(vg);
        
        nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 200));
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);
    }
    
    // Draw labels
    nvgFontSize(vg, 10.0f);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGB(100, 100, 100));
    
    for (size_t i = 0; i < std::min(n, labels.size()); i++) {
        float cx = x + (i + 0.5f) * barW;
        nvgText(vg, cx, y + h + 5, labels[i].c_str(), nullptr);
    }
}

} // namespace flexchart
