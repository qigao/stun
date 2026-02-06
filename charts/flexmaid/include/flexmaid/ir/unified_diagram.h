#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace flex {
namespace modules {
namespace flexmaid {

// 统一的图表类型枚举
enum class DiagramType {
    Flowchart,
    Sequence,
    Class,
    State,
    ER,
    Pie,
    Gantt,
    Timeline,
    Journey,
    Mindmap,
    GitGraph,
    XYChart,
    Requirement,
    Sankey,
    Architecture,
    Block,
    C4,
    Kanban,
    Packet,
    Radar,
    Treemap,
    ZenUML,
    Quadrant
};

// 节点形状枚举
enum class NodeShape {
    Rectangle,
    RoundRect,
    Circle,
    Diamond,
    Stadium,
    Subroutine,
    Cylinder,
    Parallelogram,
    Trapezoid,
    ParallelogramAlt,
    TrapezoidAlt,
    CircleFilled,
    DoubleCircle,
    Note,
    Actor,
    Entity,
    Class,
    State
};

// 边样式枚举
enum class EdgeStyle {
    Solid,
    Dotted,
    Thick,
    Dashed
};

// 边装饰枚举
enum class EdgeDecoration {
    None,
    Arrow,
    Circle,
    Cross,
    Diamond,
    DiamondFilled,
    Triangle,
    TriangleFilled
};

// 统一的属性容器
using Properties = std::unordered_map<std::string, std::string>;

// 统一的节点结构
struct Node {
    std::string id;
    std::string label;
    NodeShape shape = NodeShape::Rectangle;
    Properties props;
    
    // 便利方法
    void set_prop(const std::string& key, const std::string& value) {
        props[key] = value;
    }
    
    std::string get_prop(const std::string& key, const std::string& default_val = "") const {
        auto it = props.find(key);
        return it != props.end() ? it->second : default_val;
    }
};

// 统一的边结构
struct Edge {
    std::string from;
    std::string to;
    std::string label;
    EdgeStyle style = EdgeStyle::Solid;
    EdgeDecoration start_decoration = EdgeDecoration::None;
    EdgeDecoration end_decoration = EdgeDecoration::None;
    Properties props;
    
    // 便利方法
    void set_prop(const std::string& key, const std::string& value) {
        props[key] = value;
    }
    
    std::string get_prop(const std::string& key, const std::string& default_val = "") const {
        auto it = props.find(key);
        return it != props.end() ? it->second : default_val;
    }
};

// 子图结构
struct Subgraph {
    std::string id;
    std::string label;
    std::vector<std::string> node_ids;
    Properties props;
    
    void set_prop(const std::string& key, const std::string& value) {
        props[key] = value;
    }
    
    std::string get_prop(const std::string& key, const std::string& default_val = "") const {
        auto it = props.find(key);
        return it != props.end() ? it->second : default_val;
    }
};

// 统一的图表结构
class UnifiedDiagram {
public:
    DiagramType type = DiagramType::Flowchart;
    std::unordered_map<std::string, Node> nodes;
    std::vector<Edge> edges;
    std::vector<Subgraph> subgraphs;
    Properties props;
    
    void set_prop(const std::string& key, const std::string& value) {
        props[key] = value;
    }
    
    std::string get_prop(const std::string& key, const std::string& default_val = "") const {
        auto it = props.find(key);
        return it != props.end() ? it->second : default_val;
    }
    
    Node& ensure_node(const std::string& id, const std::string& label = "", 
                      NodeShape shape = NodeShape::Rectangle) {
        auto it = nodes.find(id);
        if (it != nodes.end()) return it->second;
        
        Node node;
        node.id = id;
        node.label = label.empty() ? id : label;
        node.shape = shape;
        return nodes.emplace(id, std::move(node)).first->second;
    }
    
    Node* find_node(const std::string& id) {
        auto it = nodes.find(id);
        return it != nodes.end() ? &it->second : nullptr;
    }
    
    const Node* find_node(const std::string& id) const {
        auto it = nodes.find(id);
        return it != nodes.end() ? &it->second : nullptr;
    }
    
    std::string get_direction() const { return get_prop("direction", "TD"); }
    void set_direction(const std::string& dir) { set_prop("direction", dir); }
    std::string get_title() const { return get_prop("title", ""); }
    void set_title(const std::string& title) { set_prop("title", title); }
    
    bool is_flowchart() const { return type == DiagramType::Flowchart; }
    bool is_sequence() const { return type == DiagramType::Sequence; }
    bool is_class() const { return type == DiagramType::Class; }
    bool is_state() const { return type == DiagramType::State; }
    bool is_er() const { return type == DiagramType::ER; }
    bool is_pie() const { return type == DiagramType::Pie; }
    bool is_gantt() const { return type == DiagramType::Gantt; }
};

// 工厂函数
std::unique_ptr<UnifiedDiagram> create_diagram(DiagramType type);

// 类型转换辅助函数
std::string diagram_type_to_string(DiagramType type);
DiagramType string_to_diagram_type(const std::string& str);

std::string node_shape_to_string(NodeShape shape);
NodeShape string_to_node_shape(const std::string& str);

std::string edge_style_to_string(EdgeStyle style);
std::string edge_decoration_to_string(EdgeDecoration dec);
EdgeStyle string_to_edge_style(const std::string& str);

} // namespace flexmaid
} // namespace modules
} // namespace flex