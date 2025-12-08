#include <flexchart/flexchart.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flexchart {

void renderRingSeries(NVGcontext* vg, const SeriesData& series,
                      float cx, float cy, float radius,
                      Color color) {
    float startAngle = -M_PI / 2;
    float endAngle = startAngle + 2 * M_PI;
    
    double ratio = series.ringMax > 0 ? series.ringValue / series.ringMax : 0;
    ratio = std::max(0.0, std::min(1.0, ratio));
    float valueAngle = startAngle + 2 * M_PI * ratio;
    
    float ringW = series.ringWidth;
    float innerR = radius - ringW;
    
    // Background ring
    nvgBeginPath(vg);
    nvgArc(vg, cx, cy, radius - ringW/2, startAngle, endAngle, NVG_CW);
    nvgStrokeColor(vg, series.ringBackgroundColor.toNVG());
    nvgStrokeWidth(vg, ringW);
    nvgLineCap(vg, NVG_ROUND);
    nvgStroke(vg);
    
    // Value ring
    if (ratio > 0) {
        nvgBeginPath(vg);
        nvgArc(vg, cx, cy, radius - ringW/2, startAngle, valueAngle, NVG_CW);
        nvgStrokeColor(vg, color.toNVG());
        nvgStrokeWidth(vg, ringW);
        nvgLineCap(vg, NVG_ROUND);
        nvgStroke(vg);
    }
    
    // Center text
    char text[32];
    snprintf(text, sizeof(text), "%.0f%%", ratio * 100);
    
    nvgFontSize(vg, radius * 0.35f);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, nvgRGB(60, 60, 60));
    nvgText(vg, cx, cy, text, nullptr);
    
    // Label below percentage
    if (!series.name.empty()) {
        nvgFontSize(vg, radius * 0.15f);
        nvgFillColor(vg, nvgRGB(120, 120, 120));
        nvgText(vg, cx, cy + radius * 0.25f, series.name.c_str(), nullptr);
    }
}

} // namespace flexchart
