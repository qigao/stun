#pragma once

#include "../ir/unified_diagram.h"
#include "render_types.h"
#include <string>

namespace flex::modules::flexmaid {

struct Theme;

// Strategy Pattern: Interface for diagram renderers
class IChartRenderer {
public:
    virtual ~IChartRenderer() = default;
    virtual std::string render(const UnifiedDiagram& diagram, const Theme& theme) = 0;
};

// Template Method Pattern: Base class providing common rendering pipeline
class ChartRenderer : public IChartRenderer {
public:
    std::string render(const UnifiedDiagram& diagram, const Theme& theme) override {
        // 1. Layout (Hook)
        LayoutData layout_data = do_layout(diagram, theme);
        
        // 2. SVG Generation (Standard mustache implementation)
        return generate_svg(diagram, layout_data, theme);
    }

protected:
    virtual LayoutData do_layout(const UnifiedDiagram& diagram, const Theme& theme) = 0;
    
    // Shared SVG generation logic
    virtual std::string generate_svg(const UnifiedDiagram& diagram, const LayoutData& layout, const Theme& theme);

    // Context building hooks (sharable across diagrams)
    virtual MustacheContext build_context(const UnifiedDiagram& diagram, const LayoutData& layout, const Theme& theme);
    virtual MustacheNodeData build_node_data(const Node& node, const LayoutData::Bounds& bounds, const UnifiedDiagram& diagram, const Theme& theme);
    virtual MustacheEdgeData build_edge_data(const Edge& edge, const LayoutData::Path& path, const Theme& theme);
    virtual MustacheSubgraphData build_subgraph_data(const Subgraph& sub, const LayoutData& layout, const Theme& theme, float offset_y = 0);
};

} // namespace flex::modules::flexmaid
