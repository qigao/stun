#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flexchart {

static void drawSymbol(NVGcontext* vg, const std::string& symbol, 
                       float cx, float cy, float size) {
    if (symbol == "circle") {
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, size / 2);
        nvgFill(vg);
    } else if (symbol == "triangle") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx, cy - size / 2);
        nvgLineTo(vg, cx + size / 2, cy + size / 2);
        nvgLineTo(vg, cx - size / 2, cy + size / 2);
        nvgClosePath(vg);
        nvgFill(vg);
    } else if (symbol == "diamond") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx, cy - size / 2);
        nvgLineTo(vg, cx + size / 2, cy);
        nvgLineTo(vg, cx, cy + size / 2);
        nvgLineTo(vg, cx - size / 2, cy);
        nvgClosePath(vg);
        nvgFill(vg);
    } else if (symbol == "star") {
        nvgBeginPath(vg);
        for (int i = 0; i < 10; i++) {
            float r = (i % 2 == 0) ? size / 2 : size / 4;
            float angle = -M_PI / 2 + i * M_PI / 5;
            float px = cx + std::cos(angle) * r;
            float py = cy + std::sin(angle) * r;
            if (i == 0) nvgMoveTo(vg, px, py);
            else nvgLineTo(vg, px, py);
        }
        nvgClosePath(vg);
        nvgFill(vg);
    } else {
        // Default: rect
        nvgBeginPath(vg);
        nvgRect(vg, cx - size / 2, cy - size / 2, size, size);
        nvgFill(vg);
    }
}

void renderPictorialBarSeries(NVGcontext* vg, const SeriesData& series,
                              float x, float y, float w, float h,
                              const std::vector<std::string>& labels,
                              double minVal, double maxVal, Color color,
                              int hoveredIdx) {
    if (series.data.empty()) return;
    
    size_t n = series.data.size();
    double range = maxVal - minVal;
    if (range <= 0) range = 100;
    
    float barW = w / n;
    float symbolSize = series.pictorialSymbolSize;
    
    for (size_t i = 0; i < n; i++) {
        float cx = x + (i + 0.5f) * barW;
        float barH = h * (series.data[i] - minVal) / range;
        
        bool hovered = ((int)i == hoveredIdx);
        NVGcolor fillColor = hovered ? 
            nvgRGBA(std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30), 255) :
            color.toNVG();
        
        // Calculate number of symbols to fill the bar
        int numSymbols = std::max(1, (int)(barH / (symbolSize + 2)));
        float spacing = barH / numSymbols;
        
        nvgFillColor(vg, fillColor);
        
        for (int j = 0; j < numSymbols; j++) {
            float sy = y + h - (j + 0.5f) * spacing;
            drawSymbol(vg, series.pictorialSymbol, cx, sy, symbolSize * 0.8f);
        }
    }
    
    // Draw labels
    nvgFontSize(vg, 10.0f);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGB(100, 100, 100));
    
    for (size_t i = 0; i < std::min(n, labels.size()); i++) {
        float cx = x + (i + 0.5f) * barW;
        nvgText(vg, cx, y + h + 5, labels[i].c_str(), nullptr);
    }
}

} // namespace flexchart
