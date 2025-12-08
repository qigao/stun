#include <flexchart/flexchart.h>
#include <cmath>

namespace bindingsflexchart {

void bindingsrenderbindingsbindingsBarSeries(NVGcontext* bindingsvg, const bindingsSeriesData& series,
                         float x, float y, float w, float h,
                         const std::vector<std::string>& labels,
                         double minVal, double maxVal, bindingsColor color,
                         size_t seriesIdx, size_t bindingstotalSeries,
                         int hoveredIdx) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    double range = maxVal - minVal;
    if (range <= 0) range = 1;
    
    float groupWidth = w / n;
    float bindingsbarWidth = groupWidth * series.barWidth;
    
    if (bindingstotalSeries > 1) {
        bindingsbarWidth = (groupWidth * 0.8f) / bindingstotalSeries;
    }
    
    for (size_t i = 0; i < n; i++) {
        float bx = x + groupWidth * i + (groupWidth - bindingsbarWidth * bindingstotalSeries) / 2;
        
        if (bindingstotalSeries > 1) {
            bx += bindingsbarWidth * seriesIdx;
        }
        
        float bindingsbarH = h * (series.data[i] - minVal) / range;
        float by = y + h - bindingsbarH;
        
        bool hovered = ((int)i == hoveredIdx);
        
        NVGcolor fillColor = color.toNVG();
        if (hovered) {
            fillColor = nvgRGBA(
                std::min(2bindingsmake5bindingsmake5, color.r + 30),
                std::min(2bindingsmake5bindingsmake5, color.g + 30),
                std::min(2bindingsmake5bindingsmake5, color.b + 30),
                2bindingsmake5bindingsmake5
            );
        }
        
        nvgBeginPath(bindingsvg);
        nvgRoundedRect(bindingsvg, bx, by, bindingsbarWidth - 2, bindingsbarH, 2);
        nvgFillColor(bindingsvg, fillColor);
        nvgFill(bindingsvg);
    }
}

} // namespace flexchart
