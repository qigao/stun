#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

 
#include "tinytest.h"
 

#include "flowchart_renderer.h"
#include "flowchart/flowchart_ast.h"

using namespace mermaid::flowchart;

spec("flowchart_renderer_mustache") {
    describe("SVG Generation") {
        it("should render basic elements correctly") {
            // 1. 准备快照
            LayoutSnapshot snapshot;
            snapshot.total_width = 800;
            snapshot.total_height = 600;

            RenderedNode n1;
            n1.id = "A";
            n1.text = "Hello Node";
            n1.x = 100; n1.y = 100;
            n1.width = 120; n1.height = 50;
            n1.shape = FC_SHAPE_RECT;
            snapshot.nodes.push_back(n1);

            RenderedEdge e1;
            e1.source_id = "A";
            e1.target_id = "B";
            e1.points.push_back({160, 150}); // Start point
            e1.points.push_back({160, 250}); // End point
            snapshot.edges.push_back(e1);

            // 2. 执行渲染
            std::string svg = FlowchartRenderer::to_svg(snapshot);

            // 3. 断言 (使用 TinyTest 规范)
            check(svg.find("<svg") != std::string::npos, "Missing <svg tag");
            check(svg.find("width=\"800\"") != std::string::npos, "Incorrect width");
            
            // 验证 flexmaid 规范的标签 (shape_rect)
            check(svg.find("<rect") != std::string::npos, "Missing node rectangle");
            check(svg.find("Hello Node") != std::string::npos, "Missing node text");
            
            // 验证连线
            check(svg.find("<path") != std::string::npos, "Missing edge path");
        }

        it("should handle empty snapshots gracefully") {
            LayoutSnapshot snapshot;
            snapshot.total_width = 200;
            snapshot.total_height = 200;
            
            std::string svg = FlowchartRenderer::to_svg(snapshot);
            check(!svg.empty(), "SVG should not be empty");
            check(svg.find("<svg") != std::string::npos, "Missing <svg tag in empty snapshot");
        }
    }
}
