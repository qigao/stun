#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderSlopeSeries(NVGcontext* vg, const SeriesData& series,
                       float x, float y, float w, float h,
                       int hoveredIdx, const std::vector<Color>& colors) {
    if (series.slopeData.empty()) return;
    
    // Find range
    double minVal = 1e9, maxVal = -1e9;
    for (const auto& s : series.slopeData) {
        minVal = std::min(minVal, std::min(s.start, s.end));
        maxVal = std::max(maxVal, std::max(s.start, s.end));
    }
    double range = maxVal - minVal;
    if (range <= 0) range = 100;
    double padding = range * 0.1;
    minVal -= padding;
    maxVal += padding;
    range = maxVal - minVal;
    
    float leftX = x + 80;
    float rightX = x + w - 80;
    float chartH = h - 40;
    
    // Draw axis lines
    nvgBeginPath(vg);
    nvgMoveTo(vg, leftX, y + 20);
    nvgLineTo(vg, leftX, y + 20 + chartH);
    nvgMoveTo(vg, rightX, y + 20);
    nvgLineTo(vg, rightX, y + 20 + chartH);
    nvgStrokeColor(vg, nvgRGBA(180, 180, 180, 255));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);
    
    // Draw labels
    nvgFontSize(vg, 11.0f);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
    nvgFillColor(vg, nvgRGB(80, 80, 80));
    nvgText(vg, leftX, y + 15, series.slopeStartLabel.c_str(), nullptr);
    nvgText(vg, rightX, y + 15, series.slopeEndLabel.c_str(), nullptr);
    
    // Draw slopes
    for (size_t i = 0; i < series.slopeData.size(); i++) {
        const auto& s = series.slopeData[i];
        
        float y1 = y + 20 + chartH * (1 - (s.start - minVal) / range);
        float y2 = y + 20 + chartH * (1 - (s.end - minVal) / range);
        
        bool hovered = ((int)i == hoveredIdx);
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[i % colors.size()];
        
        // Draw line
        nvgBeginPath(vg);
        nvgMoveTo(vg, leftX, y1);
        nvgLineTo(vg, rightX, y2);
        nvgStrokeColor(vg, hovered ?
            nvgRGBA(std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30), 255) :
            color.toNVG());
        nvgStrokeWidth(vg, hovered ? 3.0f : 2.0f);
        nvgStroke(vg);
        
        // Draw points
        float r = hovered ? 6.0f : 4.0f;
        nvgBeginPath(vg);
        nvgCircle(vg, leftX, y1, r);
        nvgCircle(vg, rightX, y2, r);
        nvgFillColor(vg, color.toNVG());
        nvgFill(vg);
        
        // Draw labels
        nvgFontSize(vg, 9.0f);
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(80, 80, 80));
        nvgText(vg, leftX - 8, y1, s.name.c_str(), nullptr);
        
        // Draw values
        char val1[16], val2[16];
        snprintf(val1, sizeof(val1), "%.0f", s.start);
        snprintf(val2, sizeof(val2), "%.0f", s.end);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, rightX + 8, y2, val2, nullptr);
    }
}

} // namespace flexchart
