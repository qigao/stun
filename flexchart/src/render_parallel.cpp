#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderParallelSeries(NVGcontext* vg, const SeriesData& series,
                          float x, float y, float w, float h,
                          int hoveredIdx,
                          const std::vector<Color>& colors) {
    if (series.parallelAxes.empty() || series.parallelData.empty()) return;
    
    size_t numAxes = series.parallelAxes.size();
    float axisSpacing = w / (numAxes - 1);
    float padding = 30.0f;
    float chartH = h - padding * 2;
    
    // Draw axes
    nvgStrokeColor(vg, nvgRGBA(180, 180, 180, 255));
    nvgStrokeWidth(vg, 1.0f);
    
    for (size_t i = 0; i < numAxes; i++) {
        float ax = x + i * axisSpacing;
        
        // Axis line
        nvgBeginPath(vg);
        nvgMoveTo(vg, ax, y + padding);
        nvgLineTo(vg, ax, y + h - padding);
        nvgStroke(vg);
        
        // Axis label
        nvgFontSize(vg, 10.0f);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
        nvgFillColor(vg, nvgRGB(100, 100, 100));
        nvgText(vg, ax, y + padding - 5, series.parallelAxes[i].name.c_str(), nullptr);
        
        // Min/Max labels
        nvgFontSize(vg, 9.0f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        
        char minLabel[32], maxLabel[32];
        snprintf(minLabel, sizeof(minLabel), "%.0f", series.parallelAxes[i].min);
        snprintf(maxLabel, sizeof(maxLabel), "%.0f", series.parallelAxes[i].max);
        
        nvgText(vg, ax, y + h - padding + 5, minLabel, nullptr);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
        nvgText(vg, ax, y + padding - 15, maxLabel, nullptr);
    }
    
    // Draw data lines
    for (size_t di = 0; di < series.parallelData.size(); di++) {
        const auto& dataLine = series.parallelData[di];
        if (dataLine.size() != numAxes) continue;
        
        bool hovered = ((int)di == hoveredIdx);
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[di % colors.size()];
        
        nvgBeginPath(vg);
        
        for (size_t i = 0; i < numAxes; i++) {
            float ax = x + i * axisSpacing;
            const auto& axis = series.parallelAxes[i];
            double range = axis.max - axis.min;
            if (range <= 0) range = 100;
            
            float ratio = (dataLine[i] - axis.min) / range;
            ratio = std::max(0.0f, std::min(1.0f, ratio));
            float py = y + h - padding - chartH * ratio;
            
            if (i == 0) {
                nvgMoveTo(vg, ax, py);
            } else {
                nvgLineTo(vg, ax, py);
            }
        }
        
        nvgStrokeColor(vg, hovered ? 
            nvgRGBA(color.r, color.g, color.b, 255) :
            nvgRGBA(color.r, color.g, color.b, 150));
        nvgStrokeWidth(vg, hovered ? 2.5f : 1.5f);
        nvgStroke(vg);
    }
}

} // namespace flexchart
