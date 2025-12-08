#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderCandlestickSeries(NVGcontext* vg, const SeriesData& series,
                             float x, float y, float w, float h,
                             const std::vector<std::string>& labels,
                             double minVal, double maxVal,
                             int hoveredIdx) {
    if (series.candlestickData.empty()) return;
    
    size_t n = series.candlestickData.size();
    double range = maxVal - minVal;
    if (range <= 0) range = 100;
    
    float candleWidth = (w / n) * 0.7f;
    float wickWidth = 1.5f;
    
    for (size_t i = 0; i < n; i++) {
        const auto& candle = series.candlestickData[i];
        
        float cx = x + (w * (i + 0.5f)) / n;
        
        // Calculate y positions
        float openY = y + h * (1.0f - (candle.open - minVal) / range);
        float closeY = y + h * (1.0f - (candle.close - minVal) / range);
        float highY = y + h * (1.0f - (candle.high - minVal) / range);
        float lowY = y + h * (1.0f - (candle.low - minVal) / range);
        
        bool isUp = candle.close >= candle.open;
        Color color = isUp ? series.upColor : series.downColor;
        
        bool hovered = ((int)i == hoveredIdx);
        if (hovered) {
            color.r = std::min(255, color.r + 30);
            color.g = std::min(255, color.g + 30);
            color.b = std::min(255, color.b + 30);
        }
        
        // Draw wick (high-low line)
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx, highY);
        nvgLineTo(vg, cx, lowY);
        nvgStrokeColor(vg, color.toNVG());
        nvgStrokeWidth(vg, wickWidth);
        nvgStroke(vg);
        
        // Draw body (open-close rectangle)
        float bodyTop = std::min(openY, closeY);
        float bodyHeight = std::abs(closeY - openY);
        if (bodyHeight < 1) bodyHeight = 1;
        
        nvgBeginPath(vg);
        nvgRect(vg, cx - candleWidth/2, bodyTop, candleWidth, bodyHeight);
        
        if (isUp) {
            // Hollow candle for up
            nvgStrokeColor(vg, color.toNVG());
            nvgStrokeWidth(vg, 1.5f);
            nvgStroke(vg);
            nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
            nvgFill(vg);
        } else {
            // Filled candle for down
            nvgFillColor(vg, color.toNVG());
            nvgFill(vg);
        }
    }
}

} // namespace flexchart
