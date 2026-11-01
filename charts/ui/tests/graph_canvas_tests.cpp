#include <charts/ui/graph_canvas_widget.h>
#include <charts/ui/graph_edit_document.h>

#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/native_canvas_widget.h>
#include <flexUI/render_command.h>
#include <flexUI/ui_document.h>

#include <tinytest.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

namespace {

namespace gl = stun::graphlayout;
using charts::ui::GraphEditDocument;
using charts::ui::GraphEditError;
using charts::ui::GraphEditSnapshot;

GraphEditSnapshot example(bool connected = true) {
  GraphEditSnapshot data;
  // Deliberately reverse ID order. The application adapter must normalize
  // ID→GraphCore index mapping, never persist an unstable solver index.
  data.nodes = {{"beta", 440, 36, 120, 72, false},
                {"alpha", 55, 36, 125, 72, false}};
  data.ports = {{"sink", "beta", gl::Side::West, 0, 36, false},
                {"source", "alpha", gl::Side::East, 125, 36, true}};
  if (connected) data.edges = {{"edge-1", "source", "sink"}};
  return data;
}

const charts::ui::GraphEditNode *node_by_id(const GraphEditDocument &model,
                                             const std::string &id) {
  for (const auto &node : model.snapshot().nodes)
    if (node.id == id) return &node;
  return nullptr;
}

} // namespace

spec("GraphCanvas M0 application-owned graph editing") {
  it("normalizes stable IDs, routes measured real ports, and is repeatable") {
    GraphEditDocument a, b;
    auto original = example();
    auto shuffled = original;
    std::reverse(shuffled.nodes.begin(), shuffled.nodes.end());
    std::reverse(shuffled.ports.begin(), shuffled.ports.end());

    check_true(static_cast<bool>(a.initialize(std::move(original))));
    check_true(static_cast<bool>(b.initialize(std::move(shuffled))));
    check_equal(a.snapshot().revision, std::uint64_t{1});
    check_equal(a.snapshot().nodes.front().id, "alpha");
    check_equal(a.snapshot().routes.size(), 1U);
    check_equal(b.snapshot().routes.size(), 1U);
    const auto &left = a.snapshot().routes.front();
    const auto &right = b.snapshot().routes.front();
    check_equal(left.edge_id, "edge-1");
    check_true(left.points.size() >= 2);
    check_equal(left.points.size(), right.points.size());
    for (std::size_t i = 0; i < left.points.size(); ++i) {
      check_equal(left.points[i].x, right.points[i].x);
      check_equal(left.points[i].y, right.points[i].y);
    }
    if (!left.points.empty()) {
      check_within(left.points.front().x, 180.0, 0.0001);
      check_within(left.points.front().y, 72.0, 0.0001);
      check_within(left.points.back().x, 440.0, 0.0001);
      check_within(left.points.back().y, 72.0, 0.0001);
    }
  }

  it("previews without committing, then commits one atomic drag with Undo/Redo") {
    GraphEditDocument model;
    check_true(static_cast<bool>(model.initialize(example())));
    const auto first_rev = model.snapshot().revision;
    const auto first_route = model.snapshot().routes.front().points;

    check_true(static_cast<bool>(model.begin_move("alpha")));
    check_true(static_cast<bool>(model.preview_move(32.0, 14.0)));
    check_true(model.has_preview());
    double px = 0.0, py = 0.0;
    check_true(model.preview_position("alpha", px, py));
    check_equal(px, 87.0);
    check_equal(py, 50.0);
    check_equal(node_by_id(model, "alpha")->x, 55.0);
    check_equal(model.snapshot().revision, first_rev);
    check_equal(model.undo_count(), 0U);
    check_true(static_cast<bool>(model.commit_move()));
    check_false(model.has_preview());
    check_equal(node_by_id(model, "alpha")->x, 87.0);
    check_equal(model.snapshot().revision, first_rev + 1);
    check_equal(model.undo_count(), 1U);
    check_true(static_cast<bool>(model.undo()));
    check_equal(node_by_id(model, "alpha")->x, 55.0);
    check_equal(model.snapshot().revision, first_rev + 2);
    check_equal(model.snapshot().routes.front().points.size(), first_route.size());
    check_true(static_cast<bool>(model.redo()));
    check_equal(node_by_id(model, "alpha")->x, 87.0);
    check_equal(model.snapshot().revision, first_rev + 3);
    check_equal(model.undo_count(), 1U);
  }

  it("never commits invalid overlap, NaN, locked nodes or duplicate ports") {
    GraphEditDocument model;
    check_true(static_cast<bool>(model.initialize(example())));
    const auto initial_rev = model.snapshot().revision;
    const auto initial_history = model.undo_count();
    check_true(static_cast<bool>(model.begin_move("alpha")));
    auto invalid = model.preview_move(std::numeric_limits<double>::quiet_NaN(), 0);
    check_false(static_cast<bool>(invalid));
    check(invalid.error == GraphEditError::InvalidGeometry);
    check_true(static_cast<bool>(model.preview_move(420, 0)));
    invalid = model.commit_move(); // overlaps beta, must not shift beta
    check_false(static_cast<bool>(invalid));
    check(invalid.error == GraphEditError::InvalidGeometry);
    check_false(model.has_preview());
    check_equal(model.snapshot().revision, initial_rev);
    check_equal(model.undo_count(), initial_history);
    check_equal(node_by_id(model, "beta")->x, 440.0);
    check_equal(node_by_id(model, "alpha")->x, 55.0);

    auto bad = example();
    bad.ports.push_back(bad.ports.front());
    invalid = model.initialize(std::move(bad));
    check_false(static_cast<bool>(invalid));
    check_equal(model.snapshot().revision, initial_rev);

    bad = example();
    bad.nodes.front().locked = true;
    check_true(static_cast<bool>(model.initialize(std::move(bad))));
    invalid = model.begin_move("beta");
    check_false(static_cast<bool>(invalid));
    check(invalid.error == GraphEditError::LockedNode);
  }

  it("commits real connected routes, rejects missing or duplicate edge IDs") {
    GraphEditDocument model;
    check_true(static_cast<bool>(model.initialize(example(false))));
    check_equal(model.snapshot().routes.size(), 0U);
    check_true(static_cast<bool>(model.connect("source", "sink", "edge-1")));
    check_equal(model.snapshot().routes.size(), 1U);
    const auto rev = model.snapshot().revision;

    const auto duplicate = model.connect("source", "sink", "edge-1");
    check_false(static_cast<bool>(duplicate));
    check(duplicate.error == GraphEditError::InvalidInput);
    const auto missing = model.connect("not-a-port", "sink", "edge-2");
    check_false(static_cast<bool>(missing));
    check(missing.error == GraphEditError::InvalidInput);
    check_equal(model.snapshot().revision, rev);
    check_equal(model.snapshot().edges.size(), 1U);

    check_true(static_cast<bool>(model.undo()));
    check_equal(model.snapshot().edges.size(), 0U);
    check_equal(model.snapshot().routes.size(), 0U);
    check_true(static_cast<bool>(model.redo()));
    check_equal(model.snapshot().routes.size(), 1U);

    GraphEditDocument too_many;
    auto over_cap = example(false);
    over_cap.nodes.clear();
    over_cap.ports.clear();
    for (int i = 0; i < 257; ++i)
      over_cap.nodes.push_back({"node-" + std::to_string(i),
                                 static_cast<double>(i) * 200.0,
                                 0.0, 80.0, 40.0, false});
    const auto cap = too_many.initialize(std::move(over_cap));
    check(cap.error == GraphEditError::CapacityExceeded);
    check_equal(too_many.snapshot().revision, std::uint64_t{0});
  }

  it("uses an invertible viewport transform and pointer-anchored zoom") {
    charts::ui::GraphCanvasViewport view;
    double x, y;
    check_true(view.to_local(70.0, 30.0, x, y));
    check_equal(x, 70.0);
    check_equal(y, 30.0);
    double wx, wy;
    check_true(view.to_world(x, y, wx, wy));
    check_equal(wx, 70.0);
    check_equal(wy, 30.0);
    check_true(view.zoom_at(70.0, 30.0, 1.0));
    check_true(view.to_world(70.0, 30.0, wx, wy));
    check_within(wx, 70.0, 0.00001);
    check_within(wy, 30.0, 0.00001);
    check_false(view.zoom_at(70.0, 30.0,
                             std::numeric_limits<double>::infinity()));
    check_true(view.to_world(70.0, 30.0, wx, wy));
    check_within(wx, 70.0, 0.00001);
  }
}

spec("GraphCanvas M0 uses real FlexUI Box-owned XML and EventDispatcher") {
  it("captures off-node moves, cancels Escape/focus loss, connects and paints") {
    auto model = std::make_shared<GraphEditDocument>();
    check_true(static_cast<bool>(model->initialize(example(false))));
    auto registry = flexUI::WidgetRegistry::builtins();
    check_false(static_cast<bool>(
        charts::ui::register_graph_canvas_widget(registry, model)));
    auto compiled = flexUI::compile_ui_xml(
      "<ui name=\"GraphCanvas\"><main id=\"root\">"
      "<native-graph id=\"graph\" /></main></ui>");
    check_true(static_cast<bool>(compiled));
    if (!compiled) return;
    flexUI::Box box(nullptr);
    const auto installed = flexUI::UiDocumentInstantiator::instantiate(
        box, *compiled.program, registry);
    check_true(static_cast<bool>(installed));
    if (!installed) return;

    auto *root = box.get_by_id("root");
    auto *graph = box.get_by_id("graph");
    check_not_null(root);
    check_not_null(graph);
    if (!root || !graph) return;
    root->set_layout_bounds(0, 0, 832, 160);
    graph->set_layout_bounds(0, 0, 832, 160);
    auto *native = dynamic_cast<flexUI::NativeCanvasWidget *>(graph->widget);
    check_not_null(native);
    if (!native) return;

    auto down = flexUI::Event::mouse_down(85, 56);
    box.dispatch_event(down);
    check_true(down.handled);
    check(box.capturing_element() == graph);
    auto move = flexUI::Event::mouse_move(117, 70);
    box.dispatch_event(move);
    check_true(move.handled);
    check_true(model->has_preview());
    check_equal(node_by_id(*model, "alpha")->x, 55.0);
    auto up = flexUI::Event::mouse_up(117, 70);
    box.dispatch_event(up);
    check_true(up.handled);
    check_null(box.capturing_element());
    check_false(model->has_preview());
    check_equal(node_by_id(*model, "alpha")->x, 87.0);

    auto cancel_down = flexUI::Event::mouse_down(110, 70);
    box.dispatch_event(cancel_down);
    check_true(model->has_preview());
    auto cancel_move = flexUI::Event::mouse_move(130, 90);
    box.dispatch_event(cancel_move);
    auto esc = flexUI::Event::key_down(flexUI::KeyCode::Escape);
    box.dispatch_event(esc);
    check_true(esc.handled);
    check_false(model->has_preview());
    check_null(box.capturing_element());
    check_equal(node_by_id(*model, "alpha")->x, 87.0);

    auto blur_down = flexUI::Event::mouse_down(110, 70);
    box.dispatch_event(blur_down);
    check_true(model->has_preview());
    box.set_focus(nullptr);
    check_false(model->has_preview());
    check_null(box.capturing_element());

    // The real measured ports now lie at alpha (212, 86) and beta (440, 72).
    auto connect_down = flexUI::Event::mouse_down(212, 86);
    box.dispatch_event(connect_down);
    check_true(connect_down.handled);
    check(box.capturing_element() == graph);
    auto connect_move = flexUI::Event::mouse_move(440, 72);
    box.dispatch_event(connect_move);
    auto connect_up = flexUI::Event::mouse_up(440, 72);
    box.dispatch_event(connect_up);
    check_true(connect_up.handled);
    check_null(box.capturing_element());
    check_equal(model->snapshot().edges.size(), 1U);
    check_equal(model->snapshot().routes.size(), 1U);

    flexUI::RenderCommandList render(flex::RendererCapabilities{});
    native->emit_render_commands(*graph, render);
    check_true(render.commands().size() > 6);
  }
}
