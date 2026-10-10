#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
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
    bool success = true;
    try {
        using namespace mermaid::flowchart;
        FlowchartRenderer renderer;
        
        std::cout << "Executing Stun GraphLayout..." << std::endl;
        LayoutSnapshot snapshot = renderer.layout(diagram);

        std::cout << "Layout completed. Nodes: " << snapshot.nodes.size() 
                  << ", Edges: " << snapshot.edges.size() << std::endl;

        if (snapshot.nodes.size() < 2 || snapshot.edges.empty())
            throw std::runtime_error("native layout must emit nodes and orthogonal edges");
        for (const auto& edge : snapshot.edges) {
            if (edge.points.size() < 2)
                throw std::runtime_error("native route has too few waypoints");
            for (std::size_t i = 1; i < edge.points.size(); ++i) {
                const auto& p = edge.points[i - 1];
                const auto& q = edge.points[i];
                if ((p.x == q.x) == (p.y == q.y))
                    throw std::runtime_error("native route must have nonzero orthogonal segments");
            }
        }

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
        success = false;
    } catch (...) {
        std::cerr << "Unknown crash occurred during renderer execution!" << std::endl;
        success = false;
    }

    // 清理
    flowchart_diagram_free(diagram);
    return success ? 0 : 1;
}
