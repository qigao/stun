#include <charts/ui/graph_edit_document.h>

#include <stun/graphlayout/port_bindings.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <limits>
#include <map>
#include <set>
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


struct SweptObstacle {
  // Conservative broad-phase union of OLD and NEW clearance-inflated bounds.
  // Touching a boundary may over-invalidate but never silently reuse an edge
  // that the moved obstacle could block at the committed destination.
  double left = 0.0, top = 0.0, right = 0.0, bottom = 0.0;
};

SweptObstacle swept_clearance(const GraphEditNode &old_node,
                              const GraphEditNode &new_node,
                              double clearance) noexcept {
  return {std::min(old_node.x, new_node.x) - clearance,
          std::min(old_node.y, new_node.y) - clearance,
          std::max(old_node.x + old_node.width,
                   new_node.x + new_node.width) + clearance,
          std::max(old_node.y + old_node.height,
                   new_node.y + new_node.height) + clearance};
}

bool same_route_schema(const GraphEditSnapshot &a,
                       const GraphEditSnapshot &b) noexcept {
  // A compound-proxy route must never be reused as an original-node route.
  // M2a conservatively fully routes every group-containing transaction.
  if (!a.groups.empty() || !b.groups.empty()) return false;
  // Dynamic node/port insertion and dimension/port changes are NOT M1a.
  // Rebuild all on such changes; do not reuse routes with stale anchors.
  if (a.nodes.size() != b.nodes.size() ||
      a.ports.size() != b.ports.size()) return false;
  for (std::size_t i = 0; i < a.nodes.size(); ++i) {
    const auto &x = a.nodes[i], &y = b.nodes[i];
    if (x.id != y.id || x.width != y.width || x.height != y.height)
      return false;
  }
  for (std::size_t i = 0; i < a.ports.size(); ++i) {
    const auto &x = a.ports[i], &y = b.ports[i];
    if (x.id != y.id || x.node_id != y.node_id ||
        x.side != y.side || x.local_x != y.local_x ||
        x.local_y != y.local_y || x.output != y.output) return false;
  }
  return true;
}

GraphEditError routing_error(gl::RouteError error) {
  switch (error) {
  case gl::RouteError::CapacityExceeded: return GraphEditError::CapacityExceeded;
  case gl::RouteError::NoPath: return GraphEditError::NoPath;
  case gl::RouteError::InternalInvariant: return GraphEditError::InternalInvariant;
  default: return GraphEditError::InvalidGeometry;
  }
}


GraphEditStatus project_visible_graph(const GraphEditSnapshot &source,
                                      GraphEditSnapshot &view) {
  // M2a is one flat, disjoint group layer. Complex/nested group geometry is
  // rejected instead of silently losing logical graph identity.
  if (source.nodes.size() > 256 || source.edges.size() > 1024 ||
      source.ports.size() > 4096 || source.groups.size() > 256)
    return reject(GraphEditError::CapacityExceeded,
                  "compound document exceeds native graph budget");

  std::map<std::string, const GraphEditNode *> nodes;
  gl::Layout original_layout;
  for (const auto &node : source.nodes) {
    if (!valid_id(node.id) || !finite_geometry(node) ||
        !nodes.emplace(node.id, &node).second)
      return reject(GraphEditError::InvalidInput,
                    "compound document has invalid or duplicate node");
    original_layout.nodes.push_back(
        {node.x, node.y, node.width, node.height, 0, 0});
  }
  // Hidden children must also be valid when expanded. Never persist a group
  // whose original geometry cannot be restored deterministically.
  for (std::size_t i = 0; i < source.nodes.size(); ++i)
    for (std::size_t j = i + 1; j < source.nodes.size(); ++j)
      if (overlap(source.nodes[i], source.nodes[j]))
        return reject(GraphEditError::InvalidGeometry,
                      "original group member rectangles overlap");

  std::map<std::string, std::size_t> node_indices;
  for (std::size_t i = 0; i < source.nodes.size(); ++i)
    node_indices.emplace(source.nodes[i].id, i);

  std::map<std::string, const GraphEditPort *> ports;
  std::vector<gl::MeasuredNamedPort> original_measurements;
  for (const auto &port : source.ports) {
    const auto n = node_indices.find(port.node_id);
    if (!valid_id(port.id) || n == node_indices.end() ||
        !ports.emplace(port.id, &port).second)
      return reject(GraphEditError::InvalidInput,
                    "compound document has duplicate or orphan port");
    if (!std::isfinite(port.local_x) || !std::isfinite(port.local_y))
      return reject(GraphEditError::InvalidGeometry,
                    "nonfinite original port measurement");
    original_measurements.push_back(
        {n->second, port.id, port.side, {port.local_x, port.local_y}});
  }
  gl::NamedPortBindings original_bindings;
  const auto originals = gl::bind_named_ports(
      original_layout, original_measurements, original_bindings);
  if (!originals)
    return reject(GraphEditError::InvalidGeometry,
                  "invalid original measured port: " + originals.message);

  std::set<std::string> edge_ids;
  for (const auto &edge : source.edges) {
    const auto a = ports.find(edge.source_port);
    const auto b = ports.find(edge.target_port);
    if (!valid_id(edge.id) || !edge_ids.insert(edge.id).second ||
        a == ports.end() || b == ports.end() ||
        !a->second->output || b->second->output)
      return reject(GraphEditError::InvalidInput,
                    "invalid original logical edge or endpoint");
  }

  std::set<std::string> group_ids;
  std::map<std::string, std::string> membership;
  std::map<std::string, std::string> hidden_members;
  std::map<std::string, std::string> proxy_for_original;
  std::set<std::string> all_proxy_ids;
  for (const auto &group : source.groups) {
    const std::string proxy_node = "group@" + group.id;
    GraphEditNode measured;
    measured.id = proxy_node;
    measured.x = group.x;
    measured.y = group.y;
    measured.width = group.width;
    measured.height = group.height;
    measured.locked = true;
    if (!valid_id(group.id) || !valid_id(proxy_node) ||
        !group_ids.insert(group.id).second ||
        nodes.count(proxy_node) || !finite_geometry(measured) ||
        group.member_node_ids.empty())
      return reject(GraphEditError::InvalidInput,
                    "duplicate, empty, or invalid measured group");

    for (const auto &id : group.member_node_ids) {
      if (nodes.find(id) == nodes.end() ||
          !membership.emplace(id, group.id).second)
        return reject(GraphEditError::InvalidInput,
                      "missing or multiply owned group member");
      if (group.collapsed) hidden_members.emplace(id, proxy_node);
    }

    std::set<std::string> local_bindings;
    for (const auto &binding : group.proxy_ports) {
      const auto original = ports.find(binding.original_port_id);
      if (!valid_id(binding.id) || original == ports.end() ||
          !all_proxy_ids.insert(binding.id).second ||
          ports.count(binding.id) || !std::isfinite(binding.local_x) ||
          !std::isfinite(binding.local_y) ||
          !local_bindings.insert(binding.original_port_id).second)
        return reject(GraphEditError::InvalidInput,
                      "invalid or duplicate group boundary port binding");
    }
  }

  // Validate ownership after all groups have been read. Group order cannot
  // change membership acceptance, group ports, or the final solver order.
  for (const auto &group : source.groups) {
    for (const auto &binding : group.proxy_ports) {
      const auto original = ports.find(binding.original_port_id);
      const auto member = membership.find(original->second->node_id);
      if (member == membership.end() || member->second != group.id)
        return reject(GraphEditError::InvalidInput,
                      "proxy port does not belong to its group");
      if (group.collapsed)
        proxy_for_original.emplace(binding.original_port_id, binding.id);
    }
  }

  view = {};
  view.revision = source.revision;
  view.document_revision = source.document_revision;
  view.geometry_revision = source.geometry_revision;
  view.routes = source.routes;
  for (const auto &node : source.nodes)
    if (!hidden_members.count(node.id)) view.nodes.push_back(node);
  for (const auto &port : source.ports)
    if (!hidden_members.count(port.node_id)) view.ports.push_back(port);
  for (const auto &group : source.groups) {
    if (!group.collapsed) continue;
    const std::string id = "group@" + group.id;
    view.nodes.push_back({id, group.x, group.y, group.width,
                          group.height, true});
    for (const auto &binding : group.proxy_ports) {
      const auto *original = ports.at(binding.original_port_id);
      view.ports.push_back({binding.id, id, binding.side,
                            binding.local_x, binding.local_y,
                            original->output});
    }
  }
  for (const auto &edge : source.edges) {
    const auto *from = ports.at(edge.source_port);
    const auto *to = ports.at(edge.target_port);
    const auto ga = hidden_members.find(from->node_id);
    const auto gb = hidden_members.find(to->node_id);
    if (ga != hidden_members.end() && gb != hidden_members.end() &&
        ga->second == gb->second)
      continue; // Internal topology remains in source, never in the view.
    GraphEditEdge visible_edge = edge;
    if (ga != hidden_members.end()) {
      const auto bound = proxy_for_original.find(edge.source_port);
      if (bound == proxy_for_original.end())
        return reject(GraphEditError::InvalidInput,
                      "missing explicit outgoing group boundary binding");
      visible_edge.source_port = bound->second;
    }
    if (gb != hidden_members.end()) {
      const auto bound = proxy_for_original.find(edge.target_port);
      if (bound == proxy_for_original.end())
        return reject(GraphEditError::InvalidInput,
                      "missing explicit incoming group boundary binding");
      visible_edge.target_port = bound->second;
    }
    view.edges.push_back(std::move(visible_edge));
  }
  return {};
}

} // namespace

GraphEditStatus GraphEditDocument::build_route_index(
    const GraphEditSnapshot &snapshot,
    std::vector<IndexedRouteSegment> &index,
    std::vector<IndexedRouteSegment> &index_y) {
  // Fail closed above a fixed bend budget instead of unbounded allocations.
  // This does NOT increase the native GraphOrthogonal 256/1024 caps.
  constexpr std::size_t kMaximumSegments = 131072;
  index.clear();
  for (const auto &route : snapshot.routes) {
    if (route.points.size() < 2)
      return reject(GraphEditError::InternalInvariant,
                    "indexed committed route is empty");
    for (std::size_t j = 1; j < route.points.size(); ++j) {
      const auto a = route.points[j - 1], b = route.points[j];
      if (!std::isfinite(a.x) || !std::isfinite(a.y) ||
          !std::isfinite(b.x) || !std::isfinite(b.y) ||
          (a.x != b.x && a.y != b.y) ||
          (a.x == b.x && a.y == b.y)) {
        return reject(GraphEditError::InternalInvariant,
                      "invalid indexed orthogonal segment");
      }
      if (index.size() == kMaximumSegments)
        return reject(GraphEditError::CapacityExceeded,
                      "bounded route segment index exceeded capacity");
      index.push_back({route.edge_id, std::min(a.x, b.x),
                       std::max(a.x, b.x), std::min(a.y, b.y),
                       std::max(a.y, b.y), 0.0, 0.0});
    }
  }
  std::sort(index.begin(), index.end(),
            [](const IndexedRouteSegment &a, const IndexedRouteSegment &b) {
              if (a.left != b.left) return a.left < b.left;
              if (a.right != b.right) return a.right < b.right;
              if (a.top != b.top) return a.top < b.top;
              if (a.bottom != b.bottom) return a.bottom < b.bottom;
              return a.edge_id < b.edge_id;
            });
  double maximum = -std::numeric_limits<double>::infinity();
  for (auto &segment : index) {
    maximum = std::max(maximum, segment.right);
    segment.prefix_max_right = maximum;
  }
  index_y = index;
  std::sort(index_y.begin(), index_y.end(),
            [](const IndexedRouteSegment &a, const IndexedRouteSegment &b) {
              if (a.top != b.top) return a.top < b.top;
              if (a.bottom != b.bottom) return a.bottom < b.bottom;
              if (a.left != b.left) return a.left < b.left;
              if (a.right != b.right) return a.right < b.right;
              return a.edge_id < b.edge_id;
            });
  double max_bottom = -std::numeric_limits<double>::infinity();
  for (auto &seg : index_y) {
    max_bottom = std::max(max_bottom, seg.bottom);
    seg.prefix_max_bottom = max_bottom;
  }
  return {};
}

GraphEditStatus GraphEditDocument::validate_and_route(
    GraphEditSnapshot &next, const GraphEditSnapshot *previous,
    const std::vector<IndexedRouteSegment> *previous_index,
    const std::vector<IndexedRouteSegment> *previous_index_y,
    GraphRouteUpdateStats &stats) {
  stats = {};
  if (!next.groups.empty()) {
    GraphEditSnapshot visible;
    const auto projected = project_visible_graph(next, visible);
    if (!projected) return projected;
    // The sole native GraphLayout receives one flat, measured view. M2a does
    // not try to reuse old routes across a compound-proxy transition.
    const auto routed = validate_and_route(
        visible, nullptr, nullptr, nullptr, stats);
    if (!routed) return routed;
    next.routes = std::move(visible.routes);
    return {};
  }
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

  stats.total_routes = next.edges.size();
  const bool reuse_allowed =
      previous && previous_index && previous_index_y &&
      previous_index->size() == previous_index_y->size() &&
      previous->revision != 0 &&
      same_route_schema(*previous, next) &&
      previous->routes.size() == previous->edges.size();
  stats.full_rebuild = !reuse_allowed;

  // The immutable previous-revision index narrows swept-obstacle tests by
  // x using prefix maximum-right bounds, followed by inclusive y overlap.
  // All cached and newly routed paths still receive full postcondition checks.
  std::set<std::string> moved_nodes;
  std::vector<SweptObstacle> swept_obstacles;
  std::map<std::string, const GraphEditEdge *> old_edges;
  std::map<std::string, const GraphEditRoute *> old_routes;
  std::set<std::string> swept_dirty_edges;
  if (reuse_allowed) {
    stats.queried_segments = previous_index->size();
    for (std::size_t i = 0; i < next.nodes.size(); ++i) {
      const auto &before = previous->nodes[i], &after = next.nodes[i];
      if (before.x == after.x && before.y == after.y) continue;
      moved_nodes.insert(after.id);
      swept_obstacles.push_back(swept_clearance(before, after, 8.0));
    }
    for (const auto &edge : previous->edges) old_edges.emplace(edge.id, &edge);
    for (const auto &route : previous->routes)
      old_routes.emplace(route.edge_id, &route);
    for (const auto &region : swept_obstacles) {
      ++stats.index_queries;
      // Evaluate both axis intervals from the SAME committed revision.
      // Use the smaller interval, then verify both dimensions. Inclusive
      // boundaries deliberately over-invalidate instead of missing a route.
      const auto x_upper = std::upper_bound(
          previous_index->begin(), previous_index->end(), region.right,
          [](double right, const IndexedRouteSegment &seg) {
            return right < seg.left;
          });
      const auto x_lower = std::lower_bound(
          previous_index->begin(), x_upper, region.left,
          [](const IndexedRouteSegment &seg, double left) {
            return seg.prefix_max_right < left;
          });
      const auto y_upper = std::upper_bound(
          previous_index_y->begin(), previous_index_y->end(), region.bottom,
          [](double bottom, const IndexedRouteSegment &seg) {
            return bottom < seg.top;
          });
      const auto y_lower = std::lower_bound(
          previous_index_y->begin(), y_upper, region.top,
          [](const IndexedRouteSegment &seg, double top) {
            return seg.prefix_max_bottom < top;
          });
      const bool use_y = (y_upper - y_lower) < (x_upper - x_lower);
      if (use_y) ++stats.y_axis_queries;
      const auto inspect = [&](const IndexedRouteSegment &seg) {
        ++stats.segment_candidates;
        if (seg.left <= region.right && seg.right >= region.left &&
            seg.top <= region.bottom && seg.bottom >= region.top)
          swept_dirty_edges.insert(seg.edge_id);
      };
      if (use_y) {
        for (auto it = y_lower; it != y_upper; ++it) inspect(*it);
      } else {
        for (auto it = x_lower; it != x_upper; ++it) inspect(*it);
      }
    }
  }

  std::vector<std::size_t> dirty_indices;
  std::vector<gl::RouteRequest> dirty_requests;
  gl::Routes combined;
  combined.edges.resize(next.edges.size());
  for (std::size_t i = 0; i < next.edges.size(); ++i) {
    const auto &edge = next.edges[i];
    bool incident = !reuse_allowed;
    const GraphEditRoute *prior_route = nullptr;
    if (reuse_allowed) {
      const auto prior_edge = old_edges.find(edge.id);
      const auto route = old_routes.find(edge.id);
      incident = prior_edge == old_edges.end() ||
                 prior_edge->second->source_port != edge.source_port ||
                 prior_edge->second->target_port != edge.target_port ||
                 route == old_routes.end();
      if (!incident) {
        const auto *from = port_by_id.at(edge.source_port);
        const auto *to = port_by_id.at(edge.target_port);
        incident = moved_nodes.count(from->node_id) != 0 ||
                   moved_nodes.count(to->node_id) != 0;
      }
      if (!incident) prior_route = route->second;
    }

    const bool obstructed = prior_route &&
                            swept_dirty_edges.count(edge.id) != 0;
    if (incident || obstructed) {
      dirty_indices.push_back(i);
      dirty_requests.push_back(requests[i]);
      if (incident) ++stats.incident_invalidated;
      else ++stats.obstacle_invalidated;
    } else {
      // Reuse the exact prior polyline in request-index order. This is only
      // provisional; the full combined postcondition is checked below.
      combined.edges[i].points = prior_route->points;
      ++stats.reused;
    }
  }

  if (!dirty_requests.empty()) {
    // IMPORTANT: same full layout, all of the new obstacle rectangles.
    // Never remove distant obstacles to make a partial route "succeed".
    gl::Routes rerouted;
    const auto status = gl::route_orthogonal(layout, dirty_requests, rerouted);
    if (!status) {
      return reject(routing_error(status.error),
                    "incremental route rejected: " + status.message);
    }
    if (rerouted.edges.size() != dirty_indices.size()) {
      return reject(GraphEditError::InternalInvariant,
                    "incremental router returned an incomplete edge batch");
    }
    for (std::size_t j = 0; j < dirty_indices.size(); ++j) {
      combined.edges[dirty_indices[j]] = std::move(rerouted.edges[j]);
    }
  }
  stats.rerouted = dirty_indices.size();

  // Mandatory complete-graph proof for every reused AND new route before
  // publishing any new positions, routes or Undo history. A missed dependency
  // fails the transaction; there is no silent full reroute/straight fallback.
  const auto checked = gl::validate_orthogonal_routes(layout, requests, combined);
  if (!checked) {
    return reject(GraphEditError::InvalidGeometry,
                  "combined graph route validation failed at edge " +
                  std::to_string(checked.route_index) + ": " + checked.message);
  }
  next.routes.clear();
  next.routes.reserve(next.edges.size());
  for (std::size_t i = 0; i < next.edges.size(); ++i) {
    next.routes.push_back({next.edges[i].id, std::move(combined.edges[i].points)});
  }
  return {};
}

GraphEditStatus GraphEditDocument::initialize(GraphEditSnapshot initial) {
  if (move_) return reject(GraphEditError::Busy, "cannot replace an active edit");
  // Replacement MUST consume a new revision. Reusing revision 1 could let a
  // worker from an earlier document incarnation pass a stale acceptance gate.
  if (committed_.revision == std::numeric_limits<std::uint64_t>::max())
    return reject(GraphEditError::CapacityExceeded, "graph revision exhausted");
  const auto next_revision = committed_.revision + 1;
  GraphRouteUpdateStats prepared_stats;
  const auto status = validate_and_route(initial, nullptr, nullptr, nullptr,
                                         prepared_stats);
  if (!status) return status;
  std::vector<IndexedRouteSegment> index, index_y;
  const auto indexed = build_route_index(initial, index, index_y);
  if (!indexed) return indexed;
  set_committed_revisions(initial, next_revision);
  prepared_stats.committed_revision = initial.revision;
  prepared_stats.indexed_segments = index.size();
  committed_ = std::move(initial);
  route_stats_ = prepared_stats;
  route_segments_.swap(index);
  route_segments_y_.swap(index_y);
  route_index_revision_ = committed_.revision;
  undo_.clear();
  redo_.clear();
  return {};
}

GraphEditStatus GraphEditDocument::begin_move(std::string_view id) {
  if (move_) return reject(GraphEditError::Busy, "another gesture is active");
  for (const auto &group : committed_.groups)
    if (group.collapsed &&
        std::find(group.member_node_ids.begin(),
                  group.member_node_ids.end(), id) !=
            group.member_node_ids.end())
      return reject(GraphEditError::LockedNode,
                    "node is hidden behind a collapsed group proxy");
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
  GraphRouteUpdateStats prepared_stats;
  const bool valid_index = route_index_revision_ == committed_.revision;
  const auto *index = valid_index ? &route_segments_ : nullptr;
  const auto *index_y = valid_index ? &route_segments_y_ : nullptr;
  const auto status = validate_and_route(candidate, &committed_, index, index_y,
                                         prepared_stats);
  if (!status) return status;
  std::vector<IndexedRouteSegment> next_index, next_index_y;
  const auto indexed = build_route_index(candidate, next_index, next_index_y);
  if (!indexed) return indexed;
  // All potentially failing validation runs before replacing the committed
  // document. History is bounded and stores complete validated snapshots.
  undo_.push_back(committed_);
  if (undo_.size() > kMaxHistory) undo_.erase(undo_.begin());
  redo_.clear();
  set_committed_revisions(candidate, committed_.revision + 1);
  prepared_stats.committed_revision = candidate.revision;
  prepared_stats.indexed_segments = next_index.size();
  committed_ = std::move(candidate);
  route_stats_ = prepared_stats;
  route_segments_.swap(next_index);
  route_segments_y_.swap(next_index_y);
  route_index_revision_ = committed_.revision;
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
  // UI may pass a visible proxy port. Persist only the original stable port
  // IDs, never an ephemeral collapsed-view edge endpoint.
  for (const auto &group : committed_.groups) {
    if (!group.collapsed) continue;
    for (const auto &binding : group.proxy_ports) {
      if (source_port == binding.id) source_port = binding.original_port_id;
      if (target_port == binding.id) target_port = binding.original_port_id;
    }
  }
  GraphEditSnapshot candidate = committed_;
  candidate.edges.push_back(
      {std::move(edge_id), std::move(source_port), std::move(target_port)});
  return publish(std::move(candidate));
}

GraphEditStatus GraphEditDocument::visible_snapshot(
    GraphEditSnapshot &view) const {
  if (committed_.groups.empty()) {
    view = committed_;
    return {};
  }
  return project_visible_graph(committed_, view);
}

GraphEditStatus GraphEditDocument::add_group(GraphEditGroup group) {
  if (move_) return reject(GraphEditError::Busy, "cannot group during drag");
  GraphEditSnapshot candidate = committed_;
  candidate.groups.push_back(std::move(group));
  return publish(std::move(candidate));
}

GraphEditStatus GraphEditDocument::remove_group(std::string_view group_id) {
  if (move_) return reject(GraphEditError::Busy, "cannot ungroup during drag");
  GraphEditSnapshot candidate = committed_;
  const auto it = std::find_if(candidate.groups.begin(), candidate.groups.end(),
      [&](const GraphEditGroup &group) { return group.id == group_id; });
  if (it == candidate.groups.end())
    return reject(GraphEditError::MissingGroup, "unknown stable group ID");
  candidate.groups.erase(it);
  return publish(std::move(candidate));
}

GraphEditStatus GraphEditDocument::set_group_collapsed(
    std::string_view group_id, bool collapsed) {
  if (move_) return reject(GraphEditError::Busy, "cannot collapse during drag");
  GraphEditSnapshot candidate = committed_;
  for (auto &group : candidate.groups) {
    if (group.id != group_id) continue;
    if (group.collapsed == collapsed) return {}; // No-op, no history entry.
    group.collapsed = collapsed;
    return publish(std::move(candidate));
  }
  return reject(GraphEditError::MissingGroup, "unknown stable group ID");
}

GraphEditStatus GraphEditDocument::undo() {
  if (move_) return reject(GraphEditError::Busy, "cancel the active edit first");
  if (undo_.empty()) return reject(GraphEditError::Conflict, "no undo available");
  if (committed_.revision == std::numeric_limits<std::uint64_t>::max()) {
    return reject(GraphEditError::CapacityExceeded, "graph revision exhausted");
  }
  std::vector<IndexedRouteSegment> restored_index, restored_index_y;
  const auto indexed = build_route_index(undo_.back(), restored_index, restored_index_y);
  if (!indexed) return indexed;
  redo_.push_back(committed_);
  if (redo_.size() > kMaxHistory) redo_.erase(redo_.begin());
  auto previous = std::move(undo_.back());
  undo_.pop_back();
  set_committed_revisions(previous, committed_.revision + 1);
  committed_ = std::move(previous);
  route_stats_ = {committed_.revision, committed_.routes.size(),
                  0, committed_.routes.size(), 0, 0, false};
  route_segments_.swap(restored_index);
  route_segments_y_.swap(restored_index_y);
  route_index_revision_ = committed_.revision;
  route_stats_.indexed_segments = route_segments_.size();
  return {};
}

GraphEditStatus GraphEditDocument::redo() {
  if (move_) return reject(GraphEditError::Busy, "cancel the active edit first");
  if (redo_.empty()) return reject(GraphEditError::Conflict, "no redo available");
  if (committed_.revision == std::numeric_limits<std::uint64_t>::max()) {
    return reject(GraphEditError::CapacityExceeded, "graph revision exhausted");
  }
  std::vector<IndexedRouteSegment> restored_index, restored_index_y;
  const auto indexed = build_route_index(redo_.back(), restored_index, restored_index_y);
  if (!indexed) return indexed;
  undo_.push_back(committed_);
  if (undo_.size() > kMaxHistory) undo_.erase(undo_.begin());
  auto next = std::move(redo_.back());
  redo_.pop_back();
  set_committed_revisions(next, committed_.revision + 1);
  committed_ = std::move(next);
  route_stats_ = {committed_.revision, committed_.routes.size(),
                  0, committed_.routes.size(), 0, 0, false};
  route_segments_.swap(restored_index);
  route_segments_y_.swap(restored_index_y);
  route_index_revision_ = committed_.revision;
  route_stats_.indexed_segments = route_segments_.size();
  return {};
}


GraphEditStatus GraphEditDocument::accept_prepared_move(
    PreparedMove &&prepared, std::uint64_t current_mount_generation) {
  // Only the original document owner may publish the worker's geometry.
  if (std::this_thread::get_id() != owner_thread_)
    return reject(GraphEditError::Conflict, "wrong graph document owner thread");
  if (move_) return reject(GraphEditError::Busy, "owner gesture still active");
  if (prepared.cancelled_ &&
      prepared.cancelled_->load(std::memory_order_acquire))
    return reject(GraphEditError::Conflict, "worker operation was canceled");
  if (!prepared.status_) return prepared.status_;
  if (prepared.mount_generation_ == 0 ||
      current_mount_generation != prepared.mount_generation_)
    return reject(GraphEditError::Conflict, "graph widget mount generation changed");
  if (prepared.expected_revision_ != committed_.revision ||
      prepared.expected_document_revision_ != committed_.document_revision ||
      prepared.expected_geometry_revision_ != committed_.geometry_revision)
    return reject(GraphEditError::Conflict, "stale graph snapshot revision");
  if (committed_.revision == std::numeric_limits<std::uint64_t>::max())
    return reject(GraphEditError::CapacityExceeded, "graph revision exhausted");

  // The only possible success candidate came from the sealed, in-tree
  // GraphRouteWorker and passed the full GraphOrthogonal postcondition
  // before it left the worker. No caller can rewrite the private buffers.
  undo_.push_back(committed_);
  if (undo_.size() > kMaxHistory) undo_.erase(undo_.begin());
  redo_.clear();
  set_committed_revisions(prepared.next_, committed_.revision + 1);
  prepared.stats_.committed_revision = prepared.next_.revision;
  prepared.stats_.indexed_segments = prepared.index_.size();
  committed_ = std::move(prepared.next_);
  route_stats_ = prepared.stats_;
  route_segments_.swap(prepared.index_);
  route_segments_y_.swap(prepared.index_y_);
  route_index_revision_ = committed_.revision;
  return {};
}

GraphRouteWorker::~GraphRouteWorker() {
  cancel();
  wait(); // Join bounded native GraphOrthogonal; never detach a worker.
}

GraphEditStatus GraphRouteWorker::submit_move(
    const GraphEditDocument &owner, std::string node_id,
    double dx, double dy, std::uint64_t mount_generation) {
  if (future_.valid())
    return reject(GraphEditError::Busy, "worker already has an outstanding task");
  if (std::this_thread::get_id() != owner.owner_thread_)
    return reject(GraphEditError::Conflict, "submit from a non-owner thread");
  if (owner.has_preview())
    return reject(GraphEditError::Busy, "synchronous graph gesture is active");
  if (owner.committed_.revision == 0 ||
      owner.route_index_revision_ != owner.committed_.revision ||
      mount_generation == 0)
    return reject(GraphEditError::Conflict, "missing stable graph or widget generation");
  if (!std::isfinite(dx) || !std::isfinite(dy) ||
      std::abs(dx) > 1.0e6 || std::abs(dy) > 1.0e6)
    return reject(GraphEditError::InvalidGeometry, "invalid worker move displacement");

  // Replacing a finished task revokes any result that was polled but not
  // yet published by the document owner. A stale handoff never remains
  // acceptable merely because the future was already consumed.
  if (cancellation_) cancellation_->store(true, std::memory_order_release);

  // These are the ONLY values captured by the worker. It cannot reach the
  // live document, a Box subtree, Widget, Element or render resource.
  GraphEditSnapshot baseline = owner.committed_;
  auto old_index = owner.route_segments_;
  auto old_index_y = owner.route_segments_y_;
  auto flag = std::make_shared<std::atomic_bool>(false);
  try {
    future_ = std::async(
        std::launch::async,
        [baseline = std::move(baseline), old_index = std::move(old_index),
         old_index_y = std::move(old_index_y),
         flag, id = std::move(node_id), dx, dy, mount_generation]() mutable {
          GraphEditDocument::PreparedMove result;
          result.cancelled_ = flag;
          result.expected_revision_ = baseline.revision;
          result.expected_document_revision_ = baseline.document_revision;
          result.expected_geometry_revision_ = baseline.geometry_revision;
          result.mount_generation_ = mount_generation;
          const auto cancelled = [&]() {
            if (!flag->load(std::memory_order_acquire)) return false;
            result.status_ = reject(GraphEditError::Conflict,
                                    "worker operation was canceled");
            return true;
          };
          if (cancelled()) return result;

          result.next_ = baseline;
          bool found = false;
          for (auto &node : result.next_.nodes) {
            if (node.id != id) continue;
            found = true;
            if (node.locked) {
              result.status_ = reject(GraphEditError::LockedNode,
                                      "worker move target is locked");
              return result;
            }
            node.x += dx;
            node.y += dy;
            break;
          }
          if (!found) {
            result.status_ = reject(GraphEditError::MissingNode,
                                    "worker move target is missing");
            return result;
          }
          // This step may run bounded A* in the one, existing GraphLayout
          // engine. No preemption inside that engine is claimed.
          result.status_ = GraphEditDocument::validate_and_route(
              result.next_, &baseline, &old_index, &old_index_y,
              result.stats_);
          if (!result.status_ || cancelled()) return result;
          result.status_ = GraphEditDocument::build_route_index(
              result.next_, result.index_, result.index_y_);
          if (cancelled()) return result;
          return result;
        });
    cancellation_ = std::move(flag);
  } catch (const std::exception &error) {
    return reject(GraphEditError::InternalInvariant,
                  std::string("failed to launch graph worker: ") + error.what());
  }
  return {};
}

std::optional<GraphEditDocument::PreparedMove> GraphRouteWorker::poll() {
  if (!future_.valid() ||
      future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    return std::nullopt;
  GraphEditDocument::PreparedMove completed;
  try {
    completed = future_.get();
  } catch (const std::exception &error) {
    completed.status_ = reject(GraphEditError::InternalInvariant,
                               std::string("graph worker failed: ") + error.what());
  } catch (...) {
    completed.status_ = reject(GraphEditError::InternalInvariant,
                               "graph worker failed with unknown exception");
  }
  completed.cancelled_ = cancellation_;
  if (cancellation_ &&
      cancellation_->load(std::memory_order_acquire))
    completed.status_ = reject(GraphEditError::Conflict,
                               "graph worker canceled before result publication");
  // Keep the cancellation token until the next submission or worker
  // retirement. Even AFTER poll(), cancel() must revoke the prepared result.
  return std::optional<GraphEditDocument::PreparedMove>{std::move(completed)};
}

void GraphRouteWorker::cancel() noexcept {
  if (cancellation_) cancellation_->store(true, std::memory_order_release);
}

void GraphRouteWorker::wait() noexcept {
  if (future_.valid()) future_.wait();
}

} // namespace charts::ui
