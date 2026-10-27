#include <cmath>
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
            check_true((directed->is_directed) == (1));
            check_true((count_nodes(directed.get())) == (3));
            check_true((count_edges(directed.get())) == (2));

            auto undirected = parse("graph { A -- B; }");
            check_not_null(undirected.get());
            check_true((undirected->is_directed) == (0));
            check_true((count_edges(undirected.get())) == (1));
        }

        it("recognizes case-insensitive DOT keywords") {
            auto diagram = parse("StRiCt DiGrApH G { A -> B; }");
            check_not_null(diagram.get());
            check_true((diagram->is_strict) == (1));
            check_true((diagram->is_directed) == (1));
            check_true((diagram->graph_id) != nullptr && ("G") != nullptr && std::strcmp((diagram->graph_id), ("G")) == 0);
        }

        it("preserves node attributes and endpoint compass points") {
            auto diagram = parse(
                "digraph { A [label=\"Hello\" shape=box color=red]; A:e -> B:w; }");
            check_not_null(diagram.get());
            const auto* node = find_node(diagram.get(), "A");
            check_not_null(node);
            check_true((node->label) != nullptr && ("Hello") != nullptr && std::strcmp((node->label), ("Hello")) == 0);
            check_true((node->shape) == (DG_SHAPE_BOX));
            check_not_null(diagram->edges);
            check_true((diagram->edges->from_compass) == (DG_COMPASS_E));
            check_true((diagram->edges->to_compass) == (DG_COMPASS_W));
        }

        it("coalesces duplicate edges in strict graphs") {
            auto diagram = parse(
                "strict digraph { A -> B [color=red]; A -> B [label=latest]; }");
            check_not_null(diagram.get());
            check_true((count_edges(diagram.get())) == (1));
            check_true((diagram->edges->color) != nullptr && ("red") != nullptr && std::strcmp((diagram->edges->color), ("red")) == 0);
            check_true((diagram->edges->label) != nullptr && ("latest") != nullptr && std::strcmp((diagram->edges->label), ("latest")) == 0);
        }

        it("fails fast on lexical and syntax errors") {
            auto lexical = parse("digraph { A @ -> B; }");
            check_null(lexical.get());
            check_true(std::string((dotgraph_get_last_error())).find(("lexical error")) != std::string::npos);

            auto partial = parse("digraph { A -> B; C -> ; }");
            check_null(partial.get());
            check_true(std::string((dotgraph_get_last_error())).find(("syntax error")) != std::string::npos);

            auto wrong_operator = parse("graph { A -> B; }");
            check_null(wrong_operator.get());
            check_true(std::string((dotgraph_get_last_error())).find(("does not match")) != std::string::npos);
        }

        it("reports null input without returning a stale error") {
            auto invalid = parse("not dot");
            check_null(invalid.get());

            auto null_input = parse(nullptr);
            check_null(null_input.get());
            check_true((dotgraph_get_last_error()) != nullptr && ("DOT input is NULL") != nullptr && std::strcmp((dotgraph_get_last_error()), ("DOT input is NULL")) == 0);
        }
    }

    group("layout and MIR") {
        it("initializes snapshot dimensions deterministically") {
            auto snapshot = layout("digraph { A -> B; }");
            check_true(std::fabs((snapshot.width) - (0.0)) <= (1e-9));
            check_true(std::fabs((snapshot.height) - (0.0)) <= (1e-9));
            const std::string svg = dotgraph::DotGraphRenderer::to_svg(snapshot);
            check_true(std::string((svg)).find(("<svg")) != std::string::npos);
            check_true(std::string((svg)).find(("</svg>")) != std::string::npos);
        }

        it("evaluates dimension and zero-valued numeric expressions") {
            auto snapshot = layout(
                "digraph { A [width=\"w*2\" height=\"h+10\" opacity=\"idx-idx\"]; }");
            const auto* node = find_rendered_node(snapshot, "A");
            check_not_null(node);
            check_true(std::fabs((node->width) - (240.0)) <= (1e-9));
            check_true(std::fabs((node->height) - (60.0)) <= (1e-9));
            check_true((node->extra.at("opacity")) == ("0"));
        }

        it("cascades graph node and edge defaults into rendered values") {
            auto snapshot = layout(
                "digraph { node [shape=box color=red fillcolor=\"#ffeeaa\" "
                "width=\"w+10\"]; edge [color=blue label=\"default\"]; "
                "A -> B [color=green]; }");
            check_true((snapshot.nodes.size()) == (2));
            for (const auto& node : snapshot.nodes) {
                check_true((node.shape) == (DG_SHAPE_BOX));
                check_true((node.color) == ("red"));
                check_true((node.fillcolor) == ("#ffeeaa"));
                check_true(std::fabs((node.width) - (130.0)) <= (1e-9));
            }
            check_true((snapshot.edges.size()) == (1));
            check_true((snapshot.edges.front().color) == ("green"));
            check_true((snapshot.edges.front().label) == ("default"));
        }

        it("rejects invalid numeric expressions") {
            auto diagram = parse("digraph { A [penwidth=\"w + (\"]; }");
            check_not_null(diagram.get());
            dotgraph::DotGraphRenderer renderer;
            check_true(([&]() { try { (void)(renderer.layout(diagram.get())); } catch (const std::invalid_argument&) { return true; } catch (...) {} return false; })());
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


        it("native orthogonal routes are axis-aligned and preserve self loops") {
            auto snapshot = layout("digraph { rankdir=LR; A -> B; B -> C; C -> C; }");
            check_true((snapshot.edges.size()) == (3));
            bool saw_loop = false;
            for (const auto& edge : snapshot.edges) {
                check(edge.points.size() >= 2);
                for (std::size_t i = 1; i < edge.points.size(); ++i) {
                    const auto& prev = edge.points[i - 1];
                    const auto& curr = edge.points[i];
                    check((prev.x == curr.x) != (prev.y == curr.y));
                }
                if (edge.from == "C" && edge.to == "C") {
                    saw_loop = true;
                    check(edge.points.size() >= 4);
                }
            }
            check(saw_loop);
        }

        it("native orthogonal mode refuses unsupported route options") {
            auto diagram = parse("digraph { A -> B; }");
            check_not_null(diagram.get());
            diagram->routing_crossing_penalty = 5.0;
            dotgraph::DotGraphRenderer renderer;
            check_true(([&]() { try { (void)(renderer.layout(diagram.get())); } catch (const std::invalid_argument&) { return true; } catch (...) {} return false; })());
            diagram->routing_crossing_penalty = -1.0;
            diagram->edges->from_compass = DG_COMPASS_C;
            check_true(([&]() { try { (void)(renderer.layout(diagram.get())); } catch (const std::invalid_argument&) { return true; } catch (...) {} return false; })());
        }

        it("binds real measured local DOT named port geometry") {
            auto diagram = parse("digraph { rankdir=LR; A:out -> B:in; }");
            check_not_null(diagram.get());
            check_not_null(diagram->edges);
            check_true((diagram->edges->from_port) != nullptr && ("out") != nullptr && std::strcmp((diagram->edges->from_port), ("out")) == 0);
            check_true((diagram->edges->to_port) != nullptr && ("in") != nullptr && std::strcmp((diagram->edges->to_port), ("in")) == 0);
            dotgraph::DotGraphRenderer renderer;
            check_true(([&]() { try { (void)(renderer.layout(diagram.get())); } catch (const std::invalid_argument&) { return true; } catch (...) {} return false; })());

            // Measurements belong to actual local node geometry, not label text.
            const std::vector<dotgraph::MeasuredDotPort> measured{
                {"A", "out", DG_COMPASS_E, 120.0, 12.5},
                {"B", "in", DG_COMPASS_W, 0.0, 37.5}
            };
            for (auto mode : {DG_ROUTE_ORTHOGONAL, DG_ROUTE_POLYLINE}) {
                diagram->routing_mode = mode;
                const auto snapshot = renderer.layout(diagram.get(), measured);
                check_true((snapshot.edges.size()) == (1));
                const auto* a = find_rendered_node(snapshot, "A");
                const auto* b = find_rendered_node(snapshot, "B");
                check_not_null(a);
                check_not_null(b);
                const auto& points = snapshot.edges.front().points;
                check(points.size() >= 2);
                check_true(std::fabs((points.front().x) - (a->x + a->width)) <= (1e-8));
                check_true(std::fabs((points.front().y) - (a->y + 12.5)) <= (1e-8));
                check_true(std::fabs((points.back().x) - (b->x)) <= (1e-8));
                check_true(std::fabs((points.back().y) - (b->y + 37.5)) <= (1e-8));
            }
            auto invalid = measured;
            invalid[0].local_x = 80.0;
            check_true(([&]() { try { (void)(renderer.layout(diagram.get(), invalid)); } catch (const std::invalid_argument&) { return true; } catch (...) {} return false; })());
            diagram->edges->from_compass = DG_COMPASS_S;
            check_true(([&]() { try { (void)(renderer.layout(diagram.get(), measured)); } catch (const std::invalid_argument&) { return true; } catch (...) {} return false; })());
        }

        it("renders nested clusters") {
            auto snapshot = layout(
                "digraph { subgraph cluster_outer { subgraph cluster_inner { A; B; } } }");
            check_true((snapshot.clusters.size()) == (2));
        }

        it("escapes labels in generated SVG") {
            auto snapshot = layout("digraph { A [label=\"<unsafe & text>\"]; }");
            const std::string svg = dotgraph::DotGraphRenderer::to_svg(snapshot);
            check_true(std::string((svg)).find(("&lt;unsafe &amp; text&gt;")) != std::string::npos);
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
            check_true(std::fabs((svg->layout_width()) - (320.0f)) <= (1e-5f));
            check_true(std::fabs((svg->layout_height()) - (200.0f)) <= (1e-5f));
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
            check_true((result.error) == (""));
            check(static_cast<bool>(result));
            check_not_null(result.root);
            check_true((result.root->tag()) == ("dotgraph"));
            check(result.root->has_class("dotgraph"));
            check_true((result.root->attribute("data-slot")->c_str()) != nullptr && ("dotgraph") != nullptr && std::strcmp((result.root->attribute("data-slot")->c_str()), ("dotgraph")) == 0);
            check_true((result.root->attribute("data-render-mode")->c_str()) != nullptr && ("tree") != nullptr && std::strcmp((result.root->attribute("data-render-mode")->c_str()), ("tree")) == 0);

            box.set_root(result.root);
            box.set_viewport(options.width, options.height);
            box.load_css(R"(
                .dot-node[data-node-id="A"] { background-color: #224466; }
                .dot-edge { border-color: #778899; }
            )");
            box.update();

            auto* node_a = box.query_selector(".dot-node[data-node-id=\"A\"]");
            auto* semantic_node_a = box.query_selector(
                "[data-slot=dotgraph-node][data-node-id=\"A\"]");
            auto* node_b = box.query_selector("#node-B");
            auto* edge = box.query_selector(".dot-edge[data-from=\"A\"][data-to=\"B\"]");
            check_not_null(node_a);
            check(node_a == semantic_node_a);
            check_not_null(node_b);
            check_not_null(edge);
            check_not_null(node_a->widget);
            check_not_null(edge->widget);
            check_true((node_a->attribute("data-shape")->c_str()) != nullptr && ("diamond") != nullptr && std::strcmp((node_a->attribute("data-shape")->c_str()), ("diamond")) == 0);
            check(node_a->utility_names().count("absolute") == 1);
            check(result.root->computed_style->position == flexUI::Position::Relative);
            check_true(box.missing_utility_tokens().empty());
            check_true(std::fabs((node_a->style_.background_color.r) - (0x22 / 255.0f)) <= (1e-5f));
            check_true(std::fabs((node_a->style_.background_color.g) - (0x44 / 255.0f)) <= (1e-5f));
            check_true(std::fabs((node_a->style_.background_color.b) - (0x66 / 255.0f)) <= (1e-5f));
            check_true((box.query_selector_all(".dot-node").size()) == (2));
            check_true((box.query_selector_all(".dot-edge").size()) == (1));
            check_not_null(box.query_selector(".dot-edge-label"));
        }

        it("runs real DOT source through GraphLayout and XML widgets with rollback") {
            flexUI::Box box(nullptr);
            auto* page = box.create("main", "existing-page");
            auto* sibling = box.create("span", "existing-control");
            page->append(sibling);
            box.set_root(page);

            dotgraph::FlexUiDotGraphOptions options;
            options.width = 430.0f;
            options.height = 290.0f;
            options.routing_mode = DG_ROUTE_ORTHOGONAL;

            const std::string source =
                "digraph { subgraph cluster_group { label=\"Group\"; "
                "A [shape=diamond fillcolor=\"#ccddff\"]; } "
                "A -> B [label=\"go\"]; }";
            const auto good = dotgraph::create_flexui_dotgraph(
                box, source, options);
            check_true(good.error.empty());
            check(static_cast<bool>(good));
            check_not_null(good.root);
            // The root is a detached but Box-owned transaction until the
            // caller attaches it; pre-existing visible siblings are intact.
            check_true(box.root() == page);
            check_true(page->child_count() == 1U);
            check_true(page->child_at(0) == sibling);

            page->append(good.root);
            box.set_viewport(options.width, options.height);
            box.load_css(
                ".dot-node[data-node-id=\"A\"] { background-color: #224466; } "
                ".dot-edge[data-from=\"A\"][data-to=\"B\"] "
                "{ border-color: #778899; }");
            box.update();
            auto* a = box.query_selector("#node-A");
            auto* b = box.query_selector("#node-B");
            auto* edge = box.query_selector(
                ".dot-edge[data-from=\"A\"][data-to=\"B\"]");
            check_not_null(a);
            check_not_null(b);
            check_not_null(edge);
            check_not_null(a->widget);
            check_not_null(edge->widget);
            check_true(a == box.query_selector(
                ".dot-node[data-node-id=\"A\"]"));
            check_true(a->style_.background_color.r > 0.12f);
            check_true(a->style_.background_color.r < 0.15f);
            check_true(box.missing_utility_tokens().empty());

            const auto before = box.owned_element_count();
            const auto children = page->child_count();
            const auto invalid_source = dotgraph::create_flexui_dotgraph(
                box, "digraph { A -> ; }", options);
            check_false(static_cast<bool>(invalid_source));
            check_false(invalid_source.error.empty());
            check_true(box.owned_element_count() == before);
            check_true(page->child_count() == children);

            // A second full parse/layout of the same IDs must be rejected
            // in the global ID preflight, with no partial Widget admission.
            const auto conflicting = dotgraph::create_flexui_dotgraph(
                box, source, options);
            check_false(static_cast<bool>(conflicting));
            check_false(conflicting.error.empty());
            check_true(box.owned_element_count() == before);
            check_true(box.root() == page);
            check_true(page->child_count() == children);
            check_true(box.get_by_id("node-A") == a);

            const auto recovered = dotgraph::create_flexui_dotgraph(
                box, "digraph { C -> D; }", options);
            check_true(recovered.error.empty());
            check(static_cast<bool>(recovered));
            page->append(recovered.root);
            box.update();
            check_not_null(box.query_selector("#node-D"));
            check_true(page->child_count() == children + 1U);
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
