#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flexchart {

static double calcTotalValue(const std::vector<SunburstData>& nodes) {
    double total = 0;
    for (const auto& n : nodes) {
        if (n.children.empty()) {
            total += n.value;
        } else {
            total += calcTotalValue(n.children);
        }
    }
    return total;
}

static void renderSunburstRing(NVGcontext* vg, const std::vector<SunburstData>& nodes,
                               float cx, float cy, float innerR, float outerR,
                               float startAngle, float sweepAngle,
                               const std::vector<Color>& colors, int depth, int& colorIdx) {
    if (nodes.empty() || outerR <= innerR) return;
    
    double total = calcTotalValue(nodes);
    if (total <= 0) return;
    
    float currentAngle = startAngle;
    
    for (const auto& node : nodes) {
        double nodeValue = node.children.empty() ? node.value : calcTotalValue(node.children);
        float nodeSweep = sweepAngle * (nodeValue / total);
        
        if (nodeSweep > 0.01f) {
            Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[colorIdx % colors.size()];
            
            // Lighten for deeper levels
            if (depth > 0) {
                color.r = std::min(255, color.r + depth * 15);
                color.g = std::min(255, color.g + depth * 15);
                color.b = std::min(255, color.b + depth * 15);
            }
            
            // Draw arc segment
            nvgBeginPath(vg);
            nvgArc(vg, cx, cy, outerR, currentAngle, currentAngle + nodeSweep, NVG_CW);
            nvgArc(vg, cx, cy, innerR, currentAngle + nodeSweep, currentAngle, NVG_CCW);
            nvgClosePath(vg);
            nvgFillColor(vg, color.toNVG());
            nvgFill(vg);
            
            // Border
            nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 200));
            nvgStrokeWidth(vg, 1.0f);
            nvgStroke(vg);
            
            // Label for large segments
            if (nodeSweep > 0.3f && (outerR - innerR) > 20) {
                float midAngle = currentAngle + nodeSweep / 2;
                float labelR = (innerR + outerR) / 2;
                float lx = cx + std::cos(midAngle) * labelR;
                float ly = cy + std::sin(midAngle) * labelR;
                
                nvgFontSize(vg, std::min(11.0f, (outerR - innerR) * 0.4f));
                nvgFontFace(vg, "sans-serif");
                nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgFillColor(vg, nvgRGB(255, 255, 255));
                nvgText(vg, lx, ly, node.name.c_str(), nullptr);
            }
            
            // Recursively render children
            if (!node.children.empty()) {
                float ringWidth = (outerR - innerR) * 0.8f;
                renderSunburstRing(vg, node.children, cx, cy, 
                                  outerR + 2, outerR + 2 + ringWidth,
                                  currentAngle, nodeSweep, colors, depth + 1, colorIdx);
            }
        }
        
        colorIdx++;
        currentAngle += nodeSweep;
    }
}

void renderSunburstSeries(NVGcontext* vg, const SeriesData& series,
                          float cx, float cy, float radius,
                          int hoveredIdx,
                          const std::vector<Color>& colors) {
    if (series.sunburstData.empty()) return;
    
    float ringWidth = radius * 0.25f;
    float innerR = radius * 0.2f;
    
    int colorIdx = 0;
    renderSunburstRing(vg, series.sunburstData, cx, cy, innerR, innerR + ringWidth,
                      -M_PI / 2, 2 * M_PI, colors, 0, colorIdx);
}

} // namespace flexchart
