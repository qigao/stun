#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderBubbleSeries(NVGcontext* vg, const SeriesData& series,
                        float x, float y, float w, float h,
                        int hoveredIdx, const std::vector<Color>& colors) {
    if (series.bubbleData.empty()) return;
    
    // Find data ranges
    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9, maxSize = 0;
    for (const auto& b : series.bubbleData) {
        minX = std::min(minX, b.x);
        maxX = std::max(maxX, b.x);
        minY = std::min(minY, b.y);
        maxY = std::max(maxY, b.y);
        maxSize = std::max(maxSize, b.size);
    }
    
    double rangeX = maxX - minX > 0 ? maxX - minX : 1;
    double rangeY = maxY - minY > 0 ? maxY - minY : 1;
    if (maxSize <= 0) maxSize = 10;
    
    float padding = 30;
    float chartW = w - padding * 2;
    float chartH = h - padding * 2;
    
    // Draw grid
    nvgStrokeColor(vg, nvgRGBA(200, 200, 200, 100));
    nvgStrokeWidth(vg, 1.0f);
    for (int i = 0; i <= 4; i++) {
        float gy = y + padding + chartH * i / 4;
        nvgBeginPath(vg);
        nvgMoveTo(vg, x + padding, gy);
        nvgLineTo(vg, x + w - padding, gy);
        nvgStroke(vg);
    }
    
    // Draw bubbles
    for (size_t i = 0; i < series.bubbleData.size(); i++) {
        const auto& b = series.bubbleData[i];
        
        float px = x + padding + chartW * (b.x - minX) / rangeX;
        float py = y + padding + chartH * (1 - (b.y - minY) / rangeY);
        float r = 5 + 25 * (b.size / maxSize);
        
        bool hovered = ((int)i == hoveredIdx);
        if (hovered) r *= 1.2f;
        
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[i % colors.size()];
        
        nvgBeginPath(vg);
        nvgCircle(vg, px, py, r);
        nvgFillColor(vg, nvgRGBA(color.r, color.g, color.b, 180));
        nvgFill(vg);
        
        nvgStrokeColor(vg, color.toNVG());
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);
        
        // Label
        if (!b.name.empty()) {
            nvgFontSize(vg, 9.0f);
            nvgFontFace(vg, "sans-serif");
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, nvgRGB(60, 60, 60));
            nvgText(vg, px, py, b.name.c_str(), nullptr);
        }
    }
}

} // namespace flexchart
