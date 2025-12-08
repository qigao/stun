#include <flexchart/flexchart.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flexchart {

void renderPolarSeries(NVGcontext* vg, const SeriesData& series,
                       float cx, float cy, float radius,
                       const std::vector<std::string>& labels,
                       Color color, int hoveredIdx) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    float angleStep = 2 * M_PI / n;
    float startAngle = -M_PI / 2;
    
    // Find max value
    double maxVal = 0;
    for (double v : series.data) maxVal = std::max(maxVal, v);
    if (maxVal <= 0) maxVal = 100;
    
    // Draw circular grid
    for (int i = 1; i <= 4; i++) {
        float r = radius * i / 4.0f;
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, r);
        nvgStrokeColor(vg, nvgRGBA(200, 200, 200, 100));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
    }
    
    // Draw angle lines
    for (size_t i = 0; i < n; i++) {
        float angle = startAngle + i * angleStep;
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx, cy);
        nvgLineTo(vg, cx + std::cos(angle) * radius, cy + std::sin(angle) * radius);
        nvgStrokeColor(vg, nvgRGBA(200, 200, 200, 100));
        nvgStroke(vg);
    }
    
    if (series.polarType == "bar") {
        // Polar bar chart
        float barWidth = angleStep * 0.7f;
        
        for (size_t i = 0; i < n; i++) {
            float angle = startAngle + i * angleStep;
            float r = radius * (series.data[i] / maxVal);
            
            bool hovered = ((int)i == hoveredIdx);
            NVGcolor fillColor = hovered ? 
                nvgRGBA(std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30), 255) :
                color.toNVG();
            
            nvgBeginPath(vg);
            nvgMoveTo(vg, cx, cy);
            nvgArc(vg, cx, cy, r, angle - barWidth/2, angle + barWidth/2, NVG_CW);
            nvgClosePath(vg);
            nvgFillColor(vg, fillColor);
            nvgFill(vg);
        }
    } else {
        // Polar line/area chart
        std::vector<float> px(n), py(n);
        for (size_t i = 0; i < n; i++) {
            float angle = startAngle + i * angleStep;
            float r = radius * (series.data[i] / maxVal);
            px[i] = cx + std::cos(angle) * r;
            py[i] = cy + std::sin(angle) * r;
        }
        
        // Fill area
        nvgBeginPath(vg);
        nvgMoveTo(vg, px[0], py[0]);
        for (size_t i = 1; i < n; i++) {
            nvgLineTo(vg, px[i], py[i]);
        }
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGBA(color.r, color.g, color.b, 50));
        nvgFill(vg);
        
        // Stroke line
        nvgBeginPath(vg);
        nvgMoveTo(vg, px[0], py[0]);
        for (size_t i = 1; i < n; i++) {
            nvgLineTo(vg, px[i], py[i]);
        }
        nvgClosePath(vg);
        nvgStrokeColor(vg, color.toNVG());
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);
        
        // Points
        for (size_t i = 0; i < n; i++) {
            bool hovered = ((int)i == hoveredIdx);
            float pointR = hovered ? 5.0f : 3.0f;
            
            nvgBeginPath(vg);
            nvgCircle(vg, px[i], py[i], pointR);
            nvgFillColor(vg, color.toNVG());
            nvgFill(vg);
        }
    }
    
    // Draw labels
    nvgFontSize(vg, 10.0f);
    nvgFontFace(vg, "sans-serif");
    nvgFillColor(vg, nvgRGB(100, 100, 100));
    
    for (size_t i = 0; i < std::min(n, labels.size()); i++) {
        float angle = startAngle + i * angleStep;
        float lx = cx + std::cos(angle) * (radius + 15);
        float ly = cy + std::sin(angle) * (radius + 15);
        
        int align = NVG_ALIGN_MIDDLE;
        if (std::abs(std::cos(angle)) < 0.1) {
            align |= NVG_ALIGN_CENTER;
        } else if (std::cos(angle) > 0) {
            align |= NVG_ALIGN_LEFT;
        } else {
            align |= NVG_ALIGN_RIGHT;
        }
        
        nvgTextAlign(vg, align);
        nvgText(vg, lx, ly, labels[i].c_str(), nullptr);
    }
}

} // namespace flexchart
