#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderGanttSeries(NVGcontext* vg, const SeriesData& series,
                       float x, float y, float w, float h,
                       int hoveredIdx, const std::vector<Color>& colors) {
    if (series.ganttTasks.empty()) return;
    
    // Find time range
    double minTime = 1e9, maxTime = -1e9;
    for (const auto& t : series.ganttTasks) {
        minTime = std::min(minTime, t.start);
        maxTime = std::max(maxTime, t.end);
    }
    double timeRange = maxTime - minTime;
    if (timeRange <= 0) timeRange = 10;
    
    size_t n = series.ganttTasks.size();
    float rowH = (h - 30) / n;
    float labelW = 80;
    float chartW = w - labelW - 20;
    
    // Draw time grid
    nvgStrokeColor(vg, nvgRGBA(200, 200, 200, 100));
    nvgStrokeWidth(vg, 1.0f);
    for (int i = 0; i <= 5; i++) {
        float gx = x + labelW + chartW * i / 5;
        nvgBeginPath(vg);
        nvgMoveTo(vg, gx, y + 20);
        nvgLineTo(vg, gx, y + h);
        nvgStroke(vg);
    }
    
    // Draw tasks
    for (size_t i = 0; i < n; i++) {
        const auto& t = series.ganttTasks[i];
        float by = y + 25 + i * rowH;
        
        float x1 = x + labelW + chartW * (t.start - minTime) / timeRange;
        float x2 = x + labelW + chartW * (t.end - minTime) / timeRange;
        float barH = rowH * 0.6f;
        
        bool hovered = ((int)i == hoveredIdx);
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[t.category % colors.size()];
        
        // Draw bar
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x1, by, x2 - x1, barH, 3);
        nvgFillColor(vg, hovered ?
            nvgRGBA(std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30), 255) :
            color.toNVG());
        nvgFill(vg);
        
        // Task name
        nvgFontSize(vg, 10.0f);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(60, 60, 60));
        nvgText(vg, x + labelW - 5, by + barH / 2, t.name.c_str(), nullptr);
    }
    
    // Draw time labels
    nvgFontSize(vg, 9.0f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    for (int i = 0; i <= 5; i++) {
        float gx = x + labelW + chartW * i / 5;
        double time = minTime + timeRange * i / 5;
        char label[16];
        snprintf(label, sizeof(label), "%.0f", time);
        nvgText(vg, gx, y + 5, label, nullptr);
    }
}

} // namespace flexchart
