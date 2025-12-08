#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderAreaStackSeries(NVGcontext* vg, const std::vector<SeriesData>& allSeries,
                           float x, float y, float w, float h,
                           const std::vector<std::string>& labels,
                           int hoveredSeriesIdx, int hoveredDataIdx,
                           const std::vector<Color>& colors) {
    // Filter AreaStack series
    std::vector<const SeriesData*> stackSeries;
    for (const auto& s : allSeries) {
        if (s.type == SeriesType::AreaStack) {
            stackSeries.push_back(&s);
        }
    }
    
    if (stackSeries.empty()) return;
    
    size_t numPoints = 0;
    for (const auto* s : stackSeries) {
        numPoints = std::max(numPoints, s->data.size());
    }
    if (numPoints == 0) return;
    
    // Calculate cumulative values
    std::vector<std::vector<double>> cumulative(stackSeries.size());
    for (size_t si = 0; si < stackSeries.size(); si++) {
        cumulative[si].resize(numPoints, 0);
        for (size_t i = 0; i < numPoints; i++) {
            double val = (i < stackSeries[si]->data.size()) ? stackSeries[si]->data[i] : 0;
            if (si > 0) {
                cumulative[si][i] = cumulative[si - 1][i] + val;
            } else {
                cumulative[si][i] = val;
            }
        }
    }
    
    double maxVal = 0;
    for (const auto& cum : cumulative) {
        for (double v : cum) maxVal = std::max(maxVal, v);
    }
    if (maxVal <= 0) maxVal = 100;
    
    float stepW = w / (numPoints > 1 ? numPoints - 1 : 1);
    
    auto getY = [&](double val) -> float {
        return y + h - h * (val / maxVal);
    };
    
    auto getX = [&](size_t i) -> float {
        return x + i * stepW;
    };
    
    // Draw areas from top to bottom (reverse order)
    for (int si = (int)stackSeries.size() - 1; si >= 0; si--) {
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[si % colors.size()];
        
        nvgBeginPath(vg);
        
        // Top line
        for (size_t i = 0; i < numPoints; i++) {
            float px = getX(i);
            float py = getY(cumulative[si][i]);
            if (i == 0) nvgMoveTo(vg, px, py);
            else nvgLineTo(vg, px, py);
        }
        
        // Bottom line (previous series or baseline)
        for (int i = (int)numPoints - 1; i >= 0; i--) {
            float px = getX(i);
            float py = (si > 0) ? getY(cumulative[si - 1][i]) : y + h;
            nvgLineTo(vg, px, py);
        }
        
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGBA(color.r, color.g, color.b, 180));
        nvgFill(vg);
        
        // Draw top line
        nvgBeginPath(vg);
        for (size_t i = 0; i < numPoints; i++) {
            float px = getX(i);
            float py = getY(cumulative[si][i]);
            if (i == 0) nvgMoveTo(vg, px, py);
            else nvgLineTo(vg, px, py);
        }
        nvgStrokeColor(vg, color.toNVG());
        nvgStrokeWidth(vg, 1.5f);
        nvgStroke(vg);
    }
    
    // Draw labels
    nvgFontSize(vg, 10.0f);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGB(100, 100, 100));
    
    for (size_t i = 0; i < std::min(numPoints, labels.size()); i++) {
        float cx = getX(i);
        nvgText(vg, cx, y + h + 5, labels[i].c_str(), nullptr);
    }
}

} // namespace flexchart
