#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flexchart {

void renderRadialBarSeries(NVGcontext* vg, const SeriesData& series,
                           float cx, float cy, float radius,
                           const std::vector<std::string>& labels,
                           int hoveredIdx, const std::vector<Color>& colors) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    
    // Find max value
    double maxVal = 0;
    for (double v : series.data) maxVal = std::max(maxVal, v);
    if (maxVal <= 0) maxVal = 100;
    
    float barWidth = series.radialBarWidth;
    float startAngle = -M_PI / 2;
    float gap = 5.0f;
    
    for (size_t i = 0; i < n; i++) {
        float r = radius - i * (barWidth + gap);
        if (r < 20) break;
        
        double ratio = series.data[i] / maxVal;
        float sweepAngle = 2 * M_PI * ratio;
        
        bool hovered = ((int)i == hoveredIdx);
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[i % colors.size()];
        
        // Background arc
        nvgBeginPath(vg);
        nvgArc(vg, cx, cy, r - barWidth / 2, startAngle, startAngle + 2 * M_PI, NVG_CW);
        nvgStrokeColor(vg, nvgRGBA(220, 220, 220, 255));
        nvgStrokeWidth(vg, barWidth);
        nvgLineCap(vg, NVG_ROUND);
        nvgStroke(vg);
        
        // Value arc
        if (ratio > 0) {
            nvgBeginPath(vg);
            nvgArc(vg, cx, cy, r - barWidth / 2, startAngle, startAngle + sweepAngle, NVG_CW);
            nvgStrokeColor(vg, hovered ?
                nvgRGBA(std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30), 255) :
                color.toNVG());
            nvgStrokeWidth(vg, barWidth);
            nvgLineCap(vg, NVG_ROUND);
            nvgStroke(vg);
        }
        
        // Label
        if (i < labels.size()) {
            nvgFontSize(vg, 10.0f);
            nvgFontFace(vg, "sans-serif");
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, nvgRGB(80, 80, 80));
            nvgText(vg, cx + r + 5, cy, labels[i].c_str(), nullptr);
        }
    }
}

} // namespace flexchart
