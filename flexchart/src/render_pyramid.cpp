#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderPyramidSeries(NVGcontext* vg, const SeriesData& series,
                         float x, float y, float w, float h,
                         const std::vector<std::string>& labels,
                         int hoveredIdx, const std::vector<Color>& colors) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    
    // Find max value
    double maxVal = 0;
    for (double v : series.data) maxVal = std::max(maxVal, v);
    if (maxVal <= 0) maxVal = 100;
    
    float cx = x + w / 2;
    float totalH = h - 30;
    float sectionH = totalH / n;
    
    for (size_t i = 0; i < n; i++) {
        float ratio = series.data[i] / maxVal;
        float topY = y + 15 + i * sectionH;
        float bottomY = topY + sectionH;
        
        // Calculate widths (pyramid shape - wider at bottom)
        float topRatio = 1.0f - (float)i / n;
        float bottomRatio = 1.0f - (float)(i + 1) / n;
        float topW = (w - 60) * topRatio * ratio;
        float bottomW = (w - 60) * bottomRatio * ratio;
        
        bool hovered = ((int)i == hoveredIdx);
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[i % colors.size()];
        
        // Draw trapezoid
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx - topW / 2, topY);
        nvgLineTo(vg, cx + topW / 2, topY);
        nvgLineTo(vg, cx + bottomW / 2, bottomY);
        nvgLineTo(vg, cx - bottomW / 2, bottomY);
        nvgClosePath(vg);
        
        nvgFillColor(vg, hovered ?
            nvgRGBA(std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30), 255) :
            color.toNVG());
        nvgFill(vg);
        
        nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 200));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
        
        // Label
        if (i < labels.size()) {
            nvgFontSize(vg, 10.0f);
            nvgFontFace(vg, "sans-serif");
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, nvgRGB(255, 255, 255));
            nvgText(vg, cx, (topY + bottomY) / 2, labels[i].c_str(), nullptr);
        }
    }
}

} // namespace flexchart
