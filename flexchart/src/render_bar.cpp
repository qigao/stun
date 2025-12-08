#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderBarSeries(NVGcontext* vg, const SeriesData& series,
                     float x, float y, float w, float h,
                     const std::vector<std::string>& labels,
                     double minVal, double maxVal, Color color,
                     size_t seriesIdx, size_t totalSeries,
                     int hoveredIdx) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    double range = maxVal - minVal;
    if (range <= 0) range = 1;
    
    float groupWidth = w / n;
    float barWidth = groupWidth * series.barWidth;
    
    if (totalSeries > 1) {
        barWidth = (groupWidth * 0.8f) / totalSeries;
    }
    
    for (size_t i = 0; i < n; i++) {
        float bx = x + groupWidth * i + (groupWidth - barWidth * totalSeries) / 2;
        
        if (totalSeries > 1) {
            bx += barWidth * seriesIdx;
        }
        
        float barH = h * (series.data[i] - minVal) / range;
        float by = y + h - barH;
        
        bool hovered = ((int)i == hoveredIdx);
        
        NVGcolor fillColor = color.toNVG();
        if (hovered) {
            fillColor = nvgRGBA(
                std::min(255, color.r + 30),
                std::min(255, color.g + 30),
                std::min(255, color.b + 30),
                255
            );
        }
        
        nvgBeginPath(vg);
        nvgRoundedRect(vg, bx, by, barWidth - 2, barH, 2);
        nvgFillColor(vg, fillColor);
        nvgFill(vg);
    }
}

} // namespace flexchart
