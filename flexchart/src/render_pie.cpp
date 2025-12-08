#include <flexchart/flexchart.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flexchart {

void renderPieSeries(NVGcontext* vg, const SeriesData& series,
                     float cx, float cy, float radius,
                     Color defaultColor, int hoveredIdx,
                     const std::vector<Color>& colors) {
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
            r *= 1.05f;
            ir *= 1.05f;
            float midAngle = startAngle + sweep / 2;
            scx += std::cos(midAngle) * (radius * 0.05f);
            scy += std::sin(midAngle) * (radius * 0.05f);
        }
        
        Color color = colors.empty() ? defaultColor : colors[i % colors.size()];
        
        nvgBeginPath(vg);
        if (ir > 0) {
            nvgArc(vg, scx, scy, r, startAngle, endAngle, NVG_CW);
            nvgArc(vg, scx, scy, ir, endAngle, startAngle, NVG_CCW);
            nvgClosePath(vg);
        } else {
            nvgMoveTo(vg, scx, scy);
            nvgArc(vg, scx, scy, r, startAngle, endAngle, NVG_CW);
            nvgClosePath(vg);
        }
        nvgFillColor(vg, color.toNVG());
        nvgFill(vg);
        
        if (sweep > 0.3f) {
            float midAngle = startAngle + sweep / 2;
            float labelR = innerRadius > 0 ? (radius + innerRadius) / 2 : radius * 0.65f;
            float lx = cx + std::cos(midAngle) * labelR;
            float ly = cy + std::sin(midAngle) * labelR;
            
            char label[32];
            float pct = (series.data[i] / total) * 100.0f;
            snprintf(label, sizeof(label), "%.0f%%", pct);
            
            nvgFontSize(vg, 12.0f);
            nvgFontFace(vg, "sans-serif");
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, nvgRGB(255, 255, 255));
            nvgText(vg, lx, ly, label, nullptr);
        }
        
        startAngle = endAngle;
    }
}

} // namespace flexchart
