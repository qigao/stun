#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderLollipopSeries(NVGcontext* vg, const SeriesData& series,
                          float x, float y, float w, float h,
                          const std::vector<std::string>& labels,
                          double minVal, double maxVal, Color color,
                          int hoveredIdx) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    double range = maxVal - minVal;
    if (range <= 0) range = 100;
    
    float barW = w / n;
    float circleR = 8.0f;
    
    for (size_t i = 0; i < n; i++) {
        float cx = x + (i + 0.5f) * barW;
        float valueY = y + h - h * (series.data[i] - minVal) / range;
        float baseY = y + h;
        
        bool hovered = ((int)i == hoveredIdx);
        float r = hovered ? circleR * 1.3f : circleR;
        
        // Draw stick
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx, baseY);
        nvgLineTo(vg, cx, valueY);
        nvgStrokeColor(vg, color.toNVG());
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);
        
        // Draw circle
        nvgBeginPath(vg);
        nvgCircle(vg, cx, valueY, r);
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
