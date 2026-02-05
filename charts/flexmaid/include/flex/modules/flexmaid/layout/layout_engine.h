#pragma once

#include "../ir/diagram.h"
#include <flex/runtime/geometry.h>
#include <vector>

namespace flex::modules::flexmaid {

struct LayoutConfig {
    float node_spacing = 50.0f;
    float rank_spacing = 50.0f;
    float edge_spacing = 10.0f;
    float subgraph_padding = 20.0f;
    float min_node_width = 60.0f;
    float min_node_height = 40.0f;
    
    // Sequence diagram specific
    struct {
        float actor_spacing = 150.0f;
        float message_spacing = 40.0f;
        float actor_height = 60.0f;
    } sequence;
};

struct NodeLayout {
    std::string id;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    NodeShape shape = NodeShape::Rectangle;
    std::vector<std::string> label_lines;
    struct ClassInfo {
        std::vector<std::string> attributes;
        std::vector<std::string> methods;
    };
    std::optional<ClassInfo> class_info;
    struct ERInfo {
        struct Attribute {
            std::string type;
            std::string name;
            std::string key;
        };
        std::vector<Attribute> attributes;
    };
    std::optional<ERInfo> er_info;
    NodeStyle style;
};

struct EdgeLayout {
    std::string from;
    std::string to;
    std::vector<flex::Vec2> points;  // Routing waypoints
    std::optional<std::string> label;
    std::optional<std::string> start_label;
    std::optional<std::string> end_label;
    bool arrow_start = false;
    bool arrow_end = true;
    EdgeDecoration start_decoration = EdgeDecoration::None;
    EdgeDecoration end_decoration = EdgeDecoration::None;
    EdgeStyle style = EdgeStyle::Solid;
};

struct SubgraphLayout {
    std::string label;
    std::vector<std::string> label_lines;
    std::vector<std::string> nodes;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    NodeStyle style;
};

struct DiagramLayout {
    DiagramKind kind = DiagramKind::Flowchart;
    std::vector<NodeLayout> nodes;
    std::vector<EdgeLayout> edges;
    std::vector<SubgraphLayout> subgraphs;
    float width = 0.0f;
    float height = 0.0f;
    
    // Sequence diagram specific
    struct SequenceData {
        struct Lifeline {
            std::string id;
            float x = 0.0f;
            float y1 = 0.0f;
            float y2 = 0.0f;
        };
        std::vector<Lifeline> lifelines;
        
        struct Activation {
            std::string participant;
            float x = 0.0f;
            float y = 0.0f;
            float width = 0.0f;
            float height = 0.0f;
        };
        std::vector<Activation> activations;
    } sequence;

    struct PieData {
        struct Slice {
            std::string label;
            float value;
            float percentage;
            float start_angle;
            float end_angle;
            float label_x;
            float label_y;
            std::string path_d;
        };
        std::string title;
        std::vector<Slice> slices;
        float radius = 0.0f;
        float center_x = 0.0f;
        float center_y = 0.0f;
    } pie;

    struct GanttData {
        struct Task {
            std::string label;
            std::string status;
            float x, y, width, height;
            std::string start_label;
            std::string end_label;
        };
        struct Section {
            std::string label;
            float y, height;
        };
        struct AxisTick {
            std::string label;
            float x;
        };
        std::string title;
        std::vector<Section> sections;
        std::vector<Task> tasks;
        std::vector<AxisTick> axis_ticks;
        float header_height = 0.0f;
    } gantt;

    struct TimelineData {
        struct Event {
            std::string label;
            float x, y;
        };
        struct Period {
            std::string label;
            float x, width;
            std::vector<Event> events;
        };
        std::string title;
        std::vector<Period> periods;
    } timeline;

    struct JourneyData {
        struct Task {
            std::string label;
            int score;
            std::vector<std::string> actors;
            float x, y, width, height;
        };
        struct Section {
            std::string label;
            float y, height;
        };
        struct Actor {
            std::string label;
            float x;
        };
        std::string title;
        std::vector<Section> sections;
        std::vector<Task> tasks;
        std::vector<Actor> actors;
    } journey;

    struct MindmapData {
        struct Node {
            std::string label;
            float x, y, width, height;
            int level;
        };
        struct Edge {
            float x1, y1, x2, y2;
        };
        std::vector<Node> nodes;
        std::vector<Edge> edges;
    } mindmap;

    struct GitGraphData {
        struct Commit {
            std::string id;
            float x, y;
            std::string branch;
        };
        struct Branch {
            std::string name;
            float y; // Branch line Y position
            float end_x;
        };
        struct Connection {
             float x1, y1, x2, y2;
             bool is_merge;
        };
        std::vector<Commit> commits;
        std::vector<Branch> branches;
        std::vector<Connection> connections;
    } gitgraph;

    struct RequirementData {
        struct Requirement {
            std::string name;
            std::string type;
            std::string id_str;
            std::string text;
            std::string risk;
            std::string verification;
            float x, y, width, height;
        };
        struct Element {
            std::string name;
            std::string type;
            std::string doc_ref;
            float x, y, width, height;
        };
        struct Relationship {
            std::string type;
            std::string from;
            std::string to;
            std::vector<flex::Vec2> points; // For routing
        };
        std::vector<Requirement> requirements;
        std::vector<Element> elements;
        std::vector<Relationship> relationships;
    } requirement;

    struct XYChartData {
        std::string title;
        float title_x, title_y;
        
        struct AxisLayout {
            std::string title;
            float min = 0, max = 100;
            struct Label {
                std::string label;
                float x, y;
            };
            std::vector<Label> labels;
        } x_axis, y_axis;
        
        float axis_left, axis_right, axis_top, axis_bottom;
        
        struct PlotData {
            std::string label;
            enum { Bar, Line } type;
            struct BarRect {
                float x, y, width, height, value;
                std::string label;
            };
            std::vector<BarRect> bars;
            std::string line_points;
            struct Dot { float cx, cy; };
            std::vector<Dot> dots;
        };
        std::vector<PlotData> plots;
    } xychart;

    struct SankeyData {
        struct Link {
            std::string source;
            std::string target;
            float value;
            // Visual path data will be calculated by layout
            std::string path_d;
        };
        std::vector<Link> links;
        // Nodes with their calculated positions
        struct Node {
            std::string id;
            float x, y, width, height;
        };
        std::vector<Node> nodes;
    } sankey;
};

class LayoutEngine {
public:
    virtual ~LayoutEngine() = default;
    
    /// Compute layout for a diagram
    virtual DiagramLayout compute(const Diagram& diagram, const LayoutConfig& config) = 0;
    
    /// Create appropriate layout engine for diagram type
    static std::unique_ptr<LayoutEngine> create(DiagramKind kind);
};

class FlowchartLayoutEngine : public LayoutEngine {
public:
    DiagramLayout compute(const Diagram& diagram, const LayoutConfig& config) override;
    
private:
    struct GraphNode {
        std::string id;
        float width = 0.0f;
        float height = 0.0f;
        int layer = 0;
        int position = 0;
        float x = 0.0f;
        float y = 0.0f;
    };
    
    struct GraphEdge {
        std::string from;
        std::string to;
        std::vector<flex::Vec2> points;
    };
    
    void assign_layers(std::vector<GraphNode>& nodes, const std::vector<Edge>& edges);
    void minimize_crossings(std::vector<GraphNode>& nodes, const std::vector<Edge>& edges);
    void assign_coordinates(std::vector<GraphNode>& nodes, const LayoutConfig& config);
    void route_edges(std::vector<GraphEdge>& edges, const std::vector<GraphNode>& nodes);
};

class SequenceLayoutEngine : public LayoutEngine {
public:
    DiagramLayout compute(const Diagram& diagram, const LayoutConfig& config) override;
};

class ClassLayoutEngine : public LayoutEngine {
public:
    DiagramLayout compute(const Diagram& diagram, const LayoutConfig& config) override;
};

class ERLayoutEngine : public LayoutEngine {
public:
    DiagramLayout compute(const Diagram& diagram, const LayoutConfig& config) override;
};

class PieLayoutEngine : public LayoutEngine {
public:
    DiagramLayout compute(const Diagram& diagram, const LayoutConfig& config) override;
};

class GanttLayoutEngine : public LayoutEngine {
public:
    DiagramLayout compute(const Diagram& diagram, const LayoutConfig& config) override;
};

class TimelineLayoutEngine : public LayoutEngine {
public:
    DiagramLayout compute(const Diagram& diagram, const LayoutConfig& config) override;
};

class JourneyLayoutEngine : public LayoutEngine {
public:
    DiagramLayout compute(const Diagram& diagram, const LayoutConfig& config) override;
};

class MindmapLayoutEngine : public LayoutEngine {
public:
    DiagramLayout compute(const Diagram& diagram, const LayoutConfig& config) override;
};

class GitGraphLayoutEngine : public LayoutEngine {
public:
    DiagramLayout compute(const Diagram& diagram, const LayoutConfig& config) override;
};

class RequirementLayoutEngine : public LayoutEngine {
public:
    DiagramLayout compute(const Diagram& diagram, const LayoutConfig& config) override;
};

class XYChartLayoutEngine : public LayoutEngine {
public:
    DiagramLayout compute(const Diagram& diagram, const LayoutConfig& config) override;
};

class SankeyLayoutEngine : public LayoutEngine {
public:
    DiagramLayout compute(const Diagram& diagram, const LayoutConfig& config) override;
};

} // namespace flex::modules::flexmaid
