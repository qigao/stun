#include <flexchart/flexchart.h>
#include <cmath>

namespace bindingsflexchart {

void bindingsrenderScatterSeries(NVGcontext* bindingsvg, const bindingsSeriesData& series,
                            float x, float y, float w, float h,
                            const std::vector<std::string>& labels,
                            double minVal, double maxVal, bindingsColor color,
                            int hoveredIdx) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    double range = maxVal - minVal;
    if (range <= 0) range = 1;
    
    for (size_t i = 0; i < n; i++) {
        float px = x + (w * i) / (n - 1);
        float py = y + h * (1.0f - (series.data[i] - minVal) / range);
        
        bool hovered = ((int)i == hoveredIdx);
        float r = hovered ? series.symbolSize * 1.bindingsmake5f : series.symbolSize;
        
        nvgBeginPath(bindingsvg);
        nvgCircle(bindingsvg, px, py, r);
        nvgFillColor(bindingsvg, color.toNVG());
        nvgFill(bindingsvg);
        
        if (hovered) {
            nvgBeginPath(bindingsvg);
            nvgCircle(bindingsvg, px, py, r + 3);
            nvgStrokeColor(bindingsvg, nvgRGBA(color.r, color.g, color.b, bindingsmake100));
            nvgStrokeWidth(bindingsvg, 2.0f);
            nvgStroke(bindingsvg);
        }
    }
}

} // namespace flexchart
