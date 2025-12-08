#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace flexchart {

void renderSankeySeries(NVGcontext* vg, const SeriesData& series,
                        float x, float y, float w, float h,
                        int hoveredIdx,
                        const std::vector<Color>& colors) {
    if (series.sankeyNodes.empty() || series.sankeyLinks.empty()) return;
    
    // Build node index map
    std::unordered_map<std::string, int> nodeIndex;
    for (size_t i = 0; i < series.sankeyNodes.size(); i++) {
        nodeIndex[series.sankeyNodes[i].name] = (int)i;
    }
    
    // Calculate node levels (simple: sources on left, targets on right)
    std::unordered_map<std::string, int> nodeLevel;
    std::unordered_map<std::string, double> nodeInValue;
    std::unordered_map<std::string, double> nodeOutValue;
    
    for (const auto& link : series.sankeyLinks) {
        nodeOutValue[link.source] += link.value;
        nodeInValue[link.target] += link.value;
    }
    
    // Assign levels
    int maxLevel = 0;
    for (const auto& node : series.sankeyNodes) {
        if (nodeInValue[node.name] == 0) {
            nodeLevel[node.name] = 0;
        } else if (nodeOutValue[node.name] == 0) {
            nodeLevel[node.name] = 2;
            maxLevel = 2;
        } else {
            nodeLevel[node.name] = 1;
            maxLevel = std::max(maxLevel, 1);
        }
    }
    
    // Group nodes by level
    std::vector<std::vector<const SankeyNode*>> levelNodes(maxLevel + 1);
    for (const auto& node : series.sankeyNodes) {
        levelNodes[nodeLevel[node.name]].push_back(&node);
    }
    
    // Calculate positions
    float nodeWidth = 20.0f;
    float levelWidth = (w - nodeWidth) / maxLevel;
    float padding = 10.0f;
    
    std::unordered_map<std::string, float> nodeX;
    std::unordered_map<std::string, float> nodeY;
    std::unordered_map<std::string, float> nodeH;
    
    // Calculate total value for scaling
    double totalValue = 0;
    for (const auto& link : series.sankeyLinks) totalValue += link.value;
    if (totalValue <= 0) return;
    
    // Position nodes
    for (int level = 0; level <= maxLevel; level++) {
        float levelX = x + level * levelWidth;
        float availH = h - padding * 2;
        
        double levelTotal = 0;
        for (const auto* node : levelNodes[level]) {
            double val = std::max(nodeInValue[node->name], nodeOutValue[node->name]);
            if (val == 0) val = node->value;
            levelTotal += val;
        }
        
        float currentY = y + padding;
        for (const auto* node : levelNodes[level]) {
            double val = std::max(nodeInValue[node->name], nodeOutValue[node->name]);
            if (val == 0) val = node->value;
            float nodeHeight = availH * (val / levelTotal) * 0.8f;
            nodeHeight = std::max(10.0f, nodeHeight);
            
            nodeX[node->name] = levelX;
            nodeY[node->name] = currentY;
            nodeH[node->name] = nodeHeight;
            
            currentY += nodeHeight + padding;
        }
    }
    
    // Draw links
    for (size_t i = 0; i < series.sankeyLinks.size(); i++) {
        const auto& link = series.sankeyLinks[i];
        
        float x1 = nodeX[link.source] + nodeWidth;
        float y1 = nodeY[link.source] + nodeH[link.source] / 2;
        float x2 = nodeX[link.target];
        float y2 = nodeY[link.target] + nodeH[link.target] / 2;
        
        float linkH = h * 0.1f * (link.value / totalValue);
        linkH = std::max(2.0f, linkH);
        
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[i % colors.size()];
        
        // Draw curved link
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1, y1 - linkH/2);
        float cx1 = x1 + (x2 - x1) / 3;
        float cx2 = x2 - (x2 - x1) / 3;
        nvgBezierTo(vg, cx1, y1 - linkH/2, cx2, y2 - linkH/2, x2, y2 - linkH/2);
        nvgLineTo(vg, x2, y2 + linkH/2);
        nvgBezierTo(vg, cx2, y2 + linkH/2, cx1, y1 + linkH/2, x1, y1 + linkH/2);
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGBA(color.r, color.g, color.b, 100));
        nvgFill(vg);
    }
    
    // Draw nodes
    for (size_t i = 0; i < series.sankeyNodes.size(); i++) {
        const auto& node = series.sankeyNodes[i];
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[i % colors.size()];
        
        float nx = nodeX[node.name];
        float ny = nodeY[node.name];
        float nh = nodeH[node.name];
        
        nvgBeginPath(vg);
        nvgRect(vg, nx, ny, nodeWidth, nh);
        nvgFillColor(vg, color.toNVG());
        nvgFill(vg);
        
        // Label
        nvgFontSize(vg, 10.0f);
        nvgFontFace(vg, "sans-serif");
        nvgFillColor(vg, nvgRGB(60, 60, 60));
        
        if (nodeLevel[node.name] == 0) {
            nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            nvgText(vg, nx - 5, ny + nh/2, node.name.c_str(), nullptr);
        } else {
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(vg, nx + nodeWidth + 5, ny + nh/2, node.name.c_str(), nullptr);
        }
    }
}

} // namespace flexchart
