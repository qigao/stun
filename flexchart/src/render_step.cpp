#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderStepSeries(NVGcontext* vg, const SeriesData& series,
                      float x, float y, float w, float h,
                      const std::vector<std::string>& labels,
                      double minVal, double maxVal, Color color,
                      int hoveredIdx) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    double range = maxVal - minVal;
    if (range <= 0) range = 100;
    
    float stepW = w / (n > 1 ? n - 1 : 1);
    
    auto getY = [&](double val) -> float {
        return y + h - h * (val - minVal) / range;
    };
    
    auto getX = [&](size_t i) -> float {
        return x + i * stepW;
    };
    
    // Draw step line
    nvgBeginPath(vg);
    
    float prevX = getX(0);
    float prevY = getY(series.data[0]);
    nvgMoveTo(vg, prevX, prevY);
    
    for (size_t i = 1; i < n; i++) {
        float curX = getX(i);
        float curY = getY(series.data[i]);
        
        if (series.stepType == "start") {
            // Step at start: horizontal first, then vertical
            nvgLineTo(vg, curX, prevY);
            nvgLineTo(vg, curX, curY);
        } else if (series.stepType == "end") {
            // Step at end: vertical first, then horizontal
            nvgLineTo(vg, prevX, curY);
            nvgLineTo(vg, curX, curY);
        } else {
            // Step at middle
            float midX = (prevX + curX) / 2;
            nvgLineTo(vg, midX, prevY);
            nvgLineTo(vg, midX, curY);
            nvgLineTo(vg, curX, curY);
        }
        
        prevX = curX;
        prevY = curY;
    }
    
    nvgStrokeColor(vg, color.toNVG());
    nvgStrokeWidth(vg, 2.0f);
    nvgStroke(vg);
    
    // Fill area under step
    if (series.areaStyle) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, getX(0), y + h);
        nvgLineTo(vg, getX(0), getY(series.data[0]));
        
        prevX = getX(0);
        prevY = getY(series.data[0]);
        
        for (size_t i = 1; i < n; i++) {
            float curX = getX(i);
            float curY = getY(series.data[i]);
            
            if (series.stepType == "start") {
                nvgLineTo(vg, curX, prevY);
                nvgLineTo(vg, curX, curY);
            } else if (series.stepType == "end") {
                nvgLineTo(vg, prevX, curY);
                nvgLineTo(vg, curX, curY);
            } else {
                float midX = (prevX + curX) / 2;
                nvgLineTo(vg, midX, prevY);
                nvgLineTo(vg, midX, curY);
                nvgLineTo(vg, curX, curY);
            }
            
            prevX = curX;
            prevY = curY;
        }
        
        nvgLineTo(vg, getX(n - 1), y + h);
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGBA(color.r, color.g, color.b, 50));
        nvgFill(vg);
    }
    
    // Draw points
    for (size_t i = 0; i < n; i++) {
        bool hovered = ((int)i == hoveredIdx);
        float px = getX(i);
        float py = getY(series.data[i]);
        float r = hovered ? 5.0f : 3.0f;
        
        nvgBeginPath(vg);
        nvgCircle(vg, px, py, r);
        nvgFillColor(vg, color.toNVG());
        nvgFill(vg);
        
        if (hovered) {
            nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 200));
            nvgStrokeWidth(vg, 2.0f);
            nvgStroke(vg);
        }
    }
}

} // namespace flexchart
