#include <flexchart/flexchart.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flexchart {

void renderRadarSeries(NVGcontext* vg, const SeriesData& series,
                       float cx, float cy, float radius,
                       Color defaultColor, int hoveredIdx,
                       const std::vector<Color>& colors) {
    if (series.radarIndicators.empty()) return;
    
    size_t n = series.radarIndicators.size();
    float angleStep = 2 * M_PI / n;
    float startAngle = -M_PI / 2;
    
    // Draw background polygon layers
    for (int layer = 5; layer >= 1; layer--) {
        float layerRadius = radius * layer / 5.0f;
        nvgBeginPath(vg);
        for (size_t i = 0; i < n; i++) {
            float angle = startAngle + i * angleStep;
            float px = cx + std::cos(angle) * layerRadius;
            float py = cy + std::sin(angle) * layerRadius;
            if (i == 0) nvgMoveTo(vg, px, py);
            else nvgLineTo(vg, px, py);
        }
        nvgClosePath(vg);
        nvgStrokeColor(vg, nvgRGBA(200, 200, 200, 100));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
    }
    
    // Draw axis lines
    nvgStrokeColor(vg, nvgRGBA(180, 180, 180, 150));
    for (size_t i = 0; i < n; i++) {
        float angle = startAngle + i * angleStep;
        float px = cx + std::cos(angle) * radius;
        float py = cy + std::sin(angle) * radius;
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx, cy);
        nvgLineTo(vg, px, py);
        nvgStroke(vg);
    }
    
    // Draw indicator labels
    nvgFontSize(vg, 11.0f);
    nvgFontFace(vg, "sans-serif");
    nvgFillColor(vg, nvgRGB(100, 100, 100));
    
    for (size_t i = 0; i < n; i++) {
        float angle = startAngle + i * angleStep;
        float labelRadius = radius + 15;
        float px = cx + std::cos(angle) * labelRadius;
        float py = cy + std::sin(angle) * labelRadius;
        
        int align = NVG_ALIGN_MIDDLE;
        if (std::abs(std::cos(angle)) < 0.1) {
            align |= NVG_ALIGN_CENTER;
        } else if (std::cos(angle) > 0) {
            align |= NVG_ALIGN_LEFT;
        } else {
            align |= NVG_ALIGN_RIGHT;
        }
        
        nvgTextAlign(vg, align);
        nvgText(vg, px, py, series.radarIndicators[i].name.c_str(), nullptr);
    }
    
    // Draw data polygons
    for (size_t di = 0; di < series.radarData.size(); di++) {
        const auto& dataSet = series.radarData[di];
        if (dataSet.size() != n) continue;
        
        Color color = colors.empty() ? defaultColor : colors[di % colors.size()];
        
        // Calculate points
        std::vector<float> px(n), py(n);
        for (size_t i = 0; i < n; i++) {
            float angle = startAngle + i * angleStep;
            const auto& ind = series.radarIndicators[i];
            double range = ind.max - ind.min;
            if (range <= 0) range = 100;
            float ratio = (dataSet[i] - ind.min) / range;
            ratio = std::max(0.0f, std::min(1.0f, ratio));
            float r = radius * ratio;
            px[i] = cx + std::cos(angle) * r;
            py[i] = cy + std::sin(angle) * r;
        }
        
        // Draw filled area
        if (series.radarAreaStyle) {
            nvgBeginPath(vg);
            for (size_t i = 0; i < n; i++) {
                if (i == 0) nvgMoveTo(vg, px[i], py[i]);
                else nvgLineTo(vg, px[i], py[i]);
            }
            nvgClosePath(vg);
            nvgFillColor(vg, nvgRGBA(color.r, color.g, color.b, 50));
            nvgFill(vg);
        }
        
        // Draw outline
        nvgBeginPath(vg);
        for (size_t i = 0; i < n; i++) {
            if (i == 0) nvgMoveTo(vg, px[i], py[i]);
            else nvgLineTo(vg, px[i], py[i]);
        }
        nvgClosePath(vg);
        nvgStrokeColor(vg, color.toNVG());
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);
        
        // Draw points
        for (size_t i = 0; i < n; i++) {
            bool hovered = (hoveredIdx == (int)i);
            float r = hovered ? 5.0f : 3.0f;
            
            nvgBeginPath(vg);
            nvgCircle(vg, px[i], py[i], r);
            nvgFillColor(vg, color.toNVG());
            nvgFill(vg);
        }
    }
}

} // namespace flexchart
