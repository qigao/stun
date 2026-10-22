#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "dotgraph/dotgraph_ast.h"
#include "dotgraph/dotgraph_ports.h"

namespace dotgraph {

struct Point {
    double x = 0.0;
    double y = 0.0;
};

struct RenderedNode {
    std::string id;
    std::string label;
    DotGraphShape shape = DG_SHAPE_ELLIPSE;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    std::string color;
    std::string fillcolor;
    std::string fontcolor;
    std::string style;
    std::unordered_map<std::string, std::string> extra;
};

struct RenderedEdge {
    std::string from;
    std::string to;
    std::string label;
    std::vector<Point> points;
    std::string color;
    std::string style;
    bool directed = false;
    std::unordered_map<std::string, std::string> extra;
};

struct RenderedCluster {
    std::string id;
    std::string label;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    std::string color;
    std::string style;
    std::unordered_map<std::string, std::string> extra;
};

struct LayoutSnapshot {
    std::vector<RenderedNode> nodes;
    std::vector<RenderedEdge> edges;
    std::vector<RenderedCluster> clusters;
    double width = 0.0;
    double height = 0.0;
    std::unordered_map<std::string, std::string> graph_extra;
};

class DotGraphRenderer {
public:
    DotGraphRenderer();
    ~DotGraphRenderer();

    DotGraphRenderer(const DotGraphRenderer&) = delete;
    DotGraphRenderer& operator=(const DotGraphRenderer&) = delete;

    LayoutSnapshot layout(const DotGraphDiagram* diagram);
    // Callers must explicitly supply real node-local port geometry when DOT
    // edges refer to names; no name-to-position inference is performed.
    LayoutSnapshot layout(const DotGraphDiagram* diagram,
                          const std::vector<MeasuredDotPort>& measured_ports);
    static std::string to_svg(const LayoutSnapshot& snapshot);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace dotgraph
