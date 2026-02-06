#include <ir/unified_diagram.h>
#include <stdexcept>

namespace flex::modules::flexmaid {

std::unique_ptr<UnifiedDiagram> create_diagram(DiagramType type) {
    auto diagram = std::make_unique<UnifiedDiagram>();
    diagram->type = type;
    
    // 设置类型特定的默认属性
    switch (type) {
        case DiagramType::Flowchart:
            diagram->set_prop("direction", "TD");
            break;
        case DiagramType::Sequence:
            diagram->set_prop("direction", "TB");
            break;
        case DiagramType::Class:
            diagram->set_prop("direction", "TB");
            break;
        case DiagramType::State:
            diagram->set_prop("direction", "TB");
            break;
        case DiagramType::ER:
            diagram->set_prop("direction", "TB");
            break;
        case DiagramType::Architecture:
            diagram->set_prop("direction", "LR");
            break;
        case DiagramType::Pie:
            diagram->set_prop("show_data", "false");
            break;
        case DiagramType::Gantt:
            diagram->set_prop("date_format", "YYYY-MM-DD");
            diagram->set_prop("axis_format", "%m/%d");
            break;
        case DiagramType::Block:
            diagram->set_prop("direction", "LR");
            break;
        default:
            diagram->set_prop("direction", "TD");
            break;
    }
    
    return diagram;
}

std::string diagram_type_to_string(DiagramType type) {
    switch (type) {
        case DiagramType::Flowchart: return "flowchart";
        case DiagramType::Sequence: return "sequenceDiagram";
        case DiagramType::Class: return "classDiagram";
        case DiagramType::State: return "stateDiagram";
        case DiagramType::ER: return "erDiagram";
        case DiagramType::Pie: return "pie";
        case DiagramType::Gantt: return "gantt";
        case DiagramType::Timeline: return "timeline";
        case DiagramType::Journey: return "journey";
        case DiagramType::Mindmap: return "mindmap";
        case DiagramType::GitGraph: return "gitGraph";
        case DiagramType::XYChart: return "xychart";
        case DiagramType::Requirement: return "requirementDiagram";
        case DiagramType::Sankey: return "sankey";
        case DiagramType::Architecture: return "architecture";
        case DiagramType::Block: return "block";
        case DiagramType::C4: return "C4";
        case DiagramType::Kanban: return "kanban";
        case DiagramType::Packet: return "packet";
        case DiagramType::Radar: return "radar";
        case DiagramType::Treemap: return "treemap";
        case DiagramType::ZenUML: return "zenuml";
        case DiagramType::Quadrant: return "quadrant";
        default: return "unknown";
    }
}

DiagramType string_to_diagram_type(const std::string& str) {
    if (str == "flowchart" || str == "graph") return DiagramType::Flowchart;
    if (str == "sequenceDiagram") return DiagramType::Sequence;
    if (str == "classDiagram") return DiagramType::Class;
    if (str == "stateDiagram" || str == "stateDiagram-v2") return DiagramType::State;
    if (str == "erDiagram") return DiagramType::ER;
    if (str == "pie") return DiagramType::Pie;
    if (str == "gantt") return DiagramType::Gantt;
    if (str == "timeline") return DiagramType::Timeline;
    if (str == "journey") return DiagramType::Journey;
    if (str == "mindmap") return DiagramType::Mindmap;
    if (str == "gitGraph") return DiagramType::GitGraph;
    if (str == "xychart" || str == "xychart-beta") return DiagramType::XYChart;
    if (str == "requirementDiagram") return DiagramType::Requirement;
    if (str == "sankey" || str == "sankey-beta") return DiagramType::Sankey;
    if (str == "architecture" || str == "architecture-beta") return DiagramType::Architecture;
    if (str == "block" || str == "block-beta") return DiagramType::Block;
    if (str == "C4" || str == "C4Context" || str == "C4Container" || str == "C4Component" || str == "C4Dynamic" || str == "C4Deployment") return DiagramType::C4;
    if (str == "kanban") return DiagramType::Kanban;
    if (str == "packet" || str == "packet-beta") return DiagramType::Packet;
    if (str == "radar" || str == "radar-beta") return DiagramType::Radar;
    if (str == "treemap" || str == "treemap-beta") return DiagramType::Treemap;
    if (str == "zenuml") return DiagramType::ZenUML;
    if (str == "quadrant" || str == "quadrantChart") return DiagramType::Quadrant;
    
    throw std::invalid_argument("Unknown diagram type: " + str);
}

std::string node_shape_to_string(NodeShape shape) {
    switch (shape) {
        case NodeShape::Rectangle: return "rectangle";
        case NodeShape::RoundRect: return "round_rect";
        case NodeShape::Circle: return "circle";
        case NodeShape::Diamond: return "diamond";
        case NodeShape::Stadium: return "stadium";
        case NodeShape::Subroutine: return "subroutine";
        case NodeShape::Cylinder: return "cylinder";
        case NodeShape::Parallelogram: return "parallelogram";
        case NodeShape::Trapezoid: return "trapezoid";
        case NodeShape::ParallelogramAlt: return "parallelogram_alt";
        case NodeShape::TrapezoidAlt: return "trapezoid_alt";
        case NodeShape::CircleFilled: return "circle_filled";
        case NodeShape::DoubleCircle: return "double_circle";
        case NodeShape::Note: return "note";
        case NodeShape::Actor: return "actor";
        case NodeShape::Entity: return "entity";
        case NodeShape::Class: return "class";
        case NodeShape::State: return "state";
        default: return "rectangle";
    }
}

NodeShape string_to_node_shape(const std::string& str) {
    if (str == "rectangle") return NodeShape::Rectangle;
    if (str == "round_rect") return NodeShape::RoundRect;
    if (str == "circle") return NodeShape::Circle;
    if (str == "diamond") return NodeShape::Diamond;
    if (str == "stadium") return NodeShape::Stadium;
    if (str == "subroutine") return NodeShape::Subroutine;
    if (str == "cylinder") return NodeShape::Cylinder;
    if (str == "parallelogram") return NodeShape::Parallelogram;
    if (str == "trapezoid") return NodeShape::Trapezoid;
    if (str == "parallelogram_alt") return NodeShape::ParallelogramAlt;
    if (str == "trapezoid_alt") return NodeShape::TrapezoidAlt;
    if (str == "circle_filled") return NodeShape::CircleFilled;
    if (str == "double_circle") return NodeShape::DoubleCircle;
    if (str == "note") return NodeShape::Note;
    if (str == "actor") return NodeShape::Actor;
    if (str == "entity") return NodeShape::Entity;
    if (str == "class") return NodeShape::Class;
    if (str == "state") return NodeShape::State;
    
    return NodeShape::Rectangle; // 默认值
}

std::string edge_style_to_string(EdgeStyle style) {
    switch (style) {
        case EdgeStyle::Solid: return "solid";
        case EdgeStyle::Dotted: return "dotted";
        case EdgeStyle::Thick: return "thick";
        case EdgeStyle::Dashed: return "dashed";
        default: return "solid";
    }
}

std::string edge_decoration_to_string(EdgeDecoration dec) {
    switch (dec) {
        case EdgeDecoration::None: return "none";
        case EdgeDecoration::Arrow: return "arrow";
        case EdgeDecoration::Triangle: return "triangle";
        case EdgeDecoration::Diamond: return "diamond";
        case EdgeDecoration::DiamondFilled: return "diamond_filled";
        case EdgeDecoration::Circle: return "circle";
        case EdgeDecoration::Cross: return "cross";
        default: return "none";
    }
}

EdgeStyle string_to_edge_style(const std::string& str) {
    if (str == "solid") return EdgeStyle::Solid;
    if (str == "dotted") return EdgeStyle::Dotted;
    if (str == "thick") return EdgeStyle::Thick;
    if (str == "dashed") return EdgeStyle::Dashed;
    
    return EdgeStyle::Solid; // 默认值
}

} // namespace flex::modules::flexmaid