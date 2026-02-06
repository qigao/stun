#pragma once

#include "ir/unified_diagram.h"
#include "parser/unified_parser.h"
#include "render/render_types.h"
#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <unordered_map>

namespace flex {
class Instance;
class Group;
}

namespace flex {
namespace modules {
namespace flexmaid {

class DagreLayoutEngine;
class IChartRenderer;
class IChartParser;

using Layouter = std::function<void(const UnifiedDiagram&, LayoutData&, const Theme&)>;

class FlexMaid {
public:
    FlexMaid();
    ~FlexMaid();
    
    ParseResult parse(const std::string& mermaid_text);
    LayoutResult layout(const UnifiedDiagram& diagram);
    std::string render_svg(const UnifiedDiagram& diagram);
    std::string render_svg(const UnifiedDiagram& diagram, const LayoutData& layout);
    std::string mermaid_to_svg(const std::string& mermaid_text);
    
    flex::Group* to_flex(const UnifiedDiagram& diagram, flex::Instance& instance);
    flex::Group* to_flex(std::string_view source, flex::Instance& instance);
    
    void set_theme(const Theme& theme);
    const Theme& get_theme() const;
    
    void register_layouter(DiagramType type, Layouter layouter);
    void register_renderer(DiagramType type, std::unique_ptr<IChartRenderer> renderer);
    
private:
    std::string render(const UnifiedDiagram& diagram);
    UnifiedParser parser_;
    Theme theme_;
    std::unordered_map<DiagramType, Layouter> layouters_;
    std::unordered_map<DiagramType, std::unique_ptr<IChartRenderer>> renderers_;
    std::unique_ptr<DagreLayoutEngine> dagre_engine_;
    
    static void layout_flowchart(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);
    static void layout_sequence(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);
    static void layout_class(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);
    static void layout_pie(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);
    static void layout_gitgraph(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);
    
    std::pair<float, float> calculate_text_size(const std::string& text) const;
    std::pair<float, float> calculate_node_size(const std::string& label, NodeShape shape) const;
    static std::string escape_xml(const std::string& text);
};
} // namespace flexmaid
} // namespace modules
} // namespace flex
