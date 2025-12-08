#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderBulletSeries(NVGcontext* vg, const SeriesData& series,
                        float x, float y, float w, float h,
                        int hoveredIdx,
                        const std::vector<Color>& colors) {
    if (series.bulletData.empty()) return;
    
    size_t n = series.bulletData.size();
    float bulletH = std::min(40.0f, (h - 20) / n);
    float gap = 10.0f;
    float labelW = 60.0f;
    float barW = w - labelW - 20;
    
    for (size_t i = 0; i < n; i++) {
        const auto& bd = series.bulletData[i];
        float by = y + 10 + i * (bulletH + gap);
        float bx = x + labelW;
        
        // Find max value for scaling
        double maxVal = bd.target;
        maxVal = std::max(maxVal, bd.actual);
        for (double r : bd.ranges) maxVal = std::max(maxVal, r);
        if (maxVal <= 0) maxVal = 100;
        
        // Draw ranges (background bars)
        std::vector<double> sortedRanges = bd.ranges;
        std::sort(sortedRanges.rbegin(), sortedRanges.rend());
        
        for (size_t ri = 0; ri < sortedRanges.size(); ri++) {
            float rw = barW * (sortedRanges[ri] / maxVal);
            uint8_t gray = 200 - ri * 30;
            
            nvgBeginPath(vg);
            nvgRect(vg, bx, by, rw, bulletH);
            nvgFillColor(vg, nvgRGBA(gray, gray, gray, 255));
            nvgFill(vg);
        }
        
        // Draw actual bar
        float actualW = barW * (bd.actual / maxVal);
        bool hovered = ((int)i == hoveredIdx);
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[i % colors.size()];
        
        nvgBeginPath(vg);
        nvgRect(vg, bx, by + bulletH * 0.25f, actualW, bulletH * 0.5f);
        nvgFillColor(vg, hovered ? 
            nvgRGBA(std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30), 255) :
            color.toNVG());
        nvgFill(vg);
        
        // Draw target marker
        float targetX = bx + barW * (bd.target / maxVal);
        nvgBeginPath(vg);
        nvgMoveTo(vg, targetX, by);
        nvgLineTo(vg, targetX, by + bulletH);
        nvgStrokeColor(vg, nvgRGB(50, 50, 50));
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);
        
        // Draw label
        nvgFontSize(vg, 11.0f);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(60, 60, 60));
        nvgText(vg, bx - 5, by + bulletH / 2, bd.name.c_str(), nullptr);
    }
}

} // namespace flexchart
