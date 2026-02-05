#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace flex::modules::flexmaid {

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
};

// 统一的图表结构
class UnifiedDiagram {
public:
    DiagramType type = DiagramType::Flowchart;
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<Subgraph> subgraphs;
    Properties props;
    
    // 便利方法
    void set_prop(const std::string& key, const std::string& value) {
        props[key] = value;
    }
    
    std::string get_prop(const std::string& key, const std::string& default_val = "") const {
        auto it = props.find(key);
        return it != props.end() ? it->second : default_val;
    }
    
    // 添加节点
    void add_node(const std::string& id, const std::string& label = "", 
                  NodeShape shape = NodeShape::Rectangle) {
        Node node;
        node.id = id;
        node.label = label.empty() ? id : label;
        node.shape = shape;
        nodes.push_back(std::move(node));
    }
    
    // 添加边
    void add_edge(const std::string& from, const std::string& to, 
                  const std::string& label = "", EdgeStyle style = EdgeStyle::Solid) {
        Edge edge;
        edge.from = from;
        edge.to = to;
        edge.label = label;
        edge.style = style;
        edges.push_back(std::move(edge));
    }
    
    // 查找节点
    Node* find_node(const std::string& id) {
        for (auto& node : nodes) {
            if (node.id == id) return &node;
        }
        return nullptr;
    }
    
    const Node* find_node(const std::string& id) const {
        for (const auto& node : nodes) {
            if (node.id == id) return &node;
        }
        return nullptr;
    }
    
    // 确保节点存在
    Node& ensure_node(const std::string& id, const std::string& label = "", 
                      NodeShape shape = NodeShape::Rectangle) {
        if (auto* node = find_node(id)) {
            return *node;
        }
        add_node(id, label, shape);
        return nodes.back();
    }
    
    // 类型特定的便利方法
    bool is_flowchart() const { return type == DiagramType::Flowchart; }
    bool is_sequence() const { return type == DiagramType::Sequence; }
    bool is_class() const { return type == DiagramType::Class; }
    bool is_state() const { return type == DiagramType::State; }
    bool is_er() const { return type == DiagramType::ER; }
    bool is_pie() const { return type == DiagramType::Pie; }
    bool is_gantt() const { return type == DiagramType::Gantt; }
    
    // 获取方向
    std::string get_direction() const {
        return get_prop("direction", "TD");
    }
    
    void set_direction(const std::string& direction) {
        set_prop("direction", direction);
    }
    
    // 获取标题
    std::string get_title() const {
        return get_prop("title", "");
    }
    
    void set_title(const std::string& title) {
        set_prop("title", title);
    }
};

// 工厂函数
std::unique_ptr<UnifiedDiagram> create_diagram(DiagramType type);

// 类型转换辅助函数
std::string diagram_type_to_string(DiagramType type);
DiagramType string_to_diagram_type(const std::string& str);

std::string node_shape_to_string(NodeShape shape);
NodeShape string_to_node_shape(const std::string& str);

std::string edge_style_to_string(EdgeStyle style);
EdgeStyle string_to_edge_style(const std::string& str);

} // namespace flex::modules::flexmaid