#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderHeatmapSeries(NVGcontext* vg, const SeriesData& series,
                         float x, float y, float w, float h,
                         const std::vector<std::string>& xLabels,
                         int hoveredIdx) {
    if (series.heatmapData.empty()) return;
    
    // Find grid dimensions
    int maxX = 0, maxY = 0;
    double minVal = 1e9, maxVal = -1e9;
    
    for (const auto& d : series.heatmapData) {
        maxX = std::max(maxX, d.x);
        maxY = std::max(maxY, d.y);
        minVal = std::min(minVal, d.value);
        maxVal = std::max(maxVal, d.value);
    }
    
    int cols = maxX + 1;
    int rows = maxY + 1;
    if (cols == 0 || rows == 0) return;
    
    float cellW = w / cols;
    float cellH = h / rows;
    double range = maxVal - minVal;
    if (range <= 0) range = 1;
    
    // Draw cells
    for (size_t i = 0; i < series.heatmapData.size(); i++) {
        const auto& d = series.heatmapData[i];
        
        float cx = x + d.x * cellW;
        float cy = y + d.y * cellH;
        
        // Interpolate color
        double ratio = (d.value - minVal) / range;
        ratio = std::max(0.0, std::min(1.0, ratio));
        
        uint8_t r = series.heatmapMinColor.r + (series.heatmapMaxColor.r - series.heatmapMinColor.r) * ratio;
        uint8_t g = series.heatmapMinColor.g + (series.heatmapMaxColor.g - series.heatmapMinColor.g) * ratio;
        uint8_t b = series.heatmapMinColor.b + (series.heatmapMaxColor.b - series.heatmapMinColor.b) * ratio;
        
        bool hovered = ((int)i == hoveredIdx);
        
        nvgBeginPath(vg);
        nvgRect(vg, cx + 1, cy + 1, cellW - 2, cellH - 2);
        nvgFillColor(vg, nvgRGBA(r, g, b, hovered ? 255 : 220));
        nvgFill(vg);
        
        if (hovered) {
            nvgStrokeColor(vg, nvgRGB(50, 50, 50));
            nvgStrokeWidth(vg, 2.0f);
            nvgStroke(vg);
        }
        
        // Draw value text for larger cells
        if (cellW > 30 && cellH > 20) {
            char text[32];
            snprintf(text, sizeof(text), "%.0f", d.value);
            
            nvgFontSize(vg, std::min(11.0f, cellH * 0.4f));
            nvgFontFace(vg, "sans-serif");
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, ratio > 0.5 ? nvgRGB(255, 255, 255) : nvgRGB(50, 50, 50));
            nvgText(vg, cx + cellW/2, cy + cellH/2, text, nullptr);
        }
    }
    
    // Draw X labels
    if (!xLabels.empty()) {
        nvgFontSize(vg, 10.0f);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(vg, nvgRGB(100, 100, 100));
        
        for (size_t i = 0; i < std::min(xLabels.size(), (size_t)cols); i++) {
            nvgText(vg, x + i * cellW + cellW/2, y + h + 4, xLabels[i].c_str(), nullptr);
        }
    }
    
    // Draw Y labels
    if (!series.heatmapYLabels.empty()) {
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        
        for (size_t i = 0; i < std::min(series.heatmapYLabels.size(), (size_t)rows); i++) {
            nvgText(vg, x - 4, y + i * cellH + cellH/2, series.heatmapYLabels[i].c_str(), nullptr);
        }
    }
}

} // namespace flexchart
