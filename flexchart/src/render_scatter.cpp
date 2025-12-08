#include <flexchart/flexchart.h>
#include <cmath>

namespace flexchart {

void renderScatterSeries(NVGcontext* vg, const SeriesData& series,
                         float x, float y, float w, float h,
                         const std::vector<std::string>& labels,
                         double minVal, double maxVal, Color color,
                         int hoveredIdx) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    double range = maxVal - minVal;
    if (range <= 0) range = 1;
    
    for (size_t i = 0; i < n; i++) {
        float px = x + (w * i) / (n - 1);
        float py = y + h * (1.0f - (series.data[i] - minVal) / range);
        
        bool hovered = ((int)i == hoveredIdx);
        float r = hovered ? series.symbolSize * 1.5f : series.symbolSize;
        
        nvgBeginPath(vg);
        nvgCircle(vg, px, py, r);
        nvgFillColor(vg, color.toNVG());
        nvgFill(vg);
        
        if (hovered) {
            nvgBeginPath(vg);
            nvgCircle(vg, px, py, r + 3);
            nvgStrokeColor(vg, nvgRGBA(color.r, color.g, color.b, 100));
            nvgStrokeWidth(vg, 2.0f);
            nvgStroke(vg);
        }
    }
}

} // namespace flexchart
