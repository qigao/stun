#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flexchart {

void renderGraphSeries(NVGcontext* vg, const SeriesData& series,
                       float x, float y, float w, float h,
                       int hoveredIdx,
                       const std::vector<Color>& colors) {
    if (series.graphNodes.empty()) return;
    
    // Build node index
    std::unordered_map<std::string, size_t> nodeIndex;
    for (size_t i = 0; i < series.graphNodes.size(); i++) {
        nodeIndex[series.graphNodes[i].name] = i;
    }
    
    // Calculate positions (simple circular layout if no positions specified)
    std::vector<float> px(series.graphNodes.size());
    std::vector<float> py(series.graphNodes.size());
    
    float cx = x + w / 2;
    float cy = y + h / 2;
    float radius = std::min(w, h) * 0.35f;
    
    bool hasPositions = false;
    for (const auto& n : series.graphNodes) {
        if (n.x != 0 || n.y != 0) { hasPositions = true; break; }
    }
    
    if (hasPositions) {
        // Find bounds
        double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
        for (const auto& n : series.graphNodes) {
            minX = std::min(minX, n.x);
            maxX = std::max(maxX, n.x);
            minY = std::min(minY, n.y);
            maxY = std::max(maxY, n.y);
        }
        double rangeX = maxX - minX > 0 ? maxX - minX : 1;
        double rangeY = maxY - minY > 0 ? maxY - minY : 1;
        
        for (size_t i = 0; i < series.graphNodes.size(); i++) {
            px[i] = x + 30 + (w - 60) * (series.graphNodes[i].x - minX) / rangeX;
            py[i] = y + 30 + (h - 60) * (series.graphNodes[i].y - minY) / rangeY;
        }
    } else {
        // Circular layout
        size_t n = series.graphNodes.size();
        for (size_t i = 0; i < n; i++) {
            float angle = -M_PI / 2 + 2 * M_PI * i / n;
            px[i] = cx + std::cos(angle) * radius;
            py[i] = cy + std::sin(angle) * radius;
        }
    }
    
    // Draw links
    for (const auto& link : series.graphLinks) {
        auto srcIt = nodeIndex.find(link.source);
        auto tgtIt = nodeIndex.find(link.target);
        if (srcIt == nodeIndex.end() || tgtIt == nodeIndex.end()) continue;
        
        size_t si = srcIt->second;
        size_t ti = tgtIt->second;
        
        nvgBeginPath(vg);
        nvgMoveTo(vg, px[si], py[si]);
        nvgLineTo(vg, px[ti], py[ti]);
        nvgStrokeColor(vg, nvgRGBA(180, 180, 180, 150));
        nvgStrokeWidth(vg, std::max(1.0f, (float)link.value * 0.5f));
        nvgStroke(vg);
    }
    
    // Draw nodes
    for (size_t i = 0; i < series.graphNodes.size(); i++) {
        const auto& node = series.graphNodes[i];
        bool hovered = ((int)i == hoveredIdx);
        
        Color color = colors.empty() ? Color{91, 143, 249, 255} : 
                      colors[node.category % colors.size()];
        
        float r = std::max(5.0f, (float)node.value * 0.5f);
        if (hovered) r *= 1.3f;
        
        nvgBeginPath(vg);
        nvgCircle(vg, px[i], py[i], r);
        nvgFillColor(vg, color.toNVG());
        nvgFill(vg);
        
        if (hovered) {
            nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 200));
            nvgStrokeWidth(vg, 2.0f);
            nvgStroke(vg);
        }
        
        // Label
        nvgFontSize(vg, 10.0f);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(vg, nvgRGB(80, 80, 80));
        nvgText(vg, px[i], py[i] + r + 3, node.name.c_str(), nullptr);
    }
}

} // namespace flexchart
