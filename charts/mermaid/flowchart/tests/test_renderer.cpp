#include <iostream>
#include <fstream>
#include <string>
#include "flowchart_renderer.h"
#include "flowchart/flowchart_parser_wrapper.h"

// 引入原有的 JSON 序列化和释放函数
extern "C" char* flowchart_to_json(FlowchartDiagram* diagram);
extern "C" void turbo_json_serialize_free(char* str);

int main() {
    const char* mermaid_code = 
        "flowchart TD\n"
        "A[Start]-->|Yes|B{Is it working}";

    std::cout << "--- Stage 1: Parsing ---" << std::endl;
    FlowchartDiagram* diagram = flowchart_parse(mermaid_code);
    if (!diagram) {
        std::cerr << "Parser failed!" << std::endl;
        return 1;
    }

    char* json = flowchart_to_json(diagram);
    if (json) {
        // std::cout << "Parser Output (JSON):\n" << json << std::endl;
        turbo_json_serialize_free(json); // 必须用专用的释放函数
    }
    std::cout << "Parsing successful." << std::endl;

    std::cout << "--- Stage 2: Layout ---" << std::endl;
    try {
        using namespace mermaid::flowchart;
        FlowchartRenderer renderer;
        
        std::cout << "Executing OGDF layout..." << std::endl;
        LayoutSnapshot snapshot = renderer.layout(diagram);

        std::cout << "Layout completed. Nodes: " << snapshot.nodes.size() 
                  << ", Edges: " << snapshot.edges.size() << std::endl;

        if (snapshot.nodes.empty()) {
            std::cerr << "Warning: Layout produced empty result!" << std::endl;
        } else {
            std::cout << "--- Stage 3: SVG Export ---" << std::endl;
            std::string svg = FlowchartRenderer::to_svg(snapshot);
            std::ofstream out("flowchart_output.svg");
            if (out.is_open()) {
                out << svg;
                out.close();
                std::cout << "SUCCESS: SVG exported to flowchart_output.svg" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Standard exception caught: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown crash occurred during renderer execution!" << std::endl;
    }

    // 清理
    flowchart_diagram_free(diagram);
    return 0;
}
