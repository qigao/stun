#pragma once

#include <flex/modules/flexmaid/ir/unified_diagram.h>
#include <flex/modules/flexmaid/parser/unified_parser.h>
#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <unordered_map>

namespace flex {
class Instance;
class Group;
}

namespace flex::modules::flexmaid {

// 布局数据 - 只存储计算出的坐标，不复制原始数据
struct LayoutData {
    // 节点边界
    struct Bounds {
        float x = 0, y = 0;
        float width = 0, height = 0;
        
        float left() const { return x - width / 2; }
        float right() const { return x + width / 2; }
        float top() const { return y - height / 2; }
        float bottom() const { return y + height / 2; }
    };
    
    // 边路径
    struct Path {
        std::vector<std::pair<float, float>> points;
    };
    
    std::unordered_map<std::string, Bounds> node_bounds;  // node.id -> bounds
    std::vector<Path> edge_paths;                          // 与 diagram.edges 索引对应
    float width = 0;
    float height = 0;
};

// 布局结果
struct LayoutResult {
    bool success = false;
    std::string error;
    LayoutData data;
};

// 主题
struct Theme {
    std::string background_color = "#ffffff";
    std::string primary_color = "#0066cc";
    std::string secondary_color = "#666666";
    std::string text_color = "#333333";
    std::string line_color = "#333333";
    std::string font_family = "Arial, sans-serif";
    int font_size = 14;
    float line_width = 2.0f;
    
    static Theme light();
    static Theme dark();
    static Theme modern();
};

// 布局器接口
using Layouter = std::function<void(const UnifiedDiagram&, LayoutData&, const Theme&)>;

// 简洁的 FlexMaid API
class FlexMaid {
public:
    FlexMaid();
    ~FlexMaid() = default;
    
    // 解析
    ParseResult parse(const std::string& mermaid_text);
    
    // 布局
    LayoutResult layout(const UnifiedDiagram& diagram);
    
    // 渲染 - 只需要 diagram，布局数据从 diagram 获取或重新计算
    std::string render_svg(const UnifiedDiagram& diagram);
    std::string render_svg(const UnifiedDiagram& diagram, const LayoutData& layout);
    
    // 一步到位
    std::string mermaid_to_svg(const std::string& mermaid_text);
    
    // Flex 运行时渲染 - 与 FlexChart/FlexInfographic 统一
    flex::Group* to_flex(const UnifiedDiagram& diagram, flex::Instance& instance);
    flex::Group* to_flex(std::string_view source, flex::Instance& instance);
    
    // 主题
    void set_theme(const Theme& theme);
    const Theme& get_theme() const;
    
    // 布局器注册 - 可扩展
    void register_layouter(DiagramType type, Layouter layouter);
    
private:
    UnifiedParser parser_;
    Theme theme_;
    std::unordered_map<DiagramType, Layouter> layouters_;
    
    // 默认布局器
    static void layout_flowchart(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);
    static void layout_sequence(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);
    static void layout_class(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);
    static void layout_pie(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);
    static void layout_gitgraph(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);
    
    // 辅助
    std::pair<float, float> calculate_text_size(const std::string& text) const;
    std::pair<float, float> calculate_node_size(const std::string& label, NodeShape shape) const;
    static std::string escape_xml(const std::string& text);
};

} // namespace flex::modules::flexmaid
