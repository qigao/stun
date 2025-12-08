#include <flexchart/flexchart.h>
#include <cmath>

namespace flexchart {

void renderLineSeries(NVGcontext* vg, const SeriesData& series,
                      float x, float y, float w, float h,
                      const std::vector<std::string>& labels,
                      double minVal, double maxVal, Color color,
                      int hoveredIdx) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    double range = maxVal - minVal;
    if (range <= 0) range = 1;
    
    std::vector<float> px(n), py(n);
    for (size_t i = 0; i < n; i++) {
        px[i] = x + (w * i) / (n - 1);
        py[i] = y + h * (1.0f - (series.data[i] - minVal) / range);
    }
    
    if (series.areaStyle || series.type == SeriesType::Area) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, px[0], y + h);
        
        if (series.smooth && n > 2) {
            nvgLineTo(vg, px[0], py[0]);
            for (size_t i = 0; i < n - 1; i++) {
                float cx = (px[i] + px[i+1]) / 2.0f;
                nvgBezierTo(vg, cx, py[i], cx, py[i+1], px[i+1], py[i+1]);
            }
        } else {
            for (size_t i = 0; i < n; i++) {
                nvgLineTo(vg, px[i], py[i]);
            }
        }
        
        nvgLineTo(vg, px[n-1], y + h);
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGBA(color.r, color.g, color.b, 50));
        nvgFill(vg);
    }
    
    nvgBeginPath(vg);
    nvgMoveTo(vg, px[0], py[0]);
    
    if (series.smooth && n > 2) {
        for (size_t i = 0; i < n - 1; i++) {
            float cx = (px[i] + px[i+1]) / 2.0f;
            nvgBezierTo(vg, cx, py[i], cx, py[i+1], px[i+1], py[i+1]);
        }
    } else {
        for (size_t i = 1; i < n; i++) {
            nvgLineTo(vg, px[i], py[i]);
        }
    }
    
    nvgStrokeColor(vg, color.toNVG());
    nvgStrokeWidth(vg, series.lineWidth);
    nvgStroke(vg);
    
    if (series.showSymbol) {
        for (size_t i = 0; i < n; i++) {
            bool hovered = ((int)i == hoveredIdx);
            float r = hovered ? series.symbolSize * 1.5f : series.symbolSize;
            
            nvgBeginPath(vg);
            nvgCircle(vg, px[i], py[i], r);
            nvgFillColor(vg, color.toNVG());
            nvgFill(vg);
            
            nvgBeginPath(vg);
            nvgCircle(vg, px[i], py[i], r - 1.5f);
            nvgFillColor(vg, nvgRGB(255, 255, 255));
            nvgFill(vg);
        }
    }
}

} // namespace flexchart
