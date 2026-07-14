#include "tinytest.h"

#include "dotgraph/dotgraph_ast.h"
#include "dotgraph/dotgraph_parser_wrapper.h"
#include "dotgraph/flex_dotgraph.h"
#include "dotgraph/flexui_dotgraph.h"
#include "dotgraph_renderer.h"
#include "flex/core/component.h"
#include "flex/core/svg.h"
#include "flexUI/box.h"
#include "flexUI/element.h"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using DiagramPtr = std::unique_ptr<DotGraphDiagram, decltype(&dotgraph_diagram_free)>;

DiagramPtr parse(const char* input) {
    return DiagramPtr(dotgraph_parse(input), &dotgraph_diagram_free);
}

size_t count_nodes(const DotGraphDiagram* diagram) {
    size_t count = 0;
    for (auto* node = diagram->nodes; node; node = node->next) ++count;
    return count;
}

size_t count_edges(const DotGraphDiagram* diagram) {
    size_t count = 0;
    for (auto* edge = diagram->edges; edge; edge = edge->next) ++count;
    return count;
}

const DotGraphNode* find_node(const DotGraphDiagram* diagram, const char* id) {
    for (auto* node = diagram->nodes; node; node = node->next) {
        if (node->id && std::strcmp(node->id, id) == 0) return node;
    }
    return nullptr;
}

const dotgraph::RenderedNode* find_rendered_node(
    const dotgraph::LayoutSnapshot& snapshot,
    const char* id) {
    for (const auto& node : snapshot.nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

dotgraph::LayoutSnapshot layout(const char* source) {
    auto diagram = parse(source);
    if (!diagram) throw std::runtime_error(dotgraph_get_last_error());
    dotgraph::DotGraphRenderer renderer;
    return renderer.layout(diagram.get());
}

} // namespace

spec("dotgraph") {
    group("parser") {
        it("parses directed and undirected graphs") {
            auto directed = parse("digraph { A -> B; B -> C; }");
            check_not_null(directed.get());
            check_int_eq(directed->is_directed, 1);
            check_size_eq(count_nodes(directed.get()), 3);
            check_size_eq(count_edges(directed.get()), 2);

            auto undirected = parse("graph { A -- B; }");
            check_not_null(undirected.get());
            check_int_eq(undirected->is_directed, 0);
            check_size_eq(count_edges(undirected.get()), 1);
        }

        it("recognizes case-insensitive DOT keywords") {
            auto diagram = parse("StRiCt DiGrApH G { A -> B; }");
            check_not_null(diagram.get());
            check_int_eq(diagram->is_strict, 1);
            check_int_eq(diagram->is_directed, 1);
            check_str_eq(diagram->graph_id, "G");
        }

        it("preserves node attributes and endpoint compass points") {
            auto diagram = parse(
                "digraph { A [label=\"Hello\" shape=box color=red]; A:e -> B:w; }");
            check_not_null(diagram.get());
            const auto* node = find_node(diagram.get(), "A");
            check_not_null(node);
            check_str_eq(node->label, "Hello");
            check_int_eq(node->shape, DG_SHAPE_BOX);
            check_not_null(diagram->edges);
            check_int_eq(diagram->edges->from_compass, DG_COMPASS_E);
            check_int_eq(diagram->edges->to_compass, DG_COMPASS_W);
        }

        it("coalesces duplicate edges in strict graphs") {
            auto diagram = parse(
                "strict digraph { A -> B [color=red]; A -> B [label=latest]; }");
            check_not_null(diagram.get());
            check_size_eq(count_edges(diagram.get()), 1);
            check_str_eq(diagram->edges->color, "red");
            check_str_eq(diagram->edges->label, "latest");
        }

        it("fails fast on lexical and syntax errors") {
            auto lexical = parse("digraph { A @ -> B; }");
            check_null(lexical.get());
            check_str_contains(dotgraph_get_last_error(), "lexical error");

            auto partial = parse("digraph { A -> B; C -> ; }");
            check_null(partial.get());
            check_str_contains(dotgraph_get_last_error(), "syntax error");

            auto wrong_operator = parse("graph { A -> B; }");
            check_null(wrong_operator.get());
            check_str_contains(dotgraph_get_last_error(), "does not match");
        }

        it("reports null input without returning a stale error") {
            auto invalid = parse("not dot");
            check_null(invalid.get());

            auto null_input = parse(nullptr);
            check_null(null_input.get());
            check_str_eq(dotgraph_get_last_error(), "DOT input is NULL");
        }
    }

    group("layout and MIR") {
        it("initializes snapshot dimensions deterministically") {
            auto snapshot = layout("digraph { A -> B; }");
            check_double_eq(snapshot.width, 0.0, 1e-9);
            check_double_eq(snapshot.height, 0.0, 1e-9);
            const std::string svg = dotgraph::DotGraphRenderer::to_svg(snapshot);
            check_string_contains(svg, "<svg");
            check_string_contains(svg, "</svg>");
        }

        it("evaluates dimension and zero-valued numeric expressions") {
            auto snapshot = layout(
                "digraph { A [width=\"w*2\" height=\"h+10\" opacity=\"idx-idx\"]; }");
            const auto* node = find_rendered_node(snapshot, "A");
            check_not_null(node);
            check_double_eq(node->width, 240.0, 1e-9);
            check_double_eq(node->height, 60.0, 1e-9);
            check_string_eq(node->extra.at("opacity"), "0");
        }

        it("cascades graph node and edge defaults into rendered values") {
            auto snapshot = layout(
                "digraph { node [shape=box color=red fillcolor=\"#ffeeaa\" "
                "width=\"w+10\"]; edge [color=blue label=\"default\"]; "
                "A -> B [color=green]; }");
            check_size_eq(snapshot.nodes.size(), 2);
            for (const auto& node : snapshot.nodes) {
                check_int_eq(node.shape, DG_SHAPE_BOX);
                check_string_eq(node.color, "red");
                check_string_eq(node.fillcolor, "#ffeeaa");
                check_double_eq(node.width, 130.0, 1e-9);
            }
            check_size_eq(snapshot.edges.size(), 1);
            check_string_eq(snapshot.edges.front().color, "green");
            check_string_eq(snapshot.edges.front().label, "default");
        }

        it("rejects invalid numeric expressions") {
            auto diagram = parse("digraph { A [penwidth=\"w + (\"]; }");
            check_not_null(diagram.get());
            dotgraph::DotGraphRenderer renderer;
            check_throws_as(renderer.layout(diagram.get()), std::invalid_argument);
        }

        it("honors all rank directions") {
            auto top_bottom = layout("digraph { rankdir=TB; A -> B; }");
            auto bottom_top = layout("digraph { rankdir=BT; A -> B; }");
            auto left_right = layout("digraph { rankdir=LR; A -> B; }");
            auto right_left = layout("digraph { rankdir=RL; A -> B; }");

            check(find_rendered_node(top_bottom, "A")->y <
                  find_rendered_node(top_bottom, "B")->y);
            check(find_rendered_node(bottom_top, "A")->y >
                  find_rendered_node(bottom_top, "B")->y);
            check(find_rendered_node(left_right, "A")->x <
                  find_rendered_node(left_right, "B")->x);
            check(find_rendered_node(right_left, "A")->x >
                  find_rendered_node(right_left, "B")->x);
        }

        it("renders nested clusters") {
            auto snapshot = layout(
                "digraph { subgraph cluster_outer { subgraph cluster_inner { A; B; } } }");
            check_size_eq(snapshot.clusters.size(), 2);
        }

        it("escapes labels in generated SVG") {
            auto snapshot = layout("digraph { A [label=\"<unsafe & text>\"]; }");
            const std::string svg = dotgraph::DotGraphRenderer::to_svg(snapshot);
            check_string_contains(svg, "&lt;unsafe &amp; text&gt;");
            check(svg.find("<unsafe & text>") == std::string::npos);
        }
    }

    group("Flex adapter") {
        it("preserves the standard Svg component for backend-neutral Flex") {
            flex::register_dotgraph_component();
            flex::Props props;
            props["code"] = std::string("digraph { A -> B; }");
            props["width"] = 320.0f;
            props["height"] = 200.0f;

            auto node = flex::create_component_instance("dotgraph", props);
            check_not_null(node.get());
            auto svg = std::dynamic_pointer_cast<flex::Svg>(node);
            check_not_null(svg.get());
            check_false(svg->data().empty());
            check_float_eq(svg->layout_width(), 320.0f, 1e-5f);
            check_float_eq(svg->layout_height(), 200.0f, 1e-5f);
        }

        it("builds a Box-owned semantic tree for CSS and events") {
            flexUI::Box box(nullptr);
            dotgraph::FlexUiDotGraphOptions options;
            options.width = 320.0f;
            options.height = 200.0f;
            const auto result = dotgraph::create_flexui_dotgraph(
                box,
                "digraph { A [shape=diamond fillcolor=red]; A -> B [label=\"edge\"]; }",
                options);
            check_string_eq(result.error, "");
            check(static_cast<bool>(result));
            check_not_null(result.root);
            check_string_eq(result.root->tag(), "dotgraph");
            check(result.root->has_class("dotgraph"));
            check_str_eq(result.root->attribute("data-render-mode")->c_str(), "tree");

            box.set_root(result.root);
            box.set_viewport(options.width, options.height);
            box.load_css(R"(
                .dot-node[data-node-id="A"] { background-color: #224466; }
                .dot-edge { border-color: #778899; }
            )");
            box.update();

            auto* node_a = box.query_selector(".dot-node[data-node-id=\"A\"]");
            auto* node_b = box.query_selector("#node-B");
            auto* edge = box.query_selector(".dot-edge[data-from=\"A\"][data-to=\"B\"]");
            check_not_null(node_a);
            check_not_null(node_b);
            check_not_null(edge);
            check_not_null(node_a->widget);
            check_not_null(edge->widget);
            check_str_eq(node_a->attribute("data-shape")->c_str(), "diamond");
            check_float_eq(node_a->style_.background_color.r, 0x22 / 255.0f, 1e-5f);
            check_float_eq(node_a->style_.background_color.g, 0x44 / 255.0f, 1e-5f);
            check_float_eq(node_a->style_.background_color.b, 0x66 / 255.0f, 1e-5f);
            check_size_eq(box.query_selector_all(".dot-node").size(), 2);
            check_size_eq(box.query_selector_all(".dot-edge").size(), 1);
            check_not_null(box.query_selector(".dot-edge-label"));
        }

        it("does not expose a partial semantic tree on parse failure") {
            flexUI::Box box(nullptr);
            const auto result = dotgraph::create_flexui_dotgraph(
                box, "digraph { A -> ; }");
            check_false(static_cast<bool>(result));
            check_null(result.root);
            check_false(result.error.empty());
            check_null(box.root());
        }
    }
}
