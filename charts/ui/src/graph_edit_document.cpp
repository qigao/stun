#include <charts/ui/graph_edit_document.h>

#include <stun/graphlayout/port_bindings.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

namespace charts::ui {
namespace {

namespace gl = stun::graphlayout;

GraphEditStatus reject(GraphEditError error, std::string message) {
  return {error, std::move(message)};
}

bool valid_id(const std::string &id) {
  return !id.empty() && id.size() <= 128;
}

bool finite_geometry(const GraphEditNode &n) {
  constexpr double kMaxCoordinate = 1.0e6;
  return std::isfinite(n.x) && std::isfinite(n.y) &&
         std::isfinite(n.width) && std::isfinite(n.height) &&
         n.width > 0.0 && n.height > 0.0 &&
         std::abs(n.x) <= kMaxCoordinate &&
         std::abs(n.y) <= kMaxCoordinate &&
         n.width <= kMaxCoordinate && n.height <= kMaxCoordinate &&
         std::abs(n.x + n.width) <= kMaxCoordinate &&
         std::abs(n.y + n.height) <= kMaxCoordinate;
}

bool overlap(const GraphEditNode &a, const GraphEditNode &b) {
  return a.x < b.x + b.width && b.x < a.x + a.width &&
         a.y < b.y + b.height && b.y < a.y + a.height;
}

void set_committed_revisions(GraphEditSnapshot &snapshot,
                             std::uint64_t revision) noexcept {
  snapshot.revision = revision;
  snapshot.document_revision = revision;
  snapshot.geometry_revision = revision;
}

GraphEditError routing_error(gl::RouteError error) {
  switch (error) {
  case gl::RouteError::CapacityExceeded: return GraphEditError::CapacityExceeded;
  case gl::RouteError::NoPath: return GraphEditError::NoPath;
  case gl::RouteError::InternalInvariant: return GraphEditError::InternalInvariant;
  default: return GraphEditError::InvalidGeometry;
  }
}

} // namespace

GraphEditStatus GraphEditDocument::validate_and_route(GraphEditSnapshot &next) {
  // The existing geometric router's bounds remain hard constraints, even if
  // the caller could draw a larger graph without routing any connectors.
  if (next.nodes.size() > 256 || next.edges.size() > 1024 ||
      next.ports.size() > 4096) {
    return reject(GraphEditError::CapacityExceeded,
                  "graph exceeds the native routing or port capacity");
  }

  auto by_id = [](const auto &a, const auto &b) { return a.id < b.id; };
  std::sort(next.nodes.begin(), next.nodes.end(), by_id);
  std::sort(next.ports.begin(), next.ports.end(), by_id);
  std::sort(next.edges.begin(), next.edges.end(), by_id);

  gl::Layout layout;
  layout.nodes.reserve(next.nodes.size());
  std::map<std::string, std::size_t> node_indices;
  for (const auto &node : next.nodes) {
    if (!valid_id(node.id) ||
        !node_indices.emplace(node.id, layout.nodes.size()).second) {
      return reject(GraphEditError::InvalidInput, "duplicate or invalid node ID");
    }
    if (!finite_geometry(node)) {
      return reject(GraphEditError::InvalidGeometry, "nonfinite or invalid node rectangle");
    }
    layout.nodes.push_back({node.x, node.y, node.width, node.height, 0, 0});
  }
  for (std::size_t i = 0; i < next.nodes.size(); ++i) {
    for (std::size_t j = i + 1; j < next.nodes.size(); ++j) {
      if (overlap(next.nodes[i], next.nodes[j])) {
        return reject(GraphEditError::InvalidGeometry,
                      "node rectangles overlap; manual drag cannot move other nodes");
      }
    }
  }

  std::map<std::string, const GraphEditPort *> port_by_id;
  std::vector<gl::MeasuredNamedPort> measurements;
  measurements.reserve(next.ports.size());
  for (const auto &port : next.ports) {
    const auto node = node_indices.find(port.node_id);
    if (!valid_id(port.id) || node == node_indices.end() ||
        !port_by_id.emplace(port.id, &port).second) {
      return reject(GraphEditError::InvalidInput, "duplicate port ID or missing node");
    }
    if (!std::isfinite(port.local_x) || !std::isfinite(port.local_y)) {
      return reject(GraphEditError::InvalidGeometry, "nonfinite local port anchor");
    }
    measurements.push_back(
        {node->second, port.id, port.side, {port.local_x, port.local_y}});
  }

  gl::NamedPortBindings bindings;
  const auto bound = gl::bind_named_ports(layout, measurements, bindings);
  if (!bound) {
    return reject(bound.error == gl::NamedPortError::CapacityExceeded
                      ? GraphEditError::CapacityExceeded : GraphEditError::InvalidGeometry,
                  "invalid measured boundary port: " + bound.message);
  }

  std::vector<gl::RouteRequest> requests;
  requests.reserve(next.edges.size());
  std::string previous_edge_id;
  for (const auto &edge : next.edges) {
    if (!valid_id(edge.id) || (!previous_edge_id.empty() &&
                               previous_edge_id == edge.id)) {
      return reject(GraphEditError::InvalidInput, "duplicate or invalid edge ID");
    }
    previous_edge_id = edge.id;
    const auto source = port_by_id.find(edge.source_port);
    const auto target = port_by_id.find(edge.target_port);
    if (source == port_by_id.end() || target == port_by_id.end() ||
        !source->second->output || target->second->output) {
      return reject(GraphEditError::InvalidInput, "edge requires real output and input ports");
    }
    const auto source_node = node_indices.find(source->second->node_id);
    const auto target_node = node_indices.find(target->second->node_id);
    const auto *from = gl::find_named_port(bindings, source_node->second,
                                           source->second->id);
    const auto *to = gl::find_named_port(bindings, target_node->second,
                                         target->second->id);
    if (!from || !to) {
      return reject(GraphEditError::InvalidGeometry, "edge port was not measured");
    }
    requests.push_back({from->port, to->port});
  }

  next.routes.clear();
  if (requests.empty()) return {};

  gl::Routes calculated;
  const auto status = gl::route_orthogonal(layout, requests, calculated);
  if (!status) {
    return reject(routing_error(status.error), "graph routing rejected: " + status.message);
  }
  const auto checked = gl::validate_orthogonal_routes(layout, requests, calculated);
  if (!checked || calculated.edges.size() != next.edges.size()) {
    return reject(GraphEditError::InvalidGeometry,
                  "graph route failed independent obstacle/terminal validation");
  }
  next.routes.reserve(next.edges.size());
  for (std::size_t i = 0; i < next.edges.size(); ++i) {
    next.routes.push_back({next.edges[i].id, std::move(calculated.edges[i].points)});
  }
  return {};
}

GraphEditStatus GraphEditDocument::initialize(GraphEditSnapshot initial) {
  if (move_) return reject(GraphEditError::Busy, "cannot replace an active edit");
  const auto status = validate_and_route(initial);
  if (!status) return status;
  set_committed_revisions(initial, 1);
  committed_ = std::move(initial);
  undo_.clear();
  redo_.clear();
  return {};
}

GraphEditStatus GraphEditDocument::begin_move(std::string_view id) {
  if (move_) return reject(GraphEditError::Busy, "another gesture is active");
  for (const auto &node : committed_.nodes) {
    if (node.id == id) {
      if (node.locked) return reject(GraphEditError::LockedNode, "node is locked");
      move_ = MovePreview{node.id, node.x, node.y, node.x, node.y};
      return {};
    }
  }
  return reject(GraphEditError::MissingNode, "unknown stable node ID");
}

GraphEditStatus GraphEditDocument::preview_move(double dx, double dy) {
  if (!move_) return reject(GraphEditError::Conflict, "no active node gesture");
  const double x = move_->original_x + dx;
  const double y = move_->original_y + dy;
  if (!std::isfinite(x) || !std::isfinite(y) ||
      std::abs(x) > 1.0e6 || std::abs(y) > 1.0e6) {
    return reject(GraphEditError::InvalidGeometry, "invalid drag preview coordinates");
  }
  move_->x = x;
  move_->y = y;
  return {};
}

bool GraphEditDocument::preview_position(std::string_view id, double &x,
                                          double &y) const noexcept {
  if (!move_ || move_->id != id) return false;
  x = move_->x;
  y = move_->y;
  return true;
}

GraphEditStatus GraphEditDocument::publish(GraphEditSnapshot candidate) {
  if (committed_.revision == std::numeric_limits<std::uint64_t>::max()) {
    return reject(GraphEditError::CapacityExceeded, "graph revision exhausted");
  }
  const auto status = validate_and_route(candidate);
  if (!status) return status;
  // All potentially failing validation runs before replacing the committed
  // document. History is bounded and stores complete validated snapshots.
  undo_.push_back(committed_);
  if (undo_.size() > kMaxHistory) undo_.erase(undo_.begin());
  redo_.clear();
  set_committed_revisions(candidate, committed_.revision + 1);
  committed_ = std::move(candidate);
  return {};
}

GraphEditStatus GraphEditDocument::commit_move() {
  if (!move_) return reject(GraphEditError::Conflict, "no pending move to commit");
  const auto preview = std::move(*move_);
  move_.reset();
  if (preview.x == preview.original_x && preview.y == preview.original_y) {
    return {}; // A click does not add an Undo step or increment revision.
  }
  GraphEditSnapshot candidate = committed_;
  for (auto &node : candidate.nodes) {
    if (node.id == preview.id) {
      node.x = preview.x;
      node.y = preview.y;
      return publish(std::move(candidate));
    }
  }
  return reject(GraphEditError::Conflict, "dragged node was retired");
}

GraphEditStatus GraphEditDocument::connect(std::string source_port,
                                            std::string target_port,
                                            std::string edge_id) {
  if (move_) return reject(GraphEditError::Busy, "cannot connect during node drag");
  GraphEditSnapshot candidate = committed_;
  candidate.edges.push_back(
      {std::move(edge_id), std::move(source_port), std::move(target_port)});
  return publish(std::move(candidate));
}

GraphEditStatus GraphEditDocument::undo() {
  if (move_) return reject(GraphEditError::Busy, "cancel the active edit first");
  if (undo_.empty()) return reject(GraphEditError::Conflict, "no undo available");
  if (committed_.revision == std::numeric_limits<std::uint64_t>::max()) {
    return reject(GraphEditError::CapacityExceeded, "graph revision exhausted");
  }
  redo_.push_back(committed_);
  if (redo_.size() > kMaxHistory) redo_.erase(redo_.begin());
  auto previous = std::move(undo_.back());
  undo_.pop_back();
  set_committed_revisions(previous, committed_.revision + 1);
  committed_ = std::move(previous);
  return {};
}

GraphEditStatus GraphEditDocument::redo() {
  if (move_) return reject(GraphEditError::Busy, "cancel the active edit first");
  if (redo_.empty()) return reject(GraphEditError::Conflict, "no redo available");
  if (committed_.revision == std::numeric_limits<std::uint64_t>::max()) {
    return reject(GraphEditError::CapacityExceeded, "graph revision exhausted");
  }
  undo_.push_back(committed_);
  if (undo_.size() > kMaxHistory) undo_.erase(undo_.begin());
  auto next = std::move(redo_.back());
  redo_.pop_back();
  set_committed_revisions(next, committed_.revision + 1);
  committed_ = std::move(next);
  return {};
}

} // namespace charts::ui
