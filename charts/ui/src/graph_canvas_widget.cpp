#include <charts/ui/graph_canvas_widget.h>

#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/native_canvas_widget.h>
#include <flexUI/render_command.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>
#include <string>
#include <utility>

namespace charts::ui {
namespace {

constexpr double kMinZoom = 0.5;
constexpr double kMaxZoom = 3.0;
constexpr double kPortHitRadiusPx = 11.0;
constexpr double kDragStartThresholdPx = 4.0;
// The Box borrower inspector currently caps each Widget at 1024 owned parts.
// M0 is an intentionally bounded, fully materialized semantic graph.
constexpr std::size_t kMaxSemanticParts = 1024;
std::atomic<std::uint64_t> next_graph_mount_generation{1};

enum class GestureMode { Idle, ArmedNode, MoveNode, ArmedGroup, MoveGroup, Connect, Pan };

struct Interaction {
  std::weak_ptr<GraphEditDocument> document;
  GraphCanvasViewport view;
  GraphCanvasRoutingMode routing_mode = GraphCanvasRoutingMode::Synchronous;
  std::uint64_t mount_generation = 0;
  GraphRouteWorker worker;
  bool routing_pending = false;
  std::string routing_state = "idle";
  GestureMode mode = GestureMode::Idle;
  std::string selected_node;
  std::string selected_group;
  std::string source_port;
  std::string status_message;
  double press_x = 0.0;
  double press_y = 0.0;
  double previous_x = 0.0;
  double previous_y = 0.0;
  double cursor_x = 0.0;
  double cursor_y = 0.0;
  std::uint64_t edge_sequence = 1;

  ~Interaction() {
    // Cancel the worker before the Box can retire this widget; the worker
    // owns only immutable numeric graph snapshots, never Element pointers.
    worker.cancel();
    // The widget may be destroyed without a final FocusOut. No Box/Element
    // pointers are touched here. The worker destructor joins the task.
    if (mode == GestureMode::MoveNode || mode == GestureMode::MoveGroup) {
      if (auto owner = document.lock()) owner->cancel_move();
    }
  }
};

bool inside(const flexUI::Element &elem, double x, double y) {
  return x >= 0.0 && y >= 0.0 &&
         x <= static_cast<double>(elem.width()) &&
         y <= static_cast<double>(elem.height());
}

const GraphEditNode *find_node(const GraphEditSnapshot &snapshot,
                                const std::string &id) {
  for (const auto &node : snapshot.nodes)
    if (node.id == id) return &node;
  return nullptr;
}

bool is_group_proxy(const GraphEditDocument &model,
                    std::string_view id) {
  for (const auto &group : model.snapshot().groups)
    if (group.collapsed && id == "group@" + group.id) return true;
  return false;
}

bool node_rect(const GraphEditNode &node, const Interaction &interaction,
               const GraphEditDocument &document, double &x, double &y,
               double &w, double &h) {
  double nx = node.x, ny = node.y;
  document.preview_position(node.id, nx, ny);
  w = node.width * interaction.view.zoom;
  h = node.height * interaction.view.zoom;
  return interaction.view.to_local(nx, ny, x, y);
}

std::optional<GraphEditNode> hit_node(const GraphEditDocument &model,
                                       const Interaction &interaction,
                                       double local_x, double local_y) {
  GraphEditSnapshot visible;
  if (!model.visible_snapshot(visible)) return std::nullopt;
  const auto &nodes = visible.nodes;
  for (auto i = nodes.rbegin(); i != nodes.rend(); ++i) {
    double x, y, w, h;
    if (node_rect(*i, interaction, model, x, y, w, h) &&
        local_x >= x && local_x <= x + w &&
        local_y >= y && local_y <= y + h) return *i;
  }
  return std::nullopt;
}

bool port_anchor(const GraphEditDocument &document,
                 const GraphEditSnapshot &snapshot, const GraphEditPort &port,
                 const Interaction &interaction, double &x, double &y) {
  const auto *node = find_node(snapshot, port.node_id);
  if (!node) return false;
  double px = node->x, py = node->y;
  document.preview_position(node->id, px, py);
  return interaction.view.to_local(
      px + port.local_x, py + port.local_y, x, y);
}

std::optional<GraphEditPort> hit_port(const GraphEditDocument &model,
                                       const Interaction &interaction,
                                       double local_x, double local_y,
                                       bool output) {
  GraphEditSnapshot snapshot;
  if (!model.visible_snapshot(snapshot)) return std::nullopt;
  for (const auto &port : snapshot.ports) {
    if (port.output != output) continue;
    double x, y;
    if (!port_anchor(model, snapshot, port, interaction, x, y)) continue;
    const double dx = local_x - x, dy = local_y - y;
    if (dx * dx + dy * dy <= kPortHitRadiusPx * kPortHitRadiusPx)
      return port;
  }
  return std::nullopt;
}

void build_semantic_parts(flexUI::NativeCanvasWidget &widget,
                          flexUI::Element &host, const GraphEditDocument &model,
                          Interaction &state) {
  const auto &snapshot = model.snapshot();
  std::size_t group_parts = snapshot.groups.size();
  for (const auto &group : snapshot.groups)
    group_parts += group.proxy_ports.size();
  if (snapshot.nodes.size() + snapshot.ports.size() + group_parts >
      kMaxSemanticParts) {
    throw std::invalid_argument(
        "GraphCanvas exceeds Box's bounded Widget-owned semantic-part capacity");
  }

  host.set_attribute("role", "region");
  host.set_attribute("aria-label", "Interactive graph editor");
  host.set_attribute("aria-keyshortcuts", "Control+Z Control+Y ArrowUp ArrowDown ArrowLeft ArrowRight");
  host.set_attribute("data-slot", "graph-canvas");
  host.set_attribute("data-routing", state.routing_state);

  for (const auto &node : snapshot.nodes) {
    const auto name = "node:" + node.id;
    auto *part = widget.declare_semantic_part("graph-node", name.c_str());
    if (!part) throw std::runtime_error("could not create GraphCanvas semantic node");
    part->set_attribute("role", "group");
    part->set_attribute("aria-hidden", "false");
    part->set_attribute("aria-label", "Graph node " + node.id);
    part->set_attribute("data-slot", "graph-node");
    part->set_attribute("data-node-id", node.id);
    part->set_attribute("aria-current", "false");
    part->set_attribute("data-state", "idle");
  }

  for (const auto &port : snapshot.ports) {
    const auto name = "port:" + port.id;
    auto *part = widget.declare_semantic_part("graph-port", name.c_str());
    if (!part) throw std::runtime_error("could not create GraphCanvas semantic port");
    part->set_attribute("role", "group");
    part->set_attribute("aria-hidden", "false");
    part->set_attribute("aria-label",
        (port.output ? "Output " : "Input ") + port.id +
        " on graph node " + port.node_id);
    part->set_attribute("data-slot", "graph-port");
    part->set_attribute("data-port-id", port.id);
    part->set_attribute("data-node-id", port.node_id);
    part->set_attribute("data-direction", port.output ? "output" : "input");
  }
  // Predeclare existing compound proxies in this Box's single semantic tree.
  // Runtime group insertion/removal still needs the separate scoped dynamic
  // semantic-part reconciliation gate (#378/#375).
  for (const auto &group : snapshot.groups) {
    const std::string proxy = "group@" + group.id;
    const auto name = "node:" + proxy;
    auto *part = widget.declare_semantic_part("graph-node", name.c_str());
    if (!part) throw std::runtime_error("could not create graph group semantic node");
    part->set_attribute("role", "group");
    part->set_attribute("aria-label", "Graph group " + group.id);
    part->set_attribute("data-slot", "graph-node");
    part->set_attribute("data-node-id", proxy);
    part->set_attribute("aria-current", "false");
    part->set_attribute("data-state", "idle");
    part->set_attribute("aria-hidden", "true");
    for (const auto &bound : group.proxy_ports) {
      const auto port_name = "port:" + bound.id;
      auto *port_part = widget.declare_semantic_part(
          "graph-port", port_name.c_str());
      if (!port_part)
        throw std::runtime_error("could not create graph proxy semantic port");
      port_part->set_attribute("role", "group");
      port_part->set_attribute("aria-label", "Group boundary " + bound.id);
      port_part->set_attribute("data-slot", "graph-port");
      port_part->set_attribute("data-port-id", bound.id);
      port_part->set_attribute("data-node-id", proxy);
      port_part->set_attribute("aria-hidden", "true");
    }
  }
}

// The single Box child list is the sole semantic ownership source. Never
// hold Element* in an application-side cache: dynamic removals can physically
// retire a part between owner frames, invalidating any borrowed old pointer.
void sync_semantic_parts(const GraphEditDocument &model,
                         const Interaction &state, flexUI::Element &host) {
  GraphEditSnapshot snapshot;
  if (!model.visible_snapshot(snapshot)) return;
  const auto update_bounds = [](flexUI::Element &part, double x, double y,
                                double w, double h) {
    if (!std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(w) || !std::isfinite(h)) return;
    const auto px = static_cast<float>(x), py = static_cast<float>(y);
    const auto pw = static_cast<float>(w), ph = static_cast<float>(h);
    if (part.x() != px || part.y() != py ||
        part.width() != pw || part.height() != ph)
      part.set_layout_bounds(px, py, pw, ph);
  };
  const auto set_attr = [](flexUI::Element &part,
                           const char *name, const std::string &value) {
    const auto *current = part.attribute(name);
    if (!current || *current != value) part.set_attribute(name, value);
  };
  const auto hidden = [&](flexUI::Element &part) {
    update_bounds(part, 0.0, 0.0, 0.0, 0.0);
    set_attr(part, "aria-hidden", "true");
  };

  for (auto *raw : host.children()) {
    auto *part = dynamic_cast<flexUI::Element *>(raw);
    if (!part || !part->is_widget_owned()) continue;
    const auto *name = part->attribute("part");
    if (!name) continue;
    if (name->rfind("node:", 0) == 0) {
      const std::string id = name->substr(5);
      const auto *node = find_node(snapshot, id);
      if (!node) { hidden(*part); continue; }
      double x, y, w, h;
      if (!node_rect(*node, state, model, x, y, w, h)) {
        hidden(*part);
        continue;
      }
      update_bounds(*part, x, y, w, h);
      set_attr(*part, "aria-hidden", "false");
      set_attr(*part, "data-node-id", id);
      set_attr(*part, "aria-current",
               state.selected_node == id ? "true" : "false");
      set_attr(*part, "data-state",
               node->locked ? "locked" :
               state.selected_node == id ? "selected" : "idle");
      continue;
    }
    if (name->rfind("port:", 0) == 0) {
      const std::string id = name->substr(5);
      const auto it = std::find_if(
          snapshot.ports.begin(), snapshot.ports.end(),
          [&](const GraphEditPort &port) { return port.id == id; });
      if (it == snapshot.ports.end()) { hidden(*part); continue; }
      double x, y;
      if (!port_anchor(model, snapshot, *it, state, x, y)) {
        hidden(*part);
        continue;
      }
      update_bounds(*part, x - 7.0, y - 7.0, 14.0, 14.0);
      set_attr(*part, "aria-hidden", "false");
      set_attr(*part, "data-port-id", it->id);
      set_attr(*part, "data-node-id", it->node_id);
      set_attr(*part, "data-direction", it->output ? "output" : "input");
      // Original-port metadata can change under a successful JSON reload
      // while the stable PortId retains the same Box-owned Element.
      bool proxy = false;
      for (const auto &group : model.snapshot().groups)
        for (const auto &boundary : group.proxy_ports)
          if (boundary.id == it->id) proxy = true;
      set_attr(*part, "aria-label",
               proxy ? "Group boundary " + it->id :
               (it->output ? "Output " : "Input ") + it->id +
                   " on graph node " + it->node_id);
    }
  }
  set_attr(host, "data-routing", state.routing_state);
}

std::vector<flexUI::WidgetSemanticPart>
desired_graph_semantics(const GraphEditDocument &model) {
  const auto &snapshot = model.snapshot();
  std::size_t total = snapshot.nodes.size() + snapshot.ports.size() +
                      snapshot.groups.size();
  for (const auto &group : snapshot.groups)
    total += group.proxy_ports.size();
  if (total > kMaxSemanticParts)
    throw std::length_error("GraphCanvas dynamic part budget exceeded");
  std::vector<flexUI::WidgetSemanticPart> desired;
  desired.reserve(total);
  for (const auto &node : snapshot.nodes) {
    desired.push_back({"node:" + node.id, "graph-node",
        {{"role", "group"}, {"aria-label", "Graph node " + node.id},
         {"data-slot", "graph-node"}, {"data-node-id", node.id},
         {"aria-current", "false"}, {"data-state", "idle"},
         {"aria-hidden", "true"}}});
  }
  for (const auto &port : snapshot.ports) {
    desired.push_back({"port:" + port.id, "graph-port",
        {{"role", "group"},
         {"aria-label", (port.output ? "Output " : "Input ") + port.id +
                             " on graph node " + port.node_id},
         {"data-slot", "graph-port"}, {"data-port-id", port.id},
         {"data-node-id", port.node_id},
         {"data-direction", port.output ? "output" : "input"},
         {"aria-hidden", "true"}}});
  }
  for (const auto &group : snapshot.groups) {
    const std::string node = "group@" + group.id;
    desired.push_back({"node:" + node, "graph-node",
        {{"role", "group"}, {"aria-label", "Graph group " + group.id},
         {"data-slot", "graph-node"}, {"data-node-id", node},
         {"aria-current", "false"}, {"data-state", "idle"},
         {"aria-hidden", "true"}}});
    for (const auto &port : group.proxy_ports) {
      desired.push_back({"port:" + port.id, "graph-port",
          {{"role", "group"}, {"aria-label", "Group boundary " + port.id},
           {"data-slot", "graph-port"}, {"data-port-id", port.id},
           {"data-node-id", node}, {"aria-hidden", "true"}}});
    }
  }
  return desired;
}

void paint_canvas(const GraphEditDocument &model, const Interaction &interaction,
                  const flexUI::Element &element,
                  flexUI::RenderCommandList &commands) {
  using flexUI::Color;
  using flexUI::Paint;
  const float width = element.width(), height = element.height();
  const auto background = Paint::solid(Color{0.09F, 0.13F, 0.17F, 1.0F});
  const auto grid = Paint::solid(Color{0.19F, 0.24F, 0.31F, 0.5F});
  const auto link = Paint::solid(Color{0.40F, 0.71F, 0.61F, 1.0F});
  const auto light = Color{0.88F, 0.93F, 0.94F, 1.0F};
  const auto muted = Color{0.59F, 0.68F, 0.74F, 1.0F};

  commands.draw_rect(0, 0, width, height, 5.0F, background, Paint::none(), 0);
  commands.save();
  commands.clip_rect(0, 0, width, height);

  // A bounded visual grid, never an Element subtree or a second scene graph.
  const double spacing = 32.0 * interaction.view.zoom;
  const double grid_start_x = std::fmod(interaction.view.pan_x, spacing);
  const double grid_start_y = std::fmod(interaction.view.pan_y, spacing);
  for (double x = grid_start_x; x < width; x += spacing)
    commands.draw_line(static_cast<float>(x), 0.0F,
                       static_cast<float>(x), height, grid, 1.0F);
  for (double y = grid_start_y; y < height; y += spacing)
    commands.draw_line(0.0F, static_cast<float>(y),
                       width, static_cast<float>(y), grid, 1.0F);

  GraphEditSnapshot snapshot;
  if (!model.visible_snapshot(snapshot)) {
    commands.restore();
    return;
  }
  // Expanded groups are document geometry, not a second UI/Element tree.
  for (const auto &group : model.snapshot().groups) {
    if (group.collapsed || !model.group_is_visible(group.id)) continue;
    GraphEditNode world;
    if (!model.group_world_rect(group.id, world)) continue;
    double wx = world.x, wy = world.y;
    model.preview_position(world.id, wx, wy);
    double gx, gy;
    if (!interaction.view.to_local(wx, wy, gx, gy)) continue;
    commands.draw_rect(static_cast<float>(gx), static_cast<float>(gy),
        static_cast<float>(world.width * interaction.view.zoom),
        static_cast<float>(world.height * interaction.view.zoom),
        5.0F, Paint::none(),
        Paint::solid(Color{0.37F, 0.52F, 0.66F, 1.0F}), 1.0F);
    commands.draw_text("GROUP " + group.id + " [-]",
        static_cast<float>(gx + 6.0), static_cast<float>(gy + 16.0),
        "Arial", 11.0F, false, light);
  }
  for (const auto &route : snapshot.routes) {
    for (std::size_t i = 1; i < route.points.size(); ++i) {
      double x0, y0, x1, y1;
      if (!interaction.view.to_local(route.points[i - 1].x, route.points[i - 1].y,
                                      x0, y0) ||
          !interaction.view.to_local(route.points[i].x, route.points[i].y,
                                      x1, y1)) continue;
      commands.draw_line(static_cast<float>(x0), static_cast<float>(y0),
                         static_cast<float>(x1), static_cast<float>(y1),
                         link, 2.0F);
    }
  }

  for (const auto &node : snapshot.nodes) {
    const bool selected = interaction.selected_node == node.id;
    const bool preview = model.has_preview() && selected;
    double x, y, w, h;
    if (!node_rect(node, interaction, model, x, y, w, h)) continue;
    const auto fill = Paint::solid(preview
        ? Color{0.32F, 0.37F, 0.31F, 1.0F}
        : selected ? Color{0.23F, 0.33F, 0.39F, 1.0F}
                   : Color{0.17F, 0.24F, 0.30F, 1.0F});
    const auto outline = Paint::solid(selected
        ? Color{0.66F, 0.79F, 0.63F, 1.0F}
        : Color{0.36F, 0.46F, 0.51F, 1.0F});
    commands.draw_rect(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(w), static_cast<float>(h), 6.0F,
                       fill, outline, selected ? 2.0F : 1.0F);
    const bool proxy = is_group_proxy(model, node.id);
    commands.draw_text(proxy ? "GROUP " + node.id.substr(6) + " [+]" : node.id,
                       static_cast<float>(x + 12.0),
                       static_cast<float>(y + 26.0), "Arial", 12.0F, true, light);
    if (node.locked)
      commands.draw_text("LOCKED", static_cast<float>(x + 12.0),
                         static_cast<float>(y + 47.0), "Arial", 10.0F, false, muted);
  }

  for (const auto &port : snapshot.ports) {
    double x, y;
    if (!port_anchor(model, snapshot, port, interaction, x, y)) continue;
    commands.draw_circle(static_cast<float>(x), static_cast<float>(y), 5.5F,
                         Paint::solid(port.output
                             ? Color{0.55F, 0.87F, 0.63F, 1.0F}
                             : Color{0.85F, 0.67F, 0.47F, 1.0F}),
                         Paint::solid(Color{0.15F, 0.22F, 0.29F, 1.0F}), 1.0F);
  }

  if (interaction.mode == GestureMode::Connect) {
    for (const auto &port : snapshot.ports) {
      if (port.id != interaction.source_port) continue;
      double x, y;
      if (port_anchor(model, snapshot, port, interaction, x, y)) {
        commands.draw_line(static_cast<float>(x), static_cast<float>(y),
                           static_cast<float>(interaction.cursor_x),
                           static_cast<float>(interaction.cursor_y),
                           Paint::solid(Color{0.97F, 0.70F, 0.38F, 0.7F}), 1.5F);
      }
      break;
    }
  }
  commands.restore();

  commands.draw_text("GRAPH  |  drag nodes / ports / middle-pan / wheel-zoom",
                     12.0F, 14.0F, "Arial", 10.0F, false, muted);
  if (model.has_preview()) {
    commands.draw_text("MOVE PREVIEW (uncommitted)", 12.0F, height - 8.0F,
                       "Arial", 10.0F, false, muted);
  } else if (interaction.routing_pending) {
    commands.draw_text("ROUTING PENDING (uncommitted)", 12.0F, height - 8.0F,
                       "Arial", 10.0F, false, muted);
  } else if (!interaction.status_message.empty()) {
    commands.draw_text(interaction.status_message, 12.0F, height - 8.0F,
                       "Arial", 10.0F, false, muted);
  }
}

void cancel_pending(Interaction &state, flexUI::Element &element) {
  if (state.routing_mode != GraphCanvasRoutingMode::AsyncCommit ||
      (!state.worker.pending() && !state.routing_pending)) return;
  state.worker.cancel();
  state.routing_pending = false;
  state.routing_state = "canceled";
  state.status_message = "Graph routing canceled";
  element.mark_paint_dirty();
}

void cancel(Interaction &state, GraphEditDocument &document,
            flexUI::NativeCanvasWidget &widget, flexUI::Element &element) {
  if (state.mode == GestureMode::MoveNode ||
      state.mode == GestureMode::MoveGroup) document.cancel_move();
  state.mode = GestureMode::Idle;
  state.source_port.clear();
  widget.release_pointer_capture();
  element.mark_paint_dirty();
}

bool handle_input(Interaction &state, GraphEditDocument &model,
                  flexUI::NativeCanvasWidget &widget,
                  const flexUI::Event &event, flexUI::Element &element) {
  using flexUI::EventType;
  using flexUI::KeyCode;
  using flexUI::MouseButton;
  if (event.type == EventType::FocusOut) {
    if (state.mode != GestureMode::Idle) cancel(state, model, widget, element);
    cancel_pending(state, element);
    return false;
  }
  if (event.type == EventType::KeyDown) {
    if (event.key == KeyCode::Escape &&
        (state.mode != GestureMode::Idle || state.routing_pending)) {
      if (state.mode != GestureMode::Idle) cancel(state, model, widget, element);
      cancel_pending(state, element);
      return true;
    }
    // Do not race new Undo/Redo or arrow edits with an in-flight edit.
    // Escape/FocusOut can revoke it, and the frame callback drains it.
    if (state.routing_mode == GraphCanvasRoutingMode::AsyncCommit &&
        state.worker.pending()) return true;
    if (state.mode == GestureMode::Idle && (event.mods & static_cast<int>(flexUI::KeyMod::Control))) {
      GraphEditStatus result;
      if (event.key == KeyCode::Z) result = model.undo();
      else if (event.key == KeyCode::Y) result = model.redo();
      else return false;
      state.status_message = result ? "" : result.message;
      element.mark_paint_dirty();
      return true;
    }
    if (state.mode == GestureMode::Idle && !state.selected_node.empty()) {
      const double step = (event.mods & static_cast<int>(flexUI::KeyMod::Shift))
          ? 32.0 : 8.0;
      double dx = 0.0, dy = 0.0;
      if (event.key == KeyCode::Left) dx = -step;
      else if (event.key == KeyCode::Right) dx = step;
      else if (event.key == KeyCode::Up) dy = -step;
      else if (event.key == KeyCode::Down) dy = step;
      else return false;

      auto result = model.begin_move(state.selected_node);
      if (result) {
        result = model.preview_move(dx, dy);
        if (result) result = model.commit_move();
        else model.cancel_move();
      }
      state.status_message = result ? "" : result.message;
      element.mark_paint_dirty();
      return true;
    }
    return false;
  }
  if (event.type != EventType::MouseDown &&
      event.type != EventType::MouseMove &&
      event.type != EventType::MouseUp &&
      event.type != EventType::MouseWheel) return false;
  if (!std::isfinite(event.x) || !std::isfinite(event.y)) {
    if (state.mode != GestureMode::Idle) cancel(state, model, widget, element);
    return false;
  }
  const double x = static_cast<double>(event.x - element.absolute_x());
  const double y = static_cast<double>(event.y - element.absolute_y());
  state.cursor_x = x;
  state.cursor_y = y;

  if (event.type == EventType::MouseWheel) {
    if (!inside(element, x, y)) return false;
    if (state.view.zoom_at(x, y, event.delta_y)) {
      element.mark_paint_dirty();
      return true;
    }
    return false;
  }

  if (event.type == EventType::MouseDown) {
    if (state.routing_mode == GraphCanvasRoutingMode::AsyncCommit &&
        state.worker.pending()) return true;
    if (state.mode != GestureMode::Idle || !inside(element, x, y)) return false;
    state.press_x = state.previous_x = x;
    state.press_y = state.previous_y = y;
    if (event.button == MouseButton::Middle) {
      state.mode = GestureMode::Pan;
      widget.request_pointer_capture();
      return true;
    }
    if (event.button != MouseButton::Left) return false;
    // Expanded group header is a reversible collapse affordance. Its content
    // geometry and all logical port identities remain untouched on failure.
    // The most deeply nested matching expanded header wins (smallest
    // rectangle); no invisible descendant can steal a collapsed parent's
    // pointer capture. The one Box still owns all EventDispatcher routing.
    std::string header_group;
    double smallest_area = std::numeric_limits<double>::infinity();
    for (const auto &group : model.snapshot().groups) {
      if (group.collapsed || !model.group_is_visible(group.id)) continue;
      GraphEditNode world;
      if (!model.group_world_rect(group.id, world)) continue;
      double gx, gy;
      if (!state.view.to_local(world.x, world.y, gx, gy)) continue;
      const double gw = world.width * state.view.zoom;
      const double area = world.width * world.height;
      if (x >= gx + 4.0 && x <= gx + gw - 4.0 &&
          y >= gy && y <= gy + 20.0 && area < smallest_area) {
        smallest_area = area;
        header_group = group.id;
      }
    }
    if (!header_group.empty()) {
      // A click toggles collapse on MouseUp. A >4px drag translates the
      // entire local subtree using the existing one-document transaction.
      state.selected_node.clear();
      state.selected_group = std::move(header_group);
      state.mode = GestureMode::ArmedGroup;
      state.status_message.clear();
      widget.request_pointer_capture();
      element.mark_paint_dirty();
      return true;
    }
    if (const auto source = hit_port(model, state, x, y, true)) {
      state.source_port = source->id;
      state.mode = GestureMode::Connect;
      widget.request_pointer_capture();
      element.mark_paint_dirty();
      return true;
    }
    if (const auto node = hit_node(model, state, x, y)) {
      if (is_group_proxy(model, node->id)) {
        state.selected_node.clear();
        state.selected_group = node->id.substr(6);
        state.mode = GestureMode::ArmedGroup;
        state.status_message.clear();
        widget.request_pointer_capture();
        element.mark_paint_dirty();
        return true;
      }
      state.selected_node = node->id;
      if (!node->locked) {
        // Reserve capture for the gesture, but do NOT begin a document
        // transaction until the pointer moves beyond the CSS-pixel threshold.
        // A normal click must not generate an Undo entry or dirty routes.
        state.mode = GestureMode::ArmedNode;
        state.status_message.clear();
        widget.request_pointer_capture();
      } else {
        state.status_message = "Node is locked";
      }
      element.mark_paint_dirty();
      return true;
    }
    state.selected_node.clear();
    element.mark_paint_dirty();
    return true;
  }

  if (event.type == EventType::MouseMove) {
    if (state.mode == GestureMode::Idle) return false;
    if (state.mode == GestureMode::Pan) {
      state.view.pan_x += x - state.previous_x;
      state.view.pan_y += y - state.previous_y;
      state.previous_x = x;
      state.previous_y = y;
    } else if (state.mode == GestureMode::ArmedNode ||
               state.mode == GestureMode::MoveNode ||
               state.mode == GestureMode::ArmedGroup ||
               state.mode == GestureMode::MoveGroup) {
      const double dx = x - state.press_x, dy = y - state.press_y;
      if (state.mode == GestureMode::ArmedNode ||
          state.mode == GestureMode::ArmedGroup) {
        if (dx * dx + dy * dy <=
            kDragStartThresholdPx * kDragStartThresholdPx) {
          return true; // Still a click, not a graph-document preview.
        }
        const bool group = state.mode == GestureMode::ArmedGroup;
        const auto started = group
            ? model.begin_group_move(state.selected_group)
            : model.begin_move(state.selected_node);
        if (!started) {
          state.status_message = started.message;
          cancel(state, model, widget, element);
          return true;
        }
        state.mode = group ? GestureMode::MoveGroup : GestureMode::MoveNode;
      }
      const auto status = model.preview_move(
          dx / state.view.zoom, dy / state.view.zoom);
      if (!status) state.status_message = status.message;
    }
    element.mark_paint_dirty();
    return true;
  }

  if (event.type == EventType::MouseUp && state.mode != GestureMode::Idle) {
    // Any unpaired MouseUp cancels rather than leaving an uncaptured gesture.
    const bool matching = (state.mode == GestureMode::Pan)
        ? event.button == MouseButton::Middle
        : event.button == MouseButton::Left;
    if (matching && state.mode == GestureMode::MoveNode) {
      if (state.routing_mode == GraphCanvasRoutingMode::AsyncCommit) {
        double px = 0.0, py = 0.0;
        const bool has_position =
            model.preview_position(state.selected_node, px, py);
        const auto *original = find_node(model.snapshot(), state.selected_node);
        if (has_position && original) {
          const auto dx = px - original->x, dy = py - original->y;
          // Release the transaction preview BEFORE submitting a frozen
          // immutable snapshot; the worker may never read the live owner.
          model.cancel_move();
          const auto result = state.worker.submit_move(
              model, state.selected_node, dx, dy, state.mount_generation);
          state.routing_pending = static_cast<bool>(result);
          state.routing_state = result ? "pending" : "error";
          state.status_message = result ? "" : result.message;
        } else {
          model.cancel_move();
          state.routing_state = "error";
          state.status_message = "drag preview was not available";
        }
      } else {
        const auto status = model.commit_move();
        state.status_message = status ? "" : status.message;
      }
    } else if (matching && state.mode == GestureMode::MoveGroup) {
      // M2b: compound move is an atomic synchronous UI-owner commit.
      // AsyncCommit's existing native worker currently handles single nodes;
      // group async scheduling is a separate gate, not an implicit fallback.
      const auto status = model.commit_move();
      state.status_message = status ? "" : status.message;
    } else if (matching && state.mode == GestureMode::ArmedGroup) {
      // Subthreshold click toggles the existing stable group rather than
      // appending a useless move transaction to Undo/Redo.
      const auto &groups = model.snapshot().groups;
      const auto found = std::find_if(groups.begin(), groups.end(),
          [&](const GraphEditGroup &g) {
            return g.id == state.selected_group;
          });
      if (found != groups.end()) {
        const auto status = model.set_group_collapsed(
            found->id, !found->collapsed);
        state.status_message = status ? "" : status.message;
      } else {
        state.status_message = "Group was retired";
      }
    } else if (matching && state.mode == GestureMode::Connect) {
      const auto target = hit_port(model, state, x, y, false);
      if (target) {
        std::string edge_id;
        bool used;
        do {
          edge_id = "edge-" + std::to_string(state.edge_sequence++);
          used = false;
          for (const auto &edge : model.snapshot().edges)
            if (edge.id == edge_id) used = true;
        } while (used);
        const auto status = model.connect(state.source_port, target->id,
                                          std::move(edge_id));
        state.status_message = status ? "" : status.message;
      } else {
        state.status_message = "Connection canceled (no input port)";
      }
    }
    cancel(state, model, widget, element);
    return true;
  }
  return false;
}

void poll_routing(Interaction &state, GraphEditDocument &document,
                  flexUI::Element &element) {
  if (state.routing_mode != GraphCanvasRoutingMode::AsyncCommit) return;
  auto result = state.worker.poll(); // zero-time wait, on UI owner thread.
  if (!result) return;
  if (state.routing_pending) {
    auto status = document.accept_prepared_move(
        std::move(*result), state.mount_generation);
    state.routing_state = status ? "committed" : "error";
    state.status_message = status ? "Graph routing committed" : status.message;
  } else {
    // A canceled job may complete later. No result is ever applied to the
    // graph after Escape, FocusOut or scope cancellation.
    state.routing_state = "canceled";
  }
  state.routing_pending = false;
  sync_semantic_parts(document, state, element);
  element.mark_paint_dirty();
}

} // namespace

bool GraphCanvasViewport::to_local(double world_x, double world_y,
                                    double &local_x, double &local_y) const noexcept {
  if (!std::isfinite(world_x) || !std::isfinite(world_y) ||
      !std::isfinite(zoom) || !std::isfinite(pan_x) ||
      !std::isfinite(pan_y) || zoom < kMinZoom || zoom > kMaxZoom) return false;
  local_x = world_x * zoom + pan_x;
  local_y = world_y * zoom + pan_y;
  return std::isfinite(local_x) && std::isfinite(local_y);
}

bool GraphCanvasViewport::to_world(double local_x, double local_y,
                                    double &world_x, double &world_y) const noexcept {
  if (!std::isfinite(local_x) || !std::isfinite(local_y) ||
      !std::isfinite(zoom) || !std::isfinite(pan_x) ||
      !std::isfinite(pan_y) || zoom < kMinZoom || zoom > kMaxZoom) return false;
  world_x = (local_x - pan_x) / zoom;
  world_y = (local_y - pan_y) / zoom;
  return std::isfinite(world_x) && std::isfinite(world_y);
}

bool GraphCanvasViewport::zoom_at(double local_x, double local_y,
                                  double wheel_delta) noexcept {
  double world_x, world_y;
  if (!std::isfinite(wheel_delta) || wheel_delta == 0.0 ||
      !to_world(local_x, local_y, world_x, world_y)) return false;
  const double desired = std::clamp(zoom *
      std::pow(1.2, std::clamp(wheel_delta, -3.0, 3.0)), kMinZoom, kMaxZoom);
  const double next_x = local_x - world_x * desired;
  const double next_y = local_y - world_y * desired;
  if (!std::isfinite(next_x) || !std::isfinite(next_y)) return false;
  const bool changed = desired != zoom;
  zoom = desired;
  pan_x = next_x;
  pan_y = next_y;
  return changed;
}

flexUI::WidgetRegistryError register_graph_canvas_widget(
    flexUI::WidgetRegistry &registry, std::shared_ptr<GraphEditDocument> document,
    GraphCanvasRoutingMode mode) {
  if (!document) {
    return {flexUI::WidgetRegistryErrorCode::InvalidFactory,
            "GraphCanvas requires one application-owned graph document"};
  }
  return registry.register_widget(
      "native-graph",
      [document = std::move(document), mode](const flexUI::UiNodeDefinition &)
          -> std::unique_ptr<flexUI::Widget> {
        auto state = std::make_shared<Interaction>();
        state->document = document;
        state->routing_mode = mode;
        state->mount_generation =
            next_graph_mount_generation.fetch_add(1, std::memory_order_relaxed);
        if (state->mount_generation == 0)
          throw std::overflow_error("GraphCanvas mount generation exhausted");
        flexUI::NativeCanvasWidget::Callbacks callbacks;
        callbacks.paint = [document, state](
            const flexUI::Element &element, flexUI::RenderCommandList &commands) {
          paint_canvas(*document, *state, element, commands);
        };
        callbacks.input = [document, state](
            flexUI::NativeCanvasWidget &widget, const flexUI::Event &event,
            flexUI::Element &element) {
          return handle_input(*state, *document, widget, event, element);
        };
        if (mode == GraphCanvasRoutingMode::AsyncCommit) {
          callbacks.frame = [document, state](float, flexUI::Element &elem) {
            poll_routing(*state, *document, elem);
          };
        }
        callbacks.mount = [document, state](flexUI::NativeCanvasWidget &widget,
                                            flexUI::Element &host) {
          build_semantic_parts(widget, host, *document, *state);
        };
        callbacks.sync_semantics = [document, state](flexUI::NativeCanvasWidget &,
                                                     flexUI::Element &host) {
          sync_semantic_parts(*document, *state, host);
        };
        // The Box alone reconciles dynamic semantic leaves after
        // EventDispatcher/frame/layout borrowers retire. No Element* or
        // renderer handles cross this immutable document callback.
        callbacks.parts_revision = [document] {
          return document->snapshot().revision;
        };
        callbacks.desired_parts = [document] {
          return desired_graph_semantics(*document);
        };
        callbacks.owns_part_paint = [](std::string_view name) {
          // The draw commands paint nodes/ports; their Box Elements provide
          // CSS query, metadata and geometry without duplicate box painting.
          return name.substr(0, 5) == "node:" ||
                 name.substr(0, 5) == "port:";
        };
        return std::make_unique<flexUI::NativeCanvasWidget>(std::move(callbacks));
      },
      flexUI::UiContentModel::Empty);
}

} // namespace charts::ui
