#include "flex/runtime/component.h"
#include "flex/runtime/node.h"
#include "flex/runtime/renderer.h"
#include "flowchart_renderer.h"
#include "flowchart/flowchart_parser_wrapper.h"
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
