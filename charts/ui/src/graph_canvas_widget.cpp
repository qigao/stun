#include <charts/ui/graph_canvas_widget.h>

#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/native_canvas_widget.h>
#include <flexUI/render_command.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace charts::ui {
namespace {

constexpr double kMinZoom = 0.5;
constexpr double kMaxZoom = 3.0;
constexpr double kPortHitRadiusPx = 11.0;

enum class GestureMode { Idle, MoveNode, Connect, Pan };

struct Interaction {
  GraphCanvasViewport view;
  GestureMode mode = GestureMode::Idle;
  std::string selected_node;
  std::string source_port;
  std::string status_message;
  double press_x = 0.0;
  double press_y = 0.0;
  double previous_x = 0.0;
  double previous_y = 0.0;
  double cursor_x = 0.0;
  double cursor_y = 0.0;
  std::uint64_t edge_sequence = 1;
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

bool node_rect(const GraphEditNode &node, const Interaction &interaction,
               const GraphEditDocument &document, double &x, double &y,
               double &w, double &h) {
  double nx = node.x, ny = node.y;
  document.preview_position(node.id, nx, ny);
  w = node.width * interaction.view.zoom;
  h = node.height * interaction.view.zoom;
  return interaction.view.to_local(nx, ny, x, y);
}

const GraphEditNode *hit_node(const GraphEditDocument &model,
                               const Interaction &interaction,
                               double local_x, double local_y) {
  const auto &nodes = model.snapshot().nodes;
  for (auto i = nodes.rbegin(); i != nodes.rend(); ++i) {
    double x, y, w, h;
    if (node_rect(*i, interaction, model, x, y, w, h) &&
        local_x >= x && local_x <= x + w &&
        local_y >= y && local_y <= y + h) return &*i;
  }
  return nullptr;
}

bool port_anchor(const GraphEditSnapshot &snapshot, const GraphEditPort &port,
                 const Interaction &interaction, double &x, double &y) {
  const auto *node = find_node(snapshot, port.node_id);
  return node && interaction.view.to_local(
      node->x + port.local_x, node->y + port.local_y, x, y);
}

const GraphEditPort *hit_port(const GraphEditDocument &model,
                               const Interaction &interaction,
                               double local_x, double local_y,
                               bool output) {
  const auto &snapshot = model.snapshot();
  for (const auto &port : snapshot.ports) {
    if (port.output != output) continue;
    double x, y;
    if (!port_anchor(snapshot, port, interaction, x, y)) continue;
    const double dx = local_x - x, dy = local_y - y;
    if (dx * dx + dy * dy <= kPortHitRadiusPx * kPortHitRadiusPx)
      return &port;
  }
  return nullptr;
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

  const auto &snapshot = model.snapshot();
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
    commands.draw_text(node.id, static_cast<float>(x + 12.0),
                       static_cast<float>(y + 26.0), "Arial", 12.0F, true, light);
    if (node.locked)
      commands.draw_text("LOCKED", static_cast<float>(x + 12.0),
                         static_cast<float>(y + 47.0), "Arial", 10.0F, false, muted);
  }

  for (const auto &port : snapshot.ports) {
    double x, y;
    if (!port_anchor(snapshot, port, interaction, x, y)) continue;
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
      if (port_anchor(snapshot, port, interaction, x, y)) {
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
  } else if (!interaction.status_message.empty()) {
    commands.draw_text(interaction.status_message, 12.0F, height - 8.0F,
                       "Arial", 10.0F, false, muted);
  }
}

void cancel(Interaction &state, GraphEditDocument &document,
            flexUI::NativeCanvasWidget &widget, flexUI::Element &element) {
  if (state.mode == GestureMode::MoveNode) document.cancel_move();
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
    return false;
  }
  if (event.type == EventType::KeyDown) {
    if (event.key == KeyCode::Escape && state.mode != GestureMode::Idle) {
      cancel(state, model, widget, element);
      return true;
    }
    if (state.mode == GestureMode::Idle && (event.mods & static_cast<int>(flexUI::KeyMod::Control))) {
      GraphEditStatus result;
      if (event.key == KeyCode::Z) result = model.undo();
      else if (event.key == KeyCode::Y) result = model.redo();
      else return false;
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
    if (state.mode != GestureMode::Idle || !inside(element, x, y)) return false;
    state.press_x = state.previous_x = x;
    state.press_y = state.previous_y = y;
    if (event.button == MouseButton::Middle) {
      state.mode = GestureMode::Pan;
      widget.request_pointer_capture();
      return true;
    }
    if (event.button != MouseButton::Left) return false;
    if (const auto *source = hit_port(model, state, x, y, true)) {
      state.source_port = source->id;
      state.mode = GestureMode::Connect;
      widget.request_pointer_capture();
      element.mark_paint_dirty();
      return true;
    }
    if (const auto *node = hit_node(model, state, x, y)) {
      state.selected_node = node->id;
      const auto status = model.begin_move(node->id);
      state.status_message = status ? "" : status.message;
      if (status) {
        state.mode = GestureMode::MoveNode;
        widget.request_pointer_capture();
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
    } else if (state.mode == GestureMode::MoveNode) {
      const auto status = model.preview_move(
          (x - state.press_x) / state.view.zoom,
          (y - state.press_y) / state.view.zoom);
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
      const auto status = model.commit_move();
      state.status_message = status ? "" : status.message;
    } else if (matching && state.mode == GestureMode::Connect) {
      const auto *target = hit_port(model, state, x, y, false);
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
    flexUI::WidgetRegistry &registry, std::shared_ptr<GraphEditDocument> document) {
  if (!document) {
    return {flexUI::WidgetRegistryErrorCode::InvalidFactory,
            "GraphCanvas requires one application-owned graph document"};
  }
  return registry.register_widget(
      "native-graph",
      [document = std::move(document)](const flexUI::UiNodeDefinition &)
          -> std::unique_ptr<flexUI::Widget> {
        auto state = std::make_shared<Interaction>();
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
        return std::make_unique<flexUI::NativeCanvasWidget>(std::move(callbacks));
      },
      flexUI::UiContentModel::Empty);
}

} // namespace charts::ui
