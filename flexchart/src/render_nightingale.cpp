#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flexchart {

void renderNightingaleSeries(NVGcontext* vg, const SeriesData& series,
                             float cx, float cy, float radius,
                             const std::vector<std::string>& labels,
                             int hoveredIdx,
                             const std::vector<Color>& colors) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    float angleStep = 2 * M_PI / n;
    float startAngle = -M_PI / 2;
    
    // Find max value
    double maxVal = 0;
    for (double v : series.data) maxVal = std::max(maxVal, v);
    if (maxVal <= 0) maxVal = 100;
    
    float innerR = radius * 0.2f;
    
    for (size_t i = 0; i < n; i++) {
        float angle1 = startAngle + i * angleStep;
        float angle2 = angle1 + angleStep;
        
        // In Nightingale chart, radius varies by value
        float r = innerR + (radius - innerR) * (series.data[i] / maxVal);
        
        bool hovered = ((int)i == hoveredIdx);
        if (hovered) r *= 1.05f;
        
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[i % colors.size()];
        
        // Draw sector
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx + std::cos(angle1) * innerR, cy + std::sin(angle1) * innerR);
        nvgArc(vg, cx, cy, r, angle1, angle2, NVG_CW);
        nvgLineTo(vg, cx + std::cos(angle2) * innerR, cy + std::sin(angle2) * innerR);
        nvgArc(vg, cx, cy, innerR, angle2, angle1, NVG_CCW);
        nvgClosePath(vg);
        
        nvgFillColor(vg, hovered ?
            nvgRGBA(std::min(255, color.r + 20), std::min(255, color.g + 20), std::min(255, color.b + 20), 255) :
            color.toNVG());
        nvgFill(vg);
        
        // Border
        nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 200));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
        
        // Label
        if (i < labels.size()) {
            float midAngle = (angle1 + angle2) / 2;
            float labelR = r + 15;
            float lx = cx + std::cos(midAngle) * labelR;
            float ly = cy + std::sin(midAngle) * labelR;
            
            nvgFontSize(vg, 10.0f);
            nvgFontFace(vg, "sans-serif");
            
            int align = NVG_ALIGN_MIDDLE;
            if (std::cos(midAngle) > 0.1) align |= NVG_ALIGN_LEFT;
            else if (std::cos(midAngle) < -0.1) align |= NVG_ALIGN_RIGHT;
            else align |= NVG_ALIGN_CENTER;
            
            nvgTextAlign(vg, align);
            nvgFillColor(vg, nvgRGB(80, 80, 80));
            nvgText(vg, lx, ly, labels[i].c_str(), nullptr);
        }
    }
}

} // namespace flexchart
