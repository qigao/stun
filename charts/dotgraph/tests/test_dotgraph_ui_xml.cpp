#include <dotgraph/flexui_dotgraph.h>

#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/render_command.h>
#include <flexUI/render_manager.h>
#include <flexUI/widget.h>
#include <tinytest.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

dotgraph::LayoutSnapshot sample_graph() {
    dotgraph::LayoutSnapshot snapshot;
    snapshot.graph_extra["bgcolor"] = "#f8fafc";

    dotgraph::RenderedCluster cluster;
    cluster.id = "workspace";
    cluster.label = "Workspace";
    cluster.x = -10.0;
    cluster.y = -10.0;
    cluster.width = 250.0;
    cluster.height = 160.0;
    cluster.color = "#94a3b8";
    cluster.style = "dashed";
    snapshot.clusters.push_back(cluster);

    dotgraph::RenderedNode a;
    a.id = "A";
    a.label = "Start";
    a.shape = DG_SHAPE_DIAMOND;
    a.x = 0.0;
    a.y = 20.0;
    a.width = 90.0;
    a.height = 46.0;
    a.fillcolor = "red";
    a.color = "#475569";
    a.fontcolor = "#1e293b";
    snapshot.nodes.push_back(a);

    dotgraph::RenderedNode b;
    b.id = "B";
    b.label = "Finish";
    b.shape = DG_SHAPE_BOX;
    b.x = 160.0;
    b.y = 20.0;
    b.width = 90.0;
    b.height = 46.0;
    b.fillcolor = "#f0f9ff";
    b.color = "#475569";
    b.fontcolor = "#0f172a";
    snapshot.nodes.push_back(b);

    dotgraph::RenderedEdge edge;
    edge.from = "A";
    edge.to = "B";
    edge.directed = true;
    edge.label = "go";
    edge.points = {{90.0, 43.0}, {160.0, 43.0}};
    edge.color = "#64748b";
    snapshot.edges.push_back(edge);
    return snapshot;
}

} // namespace

spec("DOT Graph composes semantic node edge cluster Elements via XML") {
    it("keeps real CSS selectors, widgets and visual layer geometry") {
        flexUI::Box box(nullptr);
        dotgraph::FlexUiDotGraphOptions options;
        options.width = 400.0f;
        options.height = 260.0f;
        const auto snapshot = sample_graph();
        auto* root = dotgraph::create_flexui_dotgraph(box, snapshot, options);
        check_not_null(root);
        check_null(box.root());
        check_equal(root->tag(), "dotgraph");
        check(root->has_class("dotgraph"));
        check_equal(*root->attribute("role"), "graphics-document");
        check_equal(*root->attribute("data-render-mode"), "tree");
        check_equal(*root->attribute("data-dot-bgcolor"), "#f8fafc");
        check_equal(root->child_count(), 4U);

        box.set_root(root);
        box.set_viewport(options.width, options.height);
        box.load_css(R"css(
            .dot-node[data-node-id="A"] { background-color: #224466; }
            .dot-edge[data-from="A"][data-to="B"] { border-color: #778899; }
        )css");
        box.update();

        auto* node_a = box.query_selector(".dot-node[data-node-id=\"A\"]");
        auto* same = box.query_selector("#node-A");
        auto* node_b = box.query_selector("#node-B");
        auto* edge = box.query_selector(
            ".dot-edge[data-from=\"A\"][data-to=\"B\"]");
        auto* cluster = box.query_selector("#cluster-workspace");
        check_not_null(node_a);
        check(node_a == same);
        check_not_null(node_b);
        check_not_null(edge);
        check_not_null(cluster);
        check_not_null(node_a->widget);
        check_not_null(edge->widget);
        check_equal(std::string(node_a->widget->type_name()), "DotNodeWidget");
        check_equal(std::string(edge->widget->type_name()), "DotEdgeWidget");
        check_equal(*node_a->attribute("data-slot"), "dotgraph-node");
        check_equal(*edge->attribute("data-slot"), "dotgraph-edge");
        check_equal(*cluster->attribute("data-slot"), "dotgraph-cluster");
        check_equal(*node_a->attribute("data-shape"), "diamond");
        check(node_a->utility_names().count("absolute") == 1);
        check(root->computed_style->position == flexUI::Position::Relative);
        check(box.missing_utility_tokens().empty());
        check_true(std::fabs(node_a->style_.background_color.r -
                             0x22 / 255.0f) <= 0.001f);
        check_true(std::fabs(node_a->style_.background_color.g -
                             0x44 / 255.0f) <= 0.001f);
        check_true(std::fabs(node_a->style_.background_color.b -
                             0x66 / 255.0f) <= 0.001f);
        check_equal(box.query_selector_all(".dot-node").size(), 2U);
        check_equal(box.query_selector_all(".dot-edge").size(), 1U);
        check_not_null(box.query_selector(".dot-edge-label"));
        check_not_null(box.query_selector(".dot-cluster-label"));

        // Real native shape and edge Widgets must issue backend-neutral
        // render commands; selector-only tests would miss a blank canvas.
        flexUI::RenderFrame frame;
        frame.root = root;
        frame.viewport = {options.width, options.height, 1.0f};
        flexUI::RenderManager render_manager(nullptr);
        auto commands = render_manager.build_frame_commands(
            frame, flex::RendererCapabilities{});
        check_true(!commands.commands().empty());

    }

    it("refuses a second global node ID without leaving orphan Elements") {
        flexUI::Box box(nullptr);
        auto* page = box.create("main", "page");
        box.set_root(page);
        auto graph = sample_graph();
        auto* first = dotgraph::create_flexui_dotgraph(box, graph);
        check_not_null(first);
        page->append(first);
        const auto before = box.owned_element_count();
        check_throws_as(dotgraph::create_flexui_dotgraph(box, graph),
                        std::invalid_argument);
        check_equal(box.root(), page);
        check_equal(page->child_count(), 1U);
        check_equal(box.owned_element_count(), before);
        check_not_null(box.get_by_id("node-A"));
        check_not_null(box.get_by_id("cluster-workspace"));
    }

    it("rejects duplicate nodes and nonfinite layout before Box adoption") {
        flexUI::Box box(nullptr);
        auto* page = box.create("main", "page");
        box.set_root(page);
        const auto before = box.owned_element_count();

        auto duplicate = sample_graph();
        duplicate.nodes.push_back(duplicate.nodes.front());
        check_throws_as(dotgraph::create_flexui_dotgraph(box, duplicate),
                        std::invalid_argument);
        check_equal(box.owned_element_count(), before);
        check_equal(page->child_count(), 0U);

        auto bad = sample_graph();
        bad.nodes.front().x = std::numeric_limits<double>::quiet_NaN();
        check_throws_as(dotgraph::create_flexui_dotgraph(box, bad),
                        std::invalid_argument);
        check_equal(box.owned_element_count(), before);

        auto bad_edge = sample_graph();
        bad_edge.edges.front().points[0].x =
            std::numeric_limits<double>::infinity();
        check_throws_as(dotgraph::create_flexui_dotgraph(box, bad_edge),
                        std::invalid_argument);
        check_equal(box.owned_element_count(), before);
        check_equal(page->child_count(), 0U);
    }

    it("recovers cleanly after a rejected graph and preserves Box ownership") {
        flexUI::Box box(nullptr);
        auto* page = box.create("main", "existing");
        box.set_root(page);
        dotgraph::FlexUiDotGraphOptions invalid;
        invalid.width = -1.0f;
        check_throws_as(
            dotgraph::create_flexui_dotgraph(box, sample_graph(), invalid),
            std::invalid_argument);
        check_equal(box.owned_element_count(), 1U);
        check_equal(page->child_count(), 0U);

        auto* root = dotgraph::create_flexui_dotgraph(box, sample_graph());
        check_not_null(root);
        check_equal(box.root(), page);
        page->append(root);
        box.update();
        check_not_null(box.query_selector("#node-B"));
        check_equal(page->child_count(), 1U);
    }
}
