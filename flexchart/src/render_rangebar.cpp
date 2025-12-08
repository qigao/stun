#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderRangeBarSeries(NVGcontext* vg, const SeriesData& series,
                          float x, float y, float w, float h,
                          int hoveredIdx, const std::vector<Color>& colors) {
    if (series.rangeBarData.empty()) return;
    
    size_t n = series.rangeBarData.size();
    
    // Find range
    double minVal = 1e9, maxVal = -1e9;
    for (const auto& r : series.rangeBarData) {
        minVal = std::min(minVal, r.start);
        maxVal = std::max(maxVal, r.end);
    }
    double range = maxVal - minVal;
    if (range <= 0) range = 100;
    double padding = range * 0.1;
    minVal -= padding;
    maxVal += padding;
    range = maxVal - minVal;
    
    float barH = (h - 20) / n;
    float gap = barH * 0.2f;
    float labelW = 60;
    float chartW = w - labelW - 20;
    
    for (size_t i = 0; i < n; i++) {
        const auto& r = series.rangeBarData[i];
        float by = y + 10 + i * barH + gap / 2;
        
        float x1 = x + labelW + chartW * (r.start - minVal) / range;
        float x2 = x + labelW + chartW * (r.end - minVal) / range;
        
        bool hovered = ((int)i == hoveredIdx);
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[i % colors.size()];
        
        // Draw bar
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x1, by, x2 - x1, barH - gap, 3);
        nvgFillColor(vg, hovered ?
            nvgRGBA(std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30), 255) :
            color.toNVG());
        nvgFill(vg);
        
        // Label
        nvgFontSize(vg, 10.0f);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(60, 60, 60));
        nvgText(vg, x + labelW - 5, by + (barH - gap) / 2, r.name.c_str(), nullptr);
    }
}

} // namespace flexchart
