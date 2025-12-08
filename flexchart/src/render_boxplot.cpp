#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderBoxPlotSeries(NVGcontext* vg, const SeriesData& series,
                         float x, float y, float w, float h,
                         const std::vector<std::string>& labels,
                         double minVal, double maxVal, Color color,
                         int hoveredIdx) {
    if (series.boxPlotData.empty()) return;
    
    size_t n = series.boxPlotData.size();
    double range = maxVal - minVal;
    if (range <= 0) range = 100;
    
    float boxWidth = (w / n) * 0.5f;
    float whiskerWidth = boxWidth * 0.6f;
    
    for (size_t i = 0; i < n; i++) {
        const auto& bp = series.boxPlotData[i];
        
        float cx = x + (w * (i + 0.5f)) / n;
        
        float minY = y + h * (1.0f - (bp.min - minVal) / range);
        float q1Y = y + h * (1.0f - (bp.q1 - minVal) / range);
        float medianY = y + h * (1.0f - (bp.median - minVal) / range);
        float q3Y = y + h * (1.0f - (bp.q3 - minVal) / range);
        float maxY = y + h * (1.0f - (bp.max - minVal) / range);
        
        bool hovered = ((int)i == hoveredIdx);
        NVGcolor fillColor = hovered ? 
            nvgRGBA(std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30), 255) :
            color.toNVG();
        
        // Draw vertical line (whisker stem)
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx, maxY);
        nvgLineTo(vg, cx, q3Y);
        nvgMoveTo(vg, cx, q1Y);
        nvgLineTo(vg, cx, minY);
        nvgStrokeColor(vg, fillColor);
        nvgStrokeWidth(vg, 1.5f);
        nvgStroke(vg);
        
        // Draw top whisker
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx - whiskerWidth/2, maxY);
        nvgLineTo(vg, cx + whiskerWidth/2, maxY);
        nvgStroke(vg);
        
        // Draw bottom whisker
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx - whiskerWidth/2, minY);
        nvgLineTo(vg, cx + whiskerWidth/2, minY);
        nvgStroke(vg);
        
        // Draw box (Q1 to Q3)
        nvgBeginPath(vg);
        nvgRect(vg, cx - boxWidth/2, q3Y, boxWidth, q1Y - q3Y);
        nvgFillColor(vg, nvgRGBA(color.r, color.g, color.b, 100));
        nvgFill(vg);
        nvgStrokeColor(vg, fillColor);
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);
        
        // Draw median line
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx - boxWidth/2, medianY);
        nvgLineTo(vg, cx + boxWidth/2, medianY);
        nvgStrokeColor(vg, nvgRGB(255, 100, 100));
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);
        
        // Draw outliers
        for (double outlier : bp.outliers) {
            float oy = y + h * (1.0f - (outlier - minVal) / range);
            nvgBeginPath(vg);
            nvgCircle(vg, cx, oy, 3.0f);
            nvgFillColor(vg, nvgRGB(200, 80, 80));
            nvgFill(vg);
        }
    }
}

} // namespace flexchart
