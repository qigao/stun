#include "flowchart_renderer.h"
#include "flowchart/flowchart_parser_wrapper.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {
using namespace mermaid::flowchart;
using Diagram = std::unique_ptr<FlowchartDiagram, decltype(&flowchart_diagram_free)>;
void require(bool value) { if (!value) throw std::runtime_error("named curve regression failed"); }
Diagram parse(const std::string& source) {
    FlowchartDiagram* p = nullptr;
    auto status = flowchart_parse_ex(source.c_str(), &p);
    Diagram result(p, flowchart_diagram_free);
    require(status == FLOWCHART_PARSE_COMPLETE && p);
    return result;
}
template<class F> void rejects(F action) {
    bool refused = false;
    try { action(); } catch (const std::invalid_argument&) { refused = true; }
    require(refused);
}

void write_evidence(const std::string& base, const LayoutSnapshot& snapshot,
                    const std::vector<RenderedNode>& obstacles = {}) {
    std::ofstream svg(base + ".svg");
    svg << FlowchartRenderer::to_svg(snapshot);
    require(svg.good());
    std::ofstream json(base + ".json");
    json << std::setprecision(17) << "[";
    bool first = true;
    for (const auto& edge : snapshot.edges) {
        if (!first) json << ",";
        first = false;
        // Test-generated IDs and curve names contain only ASCII letters/digits.
        json << "{\"id\":\"" << edge.id << "\",\"curve\":\"" << edge.curve << "\",\"points\":[";
        for (std::size_t i = 0; i < edge.points.size(); ++i) {
            if (i) json << ",";
            json << "[" << edge.points[i].x << "," << edge.points[i].y << "]";
        }
        json << "]}";
    }
    json << "]\n";
    require(json.good());
    std::ofstream boxes(base + ".nodes.json");
    boxes << std::setprecision(17) << "[";
    for (std::size_t i = 0; i < obstacles.size(); ++i) {
        if (i) boxes << ",";
        const auto& n = obstacles[i];
        boxes << "[" << n.x << "," << n.y << "," << n.width << "," << n.height << "]";
    }
    boxes << "]\n";
    require(boxes.good());
}

void fixture(const char* artifact) {
    std::ifstream in(std::string(GOLDEN_DIR) + "/v2-mermaid-104.input.mmd");
    require(in.good());
    const std::string source((std::istreambuf_iterator<char>(in)), {});
    const char* ids[] = {"e1", "e5", "e7", "e3", "e2", "e4", "e6", "e8"};
    const char* curves[] = {"natural", "linear", "cardinal", "monotoneY", "stepAfter", "bumpY", "catmullRom", ""};
    for (auto mode : {FC_LAYOUT_LEGACY, FC_LAYOUT_PROFESSIONAL}) {
        auto diagram = parse(source);
        diagram->layout_mode = mode;
        FlowchartRenderer renderer;
        auto snapshot = renderer.layout(diagram.get());
        require(snapshot.nodes.size() == 6 && snapshot.edges.size() == 8);
        for (std::size_t i = 0; i < 8; ++i) {
            require(snapshot.edges[i].declaration_index == i);
            require(snapshot.edges[i].id == ids[i] && snapshot.edges[i].curve == curves[i]);
        }
        diagram.reset();
        // Geometry qualification is separate from metadata ownership. Removing
        // obstacles here exposes the routed knots to the independent D3 oracle.
        auto geometry = snapshot;
        geometry.nodes.clear();
        const auto svg = FlowchartRenderer::to_svg(geometry);
        require(svg.find("data-edge-id=\"e1\" data-edge-curve=\"natural\"") != std::string::npos);
        if (artifact) write_evidence(std::string(artifact) + (mode == FC_LAYOUT_LEGACY ? ".legacy" : ".professional"), geometry, snapshot.nodes);
        if (mode == FC_LAYOUT_LEGACY) {
            rejects([&] { (void)FlowchartRenderer::to_svg(snapshot); });
        } else {
            const auto full = FlowchartRenderer::to_svg(snapshot);
            require(full.find("data-edge-id=\"e6\" data-edge-curve=\"catmullRom\"") != std::string::npos);
            if (artifact) {
                std::ofstream out(std::string(artifact) + ".qualified.svg");
                out << full;
                require(out.good());
            }
        }
    }
}

void geometry_cases(const char* artifact) {
    std::vector<std::vector<Point>> cases = {
        {}, {{0, 0}}, {{0, 0}, {100, 50}},
        {{0, 0}, {100, 0}, {100, 100}},
        {{0, 0}, {0, 50}, {70, 50}, {70, 120}, {150, 120}},
        {{0, 0}, {0, 0}, {20, 40}, {20, 40}, {90, -30}},
        {{20, 20}, {20, 20}, {20, 20}},
        {{100, 50}, {60, 20}, {0, 40}, {-10, -50}},
        {{0, 0}, {0, 50}, {0, 100}, {40, 100}},
        {{0, 0}, {20, -20}, {40, 0}, {20, 20}, {0, 0}},
        {{-.01, .125}, {.25, -3.5}, {5.125, .5}, {11.75, 7.125}},
    };
    unsigned seed = 7341;
    for (unsigned k = 0; k < 16; ++k) {
        std::vector<Point> points;
        for (unsigned i = 0; i < 8; ++i) {
            seed = seed * 1664525u + 1013904223u;
            const double x = static_cast<int>(seed % 10001) / 13.0 - 300;
            seed = seed * 1664525u + 1013904223u;
            points.push_back({x, static_cast<int>(seed % 10001) / 17.0 - 300});
        }
        cases.push_back(std::move(points));
    }
    LayoutSnapshot snapshot;
    for (const auto* name : {"linear", "natural", "cardinal", "monotoneX", "monotoneY", "step",
                             "stepBefore", "stepAfter", "bumpX", "bumpY", "catmullRom"}) {
        for (std::size_t i = 0; i < cases.size(); ++i) {
            RenderedEdge edge;
            edge.id = std::string(name) + std::to_string(i);
            edge.curve = name;
            edge.declaration_index = snapshot.edges.size();
            edge.points = cases[i];
            snapshot.edges.push_back(std::move(edge));
        }
    }
    require(!FlowchartRenderer::to_svg(snapshot).empty());
    if (artifact) write_evidence(artifact, snapshot);
}

void refusal_and_bounds() {
    LayoutSnapshot s;
    RenderedEdge e;
    e.points = {{0, 0}, {100, 0}, {100, 100}};
    e.curve = "natural";
    s.edges.push_back(e);
    RenderedNode obstacle{};
    obstacle.x = 45; obstacle.y = -12; obstacle.width = 10; obstacle.height = 5;
    s.nodes.push_back(obstacle);
    rejects([&] { (void)FlowchartRenderer::to_svg(s); });
    s.edges[0].curve = "linear"; // The original native polyline clears the box.
    require(!FlowchartRenderer::to_svg(s).empty());
    s.edges[0].curve = "natural";
    s.nodes[0].y = -6; s.nodes[0].height = 2; // Hull overlaps; actual cubic clears it.
    require(!FlowchartRenderer::to_svg(s).empty());
    s.nodes.clear();
    s.edges[0].curve = "unknown";
    rejects([&] { (void)FlowchartRenderer::to_svg(s); });
    s.edges[0].curve = "natural";
    for (const double invalid : {std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        s.edges[0].points[1].x = invalid;
        rejects([&] { (void)FlowchartRenderer::to_svg(s); });
    }
    s.edges[0].points = {{-std::numeric_limits<double>::max(), 0}, {std::numeric_limits<double>::max(), 0}};
    s.edges[0].curve = "linear";
    rejects([&] { (void)FlowchartRenderer::to_svg(s); });
    s.edges[0].points = {{0, 0}, {10, 10}};
    s.edges[0].curve = "bumpY";
    obstacle.x = 100; obstacle.y = 100; obstacle.width = obstacle.height = 1;
    s.nodes.assign(65537, obstacle);
    rejects([&] { (void)FlowchartRenderer::to_svg(s); }); // Bounded certification work.
    s.nodes.clear();
    auto parsed = parse("flowchart TD\nA e1@--> B\ne1@{ curve: unsupported }\n");
    rejects([&] { FlowchartRenderer r; (void)r.layout(parsed.get()); });
    auto parallel = parse("flowchart TD\nA first@--> B\nA second@--> B\n"
                          "first@{ curve: natural }\nsecond@{ curve: bumpY }\n");
    for (auto mode : {FC_LAYOUT_LEGACY, FC_LAYOUT_PROFESSIONAL}) {
        parallel->layout_mode = mode;
        FlowchartRenderer renderer;
        const auto snapshot = renderer.layout(parallel.get());
        require(snapshot.edges.size() == 2);
        require(snapshot.edges[0].id == "first" && snapshot.edges[0].curve == "natural");
        require(snapshot.edges[1].id == "second" && snapshot.edges[1].curve == "bumpY");
    }
    // Caller-owned snapshots need no private parser allocation.
    s.edges[0].points = {{0, 0}, {10, 20}};
    s.edges[0].id = "edge\"<&";
    s.edges[0].curve = "bumpY";
    require(FlowchartRenderer::to_svg(s).find("edge&quot;&lt;&amp;") != std::string::npos);
}
} // namespace

void check_named_curves(const char* artifact) {
    fixture(artifact);
    geometry_cases(artifact);
    refusal_and_bounds();
    std::cout << "Named curves: identity, owned snapshots, interpolation, clearance and rejection checks passed\n";
}
