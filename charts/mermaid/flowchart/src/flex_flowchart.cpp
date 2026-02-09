#include "flex/runtime/component.h"
#include "flex/runtime/node.h"
#include "flex/runtime/renderer.h"
#include "flowchart_renderer.h"
#include "flowchart/flowchart_parser_wrapper.h"
#include "flex/runtime/text.h"
#include <iostream>

namespace flex {

// 一个简单的封装 Node，用于将 FlowchartRenderer 的输出集成到 Flex
class SvgNode : public Node {
public:
    SvgNode(const std::string& svg_data) : svg_data_(svg_data) {}
    NodeType type() const override { return NodeType::Svg; }
    const char* type_name() const override { return "Svg"; }

    void render(Renderer& renderer) override {
        // 直接利用 Flex Renderer 的绘制能力
        renderer.draw_svg_data(svg_data_, x(), y(), layout_width(), layout_height());
    }

    Bounds compute_bounds() const override {
        return Bounds{0, 0, layout_width(), layout_height()};
    }

private:
    std::string svg_data_;
};

void register_flowchart_component() {
    register_component("flowchart", [](const Props& props) -> std::shared_ptr<Node> {
        std::string code = get_prop_string(props, "code", "graph TD\nA-->B");
        float w = get_prop_float(props, "width", 500.0f);
        float h = get_prop_float(props, "height", 500.0f);

        // 1. 调用布局引擎
        mermaid::flowchart::FlowchartRenderer renderer;
        FlowchartDiagram* diagram = flowchart_parse(code.c_str());
        if (!diagram) {
            return nullptr;
        }

        float font_size = get_prop_float(props, "fontSize", 14.0f);
        std::string font_family = get_prop_string(props, "fontFamily", "Segoe UI");
        float pad_x = get_prop_float(props, "nodePadX", 20.0f);
        float pad_y = get_prop_float(props, "nodePadY", 12.0f);
        std::string routing_mode = get_prop_string(props, "routingMode", "orthogonal");
        if (routing_mode == "polyline") {
            flowchart_set_routing_mode(diagram, FC_ROUTE_POLYLINE);
        } else {
            flowchart_set_routing_mode(diagram, FC_ROUTE_ORTHOGONAL);
        }
        if (props.find("routingShapeBuffer") != props.end()) {
            diagram->routing_shape_buffer = get_prop_float(props, "routingShapeBuffer", -1.0f);
        }
        if (props.find("routingNudgingDistance") != props.end()) {
            diagram->routing_nudging_distance = get_prop_float(props, "routingNudgingDistance", -1.0f);
        }
        if (props.find("routingSegmentPenalty") != props.end()) {
            diagram->routing_segment_penalty = get_prop_float(props, "routingSegmentPenalty", -1.0f);
        }
        if (props.find("routingAnglePenalty") != props.end()) {
            diagram->routing_angle_penalty = get_prop_float(props, "routingAnglePenalty", -1.0f);
        }
        if (props.find("routingCrossingPenalty") != props.end()) {
            diagram->routing_crossing_penalty = get_prop_float(props, "routingCrossingPenalty", -1.0f);
        }
        if (props.find("routingNudgeOrthogonalEnds") != props.end()) {
            diagram->routing_nudge_orthogonal_ends = get_prop_bool(props, "routingNudgeOrthogonalEnds", false) ? 1 : 0;
        }
        if (props.find("routingNudgeSharedPaths") != props.end()) {
            diagram->routing_nudge_shared_paths = get_prop_bool(props, "routingNudgeSharedPaths", true) ? 1 : 0;
        }

        for (FlowchartNode* n = diagram->nodes; n; n = n->next) {
            flex::Text t;
            t.set_content(n->label ? n->label : n->id);
            t.set_font_family(font_family);
            t.set_font_size(font_size);
            float tw = t.measured_width();
            float th = t.measured_height();
            flowchart_set_node_size(diagram, n->id, tw + pad_x, th + pad_y);
        }

        auto snapshot = renderer.layout(diagram);
        std::string svg = mermaid::flowchart::FlowchartRenderer::to_svg(snapshot);
        
        flowchart_diagram_free(diagram);

        // 2. 创建并返回 SvgNode
        auto node = std::make_shared<SvgNode>(svg);
        node->set_layout_size(w, h);
        return node;
    });
}

} // namespace flex
