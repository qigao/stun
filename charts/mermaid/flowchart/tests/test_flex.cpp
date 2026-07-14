#include "tinytest.h"
#include "flex.h"
#include "flex/runtime/svg.h"
#include "flowchart_renderer.h"

// 外部声明注册函数
namespace flex {
    void register_flowchart_component();
}

spec("flowchart flex adapter") {
    before_all() {
        flex::register_flowchart_component();
    }

    it("returns the standard flex SVG node") {
        flex::Props props;
        props["code"] = "flowchart LR\nA[App]-->|event|B{State Driver}\nB-->|render|C[View]";
        props["width"] = 800.0f;
        props["height"] = 400.0f;
        auto node = flex::create_component_instance("flowchart", props);

        check_not_null(node.get());
        if (!node) return;
        check_int_eq(static_cast<int>(node->type()), static_cast<int>(flex::NodeType::Svg));
        auto svg = std::dynamic_pointer_cast<flex::Svg>(node);
        check_not_null(svg.get());
        if (!svg) return;
        check_string_contains(svg->data(), "<svg");
        check_float_eq(node->layout_width(), 800.0f, 0.001f);
        check_float_eq(node->layout_height(), 400.0f, 0.001f);
    }

    it("rejects invalid routing and dimensions") {
        flex::Props props;
        props["code"] = "flowchart LR\nA-->B";
        props["routingMode"] = std::string("diagonal");
        check_null(flex::create_component_instance("flowchart", props).get());

        props["routingMode"] = std::string("orthogonal");
        props["width"] = -1.0f;
        check_null(flex::create_component_instance("flowchart", props).get());

        props["width"] = 800.0f;
        props["code"] = std::string("flowchart LR\nA[valid] @");
        check_null(flex::create_component_instance("flowchart", props).get());
    }
}
