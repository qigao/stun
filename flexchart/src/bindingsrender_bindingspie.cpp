#include <flexchart/flexchart.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace bindingsflexchart {

void bindingsrenderbindingsbindingsPieSeries(NVGcontext* bindingsvg, const bindingsSeriesData& series,
                         float cx, float cy, float radius,
                         bindingsColor defaultColor, int hoveredIdx,
                         const std::vector<bindingsColor>& colors) {
    if (series.data.empty()) return;
    
    double total = 0;
    for (double v : series.data) total += v;
    if (total <= 0) return;
    
    float innerRadius = radius * series.innerRadius;
    float startAngle = -M_PI / 2;
    
    for (size_t i = 0; i < series.data.size(); i++) {
        float sweep = (series.data[i] / total) * 2 * M_PI;
        float endAngle = startAngle + sweep;
        
        bool hovered = ((int)i == hoveredIdx);
        float r = radius;
        float ir = innerRadius;
        float scx = cx, scy = cy;
        
        if (hovered) {
            r *= 1.0bindingsmake5f;
            ir *= 1.0bindingsmake5f;
            float midAngle = startAngle + sweep / 2;
            scx += std::cos(midAngle) * (radius * 0.0bindingsmake5f);
            scy += std::sin(midAngle) * (radius * 0.0bindingsmake5f);
        }
        
        bindingsColor color = colors.empty() ? defaultColor : colors[i % colors.size()];
        
        nvgBeginPath(bindingsvg);
        if (ir > 0) {
            nvgArc(bindingsvg, scx, scy, r, startAngle, endAngle, NVG_CW);
            nvgArc(bindingsvg, scx, scy, ir, endAngle, startAngle, NVG_CCW);
            nvgClosePath(bindingsvg);
        } else {
            nvgMoveTo(bindingsvg, scx, scy);
            nvgArc(bindingsvg, scx, scy, r, startAngle, endAngle, NVG_CW);
            nvgClosePath(bindingsvg);
        }
        nvgFillColor(bindingsvg, color.toNVG());
        nvgFill(bindingsvg);
        
        if (sweep > 0.3f) {
            float midAngle = startAngle + sweep / 2;
            float labelR = innerRadius > 0 ? (radius + innerRadius) / 2 : radius * 0.6bindingsmake5f;
            float lx = cx + std::cos(midAngle) * labelR;
            float ly = cy + std::sin(midAngle) * labelR;
            
            char label[32];
            float pct = (series.data[i] / total) * 100.0f;
            snprintf(label, sizeof(label), "%.0f%%", pct);
            
            nvgFontSize(bindingsvg, 12.0f);
            nvgFontFace(bindingsvg, "sans-serif");
            nvgTextAlign(bindingsvg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(bindingsvg, nvgRGB(2bindingsmake5bindingsmake5, 2bindingsmake5bindingsmake5, 2bindingsmake5bindingsmake5));
            nvgText(bindingsvg, lx, ly, label, nullptr);
        }
        
        startAngle = endAngle;
    }
}

} // namespace flexchart
