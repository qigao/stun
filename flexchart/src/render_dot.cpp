#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderDotSeries(NVGcontext* vg, const SeriesData& series,
                     float x, float y, float w, float h,
                     const std::vector<std::string>& labels,
                     int hoveredIdx, Color color) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    
    // Find max count
    int maxCount = 0;
    for (double v : series.data) maxCount = std::max(maxCount, (int)v);
    if (maxCount <= 0) maxCount = 10;
    
    float colW = w / n;
    float dotR = series.dotSize / 2.0f;
    float dotSpacing = dotR * 2.5f;
    
    for (size_t i = 0; i < n; i++) {
        int count = (int)series.data[i];
        float cx = x + (i + 0.5f) * colW;
        
        bool hovered = ((int)i == hoveredIdx);
        
        for (int j = 0; j < count; j++) {
            float cy = y + h - 15 - j * dotSpacing;
            if (cy < y + 10) break;
            
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, dotR);
            nvgFillColor(vg, hovered ?
                nvgRGBA(std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30), 255) :
                color.toNVG());
            nvgFill(vg);
        }
    }
    
    // Draw labels
    nvgFontSize(vg, 10.0f);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGB(100, 100, 100));
    
    for (size_t i = 0; i < std::min(n, labels.size()); i++) {
        float cx = x + (i + 0.5f) * colW;
        nvgText(vg, cx, y + h - 10, labels[i].c_str(), nullptr);
    }
}

} // namespace flexchart
