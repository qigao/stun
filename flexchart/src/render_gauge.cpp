#include <flexchart/flexchart.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flexchart {

void renderGaugeSeries(NVGcontext* vg, const SeriesData& series,
                       float cx, float cy, float radius,
                       Color color) {
    float startAngle = series.startAngle * M_PI / 180.0f;
    float endAngle = series.endAngle * M_PI / 180.0f;
    
    // Normalize angles
    while (endAngle > startAngle) endAngle -= 2 * M_PI;
    float totalAngle = startAngle - endAngle;
    
    double range = series.gaugeMax - series.gaugeMin;
    if (range <= 0) range = 100;
    double ratio = (series.gaugeValue - series.gaugeMin) / range;
    ratio = std::max(0.0, std::min(1.0, ratio));
    
    float valueAngle = startAngle - totalAngle * ratio;
    
    // Draw background arc
    float arcWidth = radius * 0.15f;
    nvgBeginPath(vg);
    nvgArc(vg, cx, cy, radius - arcWidth/2, startAngle, endAngle, NVG_CCW);
    nvgStrokeColor(vg, nvgRGBA(230, 230, 230, 255));
    nvgStrokeWidth(vg, arcWidth);
    nvgLineCap(vg, NVG_ROUND);
    nvgStroke(vg);
    
    // Draw value arc with gradient effect
    if (ratio > 0) {
        nvgBeginPath(vg);
        nvgArc(vg, cx, cy, radius - arcWidth/2, startAngle, valueAngle, NVG_CCW);
        nvgStrokeColor(vg, color.toNVG());
        nvgStrokeWidth(vg, arcWidth);
        nvgLineCap(vg, NVG_ROUND);
        nvgStroke(vg);
    }
    
    // Draw tick marks
    int numTicks = 10;
    nvgStrokeColor(vg, nvgRGB(150, 150, 150));
    nvgStrokeWidth(vg, 1.5f);
    
    for (int i = 0; i <= numTicks; i++) {
        float tickAngle = startAngle - totalAngle * i / numTicks;
        float innerR = radius - arcWidth - 5;
        float outerR = radius - arcWidth - (i % 5 == 0 ? 15 : 10);
        
        float x1 = cx + std::cos(tickAngle) * innerR;
        float y1 = cy + std::sin(tickAngle) * innerR;
        float x2 = cx + std::cos(tickAngle) * outerR;
        float y2 = cy + std::sin(tickAngle) * outerR;
        
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1, y1);
        nvgLineTo(vg, x2, y2);
        nvgStroke(vg);
        
        // Draw tick labels for major ticks
        if (i % 5 == 0) {
            float labelR = radius - arcWidth - 25;
            float lx = cx + std::cos(tickAngle) * labelR;
            float ly = cy + std::sin(tickAngle) * labelR;
            
            double tickValue = series.gaugeMin + range * i / numTicks;
            char label[32];
            snprintf(label, sizeof(label), "%.0f", tickValue);
            
            nvgFontSize(vg, 10.0f);
            nvgFontFace(vg, "sans-serif");
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, nvgRGB(100, 100, 100));
            nvgText(vg, lx, ly, label, nullptr);
        }
    }
    
    // Draw pointer
    float pointerLength = radius * 0.6f;
    float pointerWidth = 8.0f;
    
    float px = cx + std::cos(valueAngle) * pointerLength;
    float py = cy + std::sin(valueAngle) * pointerLength;
    
    float perpAngle = valueAngle + M_PI / 2;
    float bx1 = cx + std::cos(perpAngle) * pointerWidth/2;
    float by1 = cy + std::sin(perpAngle) * pointerWidth/2;
    float bx2 = cx - std::cos(perpAngle) * pointerWidth/2;
    float by2 = cy - std::sin(perpAngle) * pointerWidth/2;
    
    nvgBeginPath(vg);
    nvgMoveTo(vg, px, py);
    nvgLineTo(vg, bx1, by1);
    nvgLineTo(vg, bx2, by2);
    nvgClosePath(vg);
    nvgFillColor(vg, nvgRGB(80, 80, 80));
    nvgFill(vg);
    
    // Draw center circle
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 8);
    nvgFillColor(vg, nvgRGB(60, 60, 60));
    nvgFill(vg);
    
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 4);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgFill(vg);
    
    // Draw value text
    if (series.showGaugeDetail) {
        char valueText[64];
        snprintf(valueText, sizeof(valueText), "%.1f", series.gaugeValue);
        
        nvgFontSize(vg, 24.0f);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(50, 50, 50));
        nvgText(vg, cx, cy + radius * 0.35f, valueText, nullptr);
        
        if (!series.name.empty()) {
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGB(120, 120, 120));
            nvgText(vg, cx, cy + radius * 0.5f, series.name.c_str(), nullptr);
        }
    }
}

} // namespace flexchart
