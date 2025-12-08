#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderDumbbellSeries(NVGcontext* vg, const SeriesData& series,
                          float x, float y, float w, float h,
                          int hoveredIdx, const std::vector<Color>& colors) {
    if (series.dumbbellData.empty()) return;
    
    size_t n = series.dumbbellData.size();
    
    // Find range
    double minVal = 1e9, maxVal = -1e9;
    for (const auto& d : series.dumbbellData) {
        minVal = std::min(minVal, std::min(d.start, d.end));
        maxVal = std::max(maxVal, std::max(d.start, d.end));
    }
    double range = maxVal - minVal;
    if (range <= 0) range = 100;
    double padding = range * 0.1;
    minVal -= padding;
    maxVal += padding;
    range = maxVal - minVal;
    
    float rowH = h / n;
    float labelW = 60;
    float chartW = w - labelW - 20;
    
    for (size_t i = 0; i < n; i++) {
        const auto& d = series.dumbbellData[i];
        float cy = y + (i + 0.5f) * rowH;
        
        float x1 = x + labelW + chartW * (d.start - minVal) / range;
        float x2 = x + labelW + chartW * (d.end - minVal) / range;
        
        bool hovered = ((int)i == hoveredIdx);
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[i % colors.size()];
        
        // Draw connecting line
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1, cy);
        nvgLineTo(vg, x2, cy);
        nvgStrokeColor(vg, nvgRGBA(color.r, color.g, color.b, 150));
        nvgStrokeWidth(vg, hovered ? 4.0f : 3.0f);
        nvgStroke(vg);
        
        // Draw start circle
        float r = hovered ? 8.0f : 6.0f;
        nvgBeginPath(vg);
        nvgCircle(vg, x1, cy, r);
        nvgFillColor(vg, color.toNVG());
        nvgFill(vg);
        
        // Draw end circle
        nvgBeginPath(vg);
        nvgCircle(vg, x2, cy, r);
        nvgFillColor(vg, color.toNVG());
        nvgFill(vg);
        
        // Label
        nvgFontSize(vg, 10.0f);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(60, 60, 60));
        nvgText(vg, x + labelW - 5, cy, d.name.c_str(), nullptr);
    }
}

} // namespace flexchart
