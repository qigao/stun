#include <iostream>
#include "flex.h"
#include "flowchart_renderer.h"

// 外部声明注册函数
namespace flex {
    void register_flowchart_component();
}

int main() {
    // 1. 注册核心组件和我们的新组件
    flex::register_flowchart_component();

    // 2. 模拟一个 UI 系统加载过过程
    auto& registry = flex::ComponentRegistry::instance();
    if (registry.has("flowchart")) {
        std::cout << "Flowchart component registered successfully!" << std::endl;
    }

    // 3. 实例化组件
    flex::Props props;
    props["code"] = "flowchart LR\nA[App]-->|event|B{State Driver}\nB-->|render|C[View]";
    props["width"] = 800.0f;
    props["height"] = 400.0f;

    auto node = flex::create_component_instance("flowchart", props);

    if (node) {
        std::cout << "Successfully instantiated flowchart node." << std::endl;
        std::cout << "Node type: " << node->type_name() << std::endl;
        std::cout << "Initial position: (" << node->x() << ", " << node->y() << ")" << std::endl;
        std::cout << "Layout size: " << node->layout_width() << "x" << node->layout_height() << std::endl;
    } else {
        std::cerr << "Failed to instantiate flowchart node." << std::endl;
        return 1;
    }

    std::cout << "--- FLEX INTEGRATION TEST PASSED ---" << std::endl;
    return 0;
}
