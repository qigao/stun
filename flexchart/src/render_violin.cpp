#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderViolinSeries(NVGcontext* vg, const SeriesData& series,
                        float x, float y, float w, float h,
                        int hoveredIdx, const std::vector<Color>& colors) {
    if (series.violinData.empty()) return;
    
    size_t n = series.violinData.size();
    float violinW = w / n;
    
    // Find global range
    double globalMin = 1e9, globalMax = -1e9;
    for (const auto& v : series.violinData) {
        for (double val : v.values) {
            globalMin = std::min(globalMin, val);
            globalMax = std::max(globalMax, val);
        }
    }
    double range = globalMax - globalMin;
    if (range <= 0) range = 100;
    
    for (size_t vi = 0; vi < n; vi++) {
        const auto& v = series.violinData[vi];
        if (v.values.empty()) continue;
        
        float cx = x + (vi + 0.5f) * violinW;
        bool hovered = ((int)vi == hoveredIdx);
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[vi % colors.size()];
        
        // Compute kernel density estimate (simplified)
        int numBins = 20;
        std::vector<double> density(numBins, 0);
        double binWidth = range / numBins;
        
        for (double val : v.values) {
            int bin = std::min((int)((val - globalMin) / binWidth), numBins - 1);
            density[bin]++;
        }
        
        // Smooth density
        std::vector<double> smoothed(numBins, 0);
        for (int i = 0; i < numBins; i++) {
            double sum = density[i] * 2;
            int count = 2;
            if (i > 0) { sum += density[i-1]; count++; }
            if (i < numBins - 1) { sum += density[i+1]; count++; }
            smoothed[i] = sum / count;
        }
        
        // Normalize
        double maxDensity = 0;
        for (double d : smoothed) maxDensity = std::max(maxDensity, d);
        if (maxDensity <= 0) maxDensity = 1;
        
        float maxWidth = violinW * 0.4f;
        
        // Draw violin shape (symmetric)
        nvgBeginPath(vg);
        
        // Right side
        for (int i = 0; i < numBins; i++) {
            float py = y + h - h * (i + 0.5f) / numBins;
            float pw = maxWidth * smoothed[i] / maxDensity;
            if (i == 0) nvgMoveTo(vg, cx + pw, py);
            else nvgLineTo(vg, cx + pw, py);
        }
        
        // Left side (reverse)
        for (int i = numBins - 1; i >= 0; i--) {
            float py = y + h - h * (i + 0.5f) / numBins;
            float pw = maxWidth * smoothed[i] / maxDensity;
            nvgLineTo(vg, cx - pw, py);
        }
        
        nvgClosePath(vg);
        nvgFillColor(vg, hovered ?
            nvgRGBA(std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30), 200) :
            nvgRGBA(color.r, color.g, color.b, 200));
        nvgFill(vg);
        
        nvgStrokeColor(vg, color.toNVG());
        nvgStrokeWidth(vg, 1.5f);
        nvgStroke(vg);
        
        // Draw median line
        std::vector<double> sorted = v.values;
        std::sort(sorted.begin(), sorted.end());
        double median = sorted[sorted.size() / 2];
        float medianY = y + h - h * (median - globalMin) / range;
        
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx - maxWidth * 0.3f, medianY);
        nvgLineTo(vg, cx + maxWidth * 0.3f, medianY);
        nvgStrokeColor(vg, nvgRGB(255, 255, 255));
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);
        
        // Label
        nvgFontSize(vg, 10.0f);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(vg, nvgRGB(80, 80, 80));
        nvgText(vg, cx, y + h + 5, v.name.c_str(), nullptr);
    }
}

} // namespace flexchart
