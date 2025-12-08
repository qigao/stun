#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderWaterfallSeries(NVGcontext* vg, const SeriesData& series,
                           float x, float y, float w, float h,
                           const std::vector<std::string>& labels,
                           double minVal, double maxVal,
                           int hoveredIdx,
                           const std::vector<Color>& colors) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    bool showTotal = series.waterfallShowTotal;
    size_t totalBars = showTotal ? n + 1 : n;
    
    double range = maxVal - minVal;
    if (range <= 0) range = 100;
    
    float barWidth = (w / totalBars) * 0.6f;
    
    double cumulative = 0;
    double total = 0;
    for (double v : series.data) total += v;
    
    Color positiveColor = colors.size() > 0 ? colors[0] : Color{16, 185, 129, 255};  // Green
    Color negativeColor = colors.size() > 1 ? colors[1] : Color{239, 68, 68, 255};   // Red
    Color totalColor = colors.size() > 2 ? colors[2] : Color{91, 143, 249, 255};     // Blue
    
    for (size_t i = 0; i < n; i++) {
        double value = series.data[i];
        double prevCumulative = cumulative;
        cumulative += value;
        
        float cx = x + (w * (i + 0.5f)) / totalBars;
        
        float startY = y + h * (1.0f - (prevCumulative - minVal) / range);
        float endY = y + h * (1.0f - (cumulative - minVal) / range);
        
        float barTop = std::min(startY, endY);
        float barHeight = std::abs(endY - startY);
        if (barHeight < 1) barHeight = 1;
        
        bool hovered = ((int)i == hoveredIdx);
        bool isPositive = value >= 0;
        Color baseColor = isPositive ? positiveColor : negativeColor;
        
        if (hovered) {
            baseColor.r = std::min(255, baseColor.r + 30);
            baseColor.g = std::min(255, baseColor.g + 30);
            baseColor.b = std::min(255, baseColor.b + 30);
        }
        
        // Draw bar
        nvgBeginPath(vg);
        nvgRoundedRect(vg, cx - barWidth/2, barTop, barWidth, barHeight, 2);
        nvgFillColor(vg, baseColor.toNVG());
        nvgFill(vg);
        
        // Draw connector line to next bar
        if (i < n - 1) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, cx + barWidth/2, endY);
            float nextCx = x + (w * (i + 1.5f)) / totalBars;
            nvgLineTo(vg, nextCx - barWidth/2, endY);
            nvgStrokeColor(vg, nvgRGBA(150, 150, 150, 150));
            nvgStrokeWidth(vg, 1.0f);
            nvgStroke(vg);
        }
    }
    
    // Draw total bar
    if (showTotal) {
        float cx = x + (w * (n + 0.5f)) / totalBars;
        float zeroY = y + h * (1.0f - (0 - minVal) / range);
        float totalY = y + h * (1.0f - (total - minVal) / range);
        
        float barTop = std::min(zeroY, totalY);
        float barHeight = std::abs(totalY - zeroY);
        if (barHeight < 1) barHeight = 1;
        
        bool hovered = ((int)n == hoveredIdx);
        Color color = totalColor;
        if (hovered) {
            color.r = std::min(255, color.r + 30);
            color.g = std::min(255, color.g + 30);
            color.b = std::min(255, color.b + 30);
        }
        
        nvgBeginPath(vg);
        nvgRoundedRect(vg, cx - barWidth/2, barTop, barWidth, barHeight, 2);
        nvgFillColor(vg, color.toNVG());
        nvgFill(vg);
    }
}

} // namespace flexchart
