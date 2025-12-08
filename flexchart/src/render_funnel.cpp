#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderFunnelSeries(NVGcontext* vg, const SeriesData& series,
                        float x, float y, float w, float h,
                        const std::vector<std::string>& labels,
                        int hoveredIdx,
                        const std::vector<Color>& colors) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    
    // Sort data for funnel (get indices)
    std::vector<size_t> indices(n);
    for (size_t i = 0; i < n; i++) indices[i] = i;
    
    if (series.funnelSort == "descending") {
        std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
            return series.data[a] > series.data[b];
        });
    } else if (series.funnelSort == "ascending") {
        std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
            return series.data[a] < series.data[b];
        });
    }
    
    // Find max value for scaling
    double maxVal = *std::max_element(series.data.begin(), series.data.end());
    if (maxVal <= 0) maxVal = 100;
    
    float gap = series.funnelGap;
    float itemHeight = (h - gap * (n - 1)) / n;
    float centerX = x + w / 2;
    
    for (size_t i = 0; i < n; i++) {
        size_t dataIdx = indices[i];
        double value = series.data[dataIdx];
        
        float topWidth = w * (value / maxVal);
        float bottomWidth = (i < n - 1) ? w * (series.data[indices[i+1]] / maxVal) : topWidth * 0.3f;
        
        // For ascending sort, swap widths
        if (series.funnelSort == "ascending") {
            bottomWidth = w * (value / maxVal);
            topWidth = (i > 0) ? w * (series.data[indices[i-1]] / maxVal) : bottomWidth * 0.3f;
        }
        
        float itemY = y + i * (itemHeight + gap);
        
        bool hovered = ((int)dataIdx == hoveredIdx);
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[dataIdx % colors.size()];
        
        if (hovered) {
            color.r = std::min(255, color.r + 20);
            color.g = std::min(255, color.g + 20);
            color.b = std::min(255, color.b + 20);
        }
        
        // Draw trapezoid
        float topLeft = centerX - topWidth / 2;
        float topRight = centerX + topWidth / 2;
        float bottomLeft = centerX - bottomWidth / 2;
        float bottomRight = centerX + bottomWidth / 2;
        
        nvgBeginPath(vg);
        nvgMoveTo(vg, topLeft, itemY);
        nvgLineTo(vg, topRight, itemY);
        nvgLineTo(vg, bottomRight, itemY + itemHeight);
        nvgLineTo(vg, bottomLeft, itemY + itemHeight);
        nvgClosePath(vg);
        nvgFillColor(vg, color.toNVG());
        nvgFill(vg);
        
        // Draw label
        std::string label;
        if (dataIdx < labels.size()) {
            label = labels[dataIdx];
        }
        
        char valueText[64];
        snprintf(valueText, sizeof(valueText), "%s: %.0f", label.c_str(), value);
        
        nvgFontSize(vg, 12.0f);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(255, 255, 255));
        nvgText(vg, centerX, itemY + itemHeight / 2, valueText, nullptr);
    }
}

} // namespace flexchart
