#include "flowchart_renderer.h"
#include "flowchart/flowchart_parser_wrapper.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

using namespace mermaid::flowchart;
using Diagram = std::unique_ptr<FlowchartDiagram, decltype(&flowchart_diagram_free)>;
#define CHECK(x) do { if (!(x)) throw std::runtime_error("linkStyle: " #x); } while (0)

static Diagram parse(const std::string& text) {
    FlowchartDiagram* raw = nullptr;
    const auto status = flowchart_parse_ex(text.c_str(), &raw);
    Diagram diagram(raw, flowchart_diagram_free);
    CHECK(status == FLOWCHART_PARSE_COMPLETE && diagram);
    return diagram;
}

static std::string path_tag(const std::string& svg, std::size_t ordinal) {
    const auto at = svg.find("data-edge-index=\"" + std::to_string(ordinal) + "\"");
    CHECK(at != std::string::npos);
    return svg.substr(at, svg.find('>', at) - at + 1);
}

static void check_original_fixture(const char* artifact) {
    std::ifstream source(std::string(GOLDEN_DIR) + "/mermaid-024.input.mmd");
    CHECK(source.good());
    const std::string input((std::istreambuf_iterator<char>(source)), {});
    for (auto mode : {FC_LAYOUT_LEGACY, FC_LAYOUT_PROFESSIONAL}) {
        auto diagram = parse(input);
        diagram->layout_mode = mode;
        FlowchartRenderer renderer;
        auto snapshot = renderer.layout(diagram.get());
        CHECK(snapshot.edges.size() == 3);
        CHECK(snapshot.edges[1].source_id == "B" && snapshot.edges[1].target_id == "C");
        CHECK(snapshot.edges[1].declaration_index == 1);
        CHECK(snapshot.edges[0].stroke.empty() && snapshot.edges[2].stroke.empty());
        CHECK(snapshot.edges[1].stroke == "greenyellow" && snapshot.edges[1].stroke_width == 2);
        diagram.reset(); // The render snapshot owns its appearance values.
        snapshot.options.line_color = "#123456";
        snapshot.options.line_width = 7;
        const auto svg = FlowchartRenderer::to_svg(snapshot);
        CHECK(path_tag(svg, 0).find("stroke=\"#123456\" stroke-width=\"7\"") != std::string::npos);
        CHECK(path_tag(svg, 1).find("stroke=\"greenyellow\" stroke-width=\"2\"") != std::string::npos);
        CHECK(path_tag(svg, 2).find("stroke=\"#123456\" stroke-width=\"7\"") != std::string::npos);
        const auto marker = svg.find("id=\"arrowhead-1\"");
        CHECK(marker != std::string::npos);
        CHECK(svg.substr(marker, svg.find("</marker>", marker) - marker).find("fill=\"greenyellow\"") != std::string::npos);
        if (artifact && mode == FC_LAYOUT_PROFESSIONAL) {
            std::ofstream out(artifact);
            out << svg;
            CHECK(out.good());
        }
    }
}

static void check_precedence_and_identity() {
    auto diagram = parse("flowchart LR\nA --> B\nA --> B\nB --> C\n"
        "linkStyle default stroke:blue,stroke-width:4px\n"
        "linkStyle 1 stroke:red,stroke-width:8px\n"
        "linkStyle 1 stroke-width:0px\n"
        "linkStyle default stroke:green,stroke-width:3px\n"
        "linkStyle 0,2 stroke:#abc\n");
    FlowchartRenderer renderer;
    for (auto mode : {FC_LAYOUT_LEGACY, FC_LAYOUT_PROFESSIONAL}) {
        diagram->layout_mode = mode;
        const auto snapshot = renderer.layout(diagram.get());
        CHECK(snapshot.edges.size() == 3);
        CHECK(snapshot.edges[0].source_id == snapshot.edges[1].source_id);
        CHECK(snapshot.edges[0].target_id == snapshot.edges[1].target_id);
        CHECK(snapshot.edges[0].stroke == "#abc" && snapshot.edges[0].stroke_width == 3);
        CHECK(snapshot.edges[1].stroke == "green" && snapshot.edges[1].stroke_width == 0);
        CHECK(snapshot.edges[2].stroke == "#abc" && snapshot.edges[2].stroke_width == 3);
    }
}

static void check_supported_values() {
    FlowchartRenderer renderer;
    for (const auto& color : {"RED", "#ABC", "#abcd", "#012345", "#01234567",
                              "transparent", "none", "currentColor"}) {
        auto diagram = parse(std::string("flowchart LR\nA --> B\nlinkStyle 0 stroke:") +
                             color + ",stroke-width:0.001px,fill:none\n");
        const auto snapshot = renderer.layout(diagram.get());
        CHECK(snapshot.edges[0].stroke == color);
        CHECK(snapshot.edges[0].stroke_width == 0.001);
        CHECK(path_tag(FlowchartRenderer::to_svg(snapshot), 0).find("stroke-width=\"0.001\"") != std::string::npos);
    }
    auto diagram = parse("flowchart LR\nA --> B\nlinkStyle 0 stroke:blue,stroke:red,stroke-width:2.5\n");
    const auto snapshot = renderer.layout(diagram.get());
    CHECK(snapshot.edges[0].stroke == "red" && snapshot.edges[0].stroke_width == 2.5);
}

static void check_rejection() {
    for (const auto& rule : {
        "linkStyle 1 stroke:red", "linkStyle 99999999999999999999999999999 stroke:red",
        "linkStyle nope stroke:red", "linkStyle 0 stroke:banana",
        "linkStyle 0 stroke:url(https://example.invalid/paint)",
        "linkStyle 0 stroke-width:-1", "linkStyle 0 stroke-width:NaN",
        "linkStyle 0 stroke-width:inf", "linkStyle 0 stroke-width:1e999",
        "linkStyle 0 stroke-width:2em", "linkStyle 0 stroke-width:2pxjunk",
        "linkStyle 0 opacity:0.2", "linkStyle 0 fill:red"}) {
        auto diagram = parse(std::string("flowchart LR\nA --> B\n") + rule + "\n");
        bool rejected = false;
        try { FlowchartRenderer renderer; (void)renderer.layout(diagram.get()); }
        catch (const std::invalid_argument&) { rejected = true; }
        CHECK(rejected);
    }
}

int main(int argc, char** argv) {
    try {
        CHECK(argc == 1 || argc == 2);
        check_original_fixture(argc == 2 ? argv[1] : nullptr);
        check_precedence_and_identity();
        check_supported_values();
        check_rejection();
        CHECK(FlowchartRenderer::to_svg(LayoutSnapshot{}).find("<svg") != std::string::npos);
        std::cout << "Flowchart linkStyle: ordinal, precedence, ownership, SVG and rejection checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
