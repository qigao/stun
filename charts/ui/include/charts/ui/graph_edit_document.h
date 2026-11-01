#pragma once

#include <stun/graphlayout/orthogonal.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace charts::ui {

// Graph document identity is independent of GraphCore's per-call node indices.
// These are application-owned data, not Box Elements or rendering resources.
struct GraphEditNode {
  std::string id;
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;
  bool locked = false;
};

struct GraphEditPort {
  std::string id;
  std::string node_id;
  stun::graphlayout::Side side = stun::graphlayout::Side::Auto;
  double local_x = 0.0;
  double local_y = 0.0;
  bool output = false;
};

struct GraphEditEdge {
  std::string id;
  std::string source_port;
  std::string target_port;
};

struct GraphEditRoute {
  std::string edge_id;
  std::vector<stun::graphlayout::Point> points;
};

struct GraphEditSnapshot {
  // Revision is monotone even across Undo/Redo; never restore an old revision.
  std::uint64_t revision = 0; // Legacy alias for document_revision.
  // Distinct fields are required for future async dirty routing. M0 commits
  // both together because every accepted edit recomputes full geometry.
  std::uint64_t document_revision = 0;
  std::uint64_t geometry_revision = 0;
  std::vector<GraphEditNode> nodes;
  std::vector<GraphEditPort> ports;
  std::vector<GraphEditEdge> edges;
  std::vector<GraphEditRoute> routes;
};

enum class GraphEditError {
  None,
  InvalidInput,
  InvalidGeometry,
  MissingNode,
  LockedNode,
  Busy,
  NoPath,
  CapacityExceeded,
  Conflict,
  InternalInvariant,
};

struct GraphEditStatus {
  GraphEditError error = GraphEditError::None;
  std::string message;
  explicit operator bool() const noexcept { return error == GraphEditError::None; }
};

// Per-commit accounting counts real A* routes, not the full obstacle/route
// postcondition validation (which M1a deliberately retains on the full graph).
// No UI or GPU pointers are stored with these application-owned diagnostics.
struct GraphRouteUpdateStats {
  std::uint64_t committed_revision = 0;
  std::size_t total_routes = 0;
  std::size_t rerouted = 0;
  std::size_t reused = 0;
  std::size_t incident_invalidated = 0;
  std::size_t obstacle_invalidated = 0;
  bool full_rebuild = false;
  // Counts reflect the actual sorted-interval segment index, not GPU draws.
  std::size_t indexed_segments = 0;
  std::size_t segment_candidates = 0;
  std::size_t index_queries = 0;
};

// One owner-thread edit session. M1a reuses previously validated paths only
// after conservative swept-obstacle invalidation and independent complete-graph
// postcondition validation. No asynchronous solver, cache server, or second
// GraphLayout engine is introduced.
class GraphEditDocument {
public:
  GraphEditStatus initialize(GraphEditSnapshot initial);
  const GraphEditSnapshot &snapshot() const noexcept { return committed_; }
  const GraphRouteUpdateStats &last_route_update() const noexcept {
    return route_stats_;
  }

  GraphEditStatus begin_move(std::string_view id);
  GraphEditStatus preview_move(double dx, double dy);
  GraphEditStatus commit_move();
  void cancel_move() noexcept { move_.reset(); }
  bool has_preview() const noexcept { return move_.has_value(); }
  bool preview_position(std::string_view id, double &x, double &y) const noexcept;

  GraphEditStatus connect(std::string source_port, std::string target_port,
                          std::string edge_id);
  GraphEditStatus undo();
  GraphEditStatus redo();
  std::size_t undo_count() const noexcept { return undo_.size(); }
  std::size_t redo_count() const noexcept { return redo_.size(); }

private:
  struct MovePreview {
    std::string id;
    double original_x = 0.0;
    double original_y = 0.0;
    double x = 0.0;
    double y = 0.0;
  };

  // M1b keeps one *committed-revision-owned* sorted interval index. It is
  // rebuilt from already-validated route geometry before any snapshot
  // publication, so an old revision can never supply a new route's index.
  // The prefix maximum permits binary search to skip all segments ending
  // strictly left of a swept obstacle.
  struct IndexedRouteSegment {
    std::string edge_id;
    double left = 0.0, right = 0.0, top = 0.0, bottom = 0.0;
    double prefix_max_right = 0.0;
  };
  static GraphEditStatus build_route_index(
      const GraphEditSnapshot &snapshot,
      std::vector<IndexedRouteSegment> &index);
  static GraphEditStatus validate_and_route(
      GraphEditSnapshot &next, const GraphEditSnapshot *previous,
      const std::vector<IndexedRouteSegment> *previous_index,
      GraphRouteUpdateStats &stats);
  GraphEditStatus publish(GraphEditSnapshot candidate);

  GraphEditSnapshot committed_;
  GraphRouteUpdateStats route_stats_;
  std::vector<IndexedRouteSegment> route_segments_;
  std::uint64_t route_index_revision_ = 0;
  std::optional<MovePreview> move_;
  std::vector<GraphEditSnapshot> undo_;
  std::vector<GraphEditSnapshot> redo_;
  static constexpr std::size_t kMaxHistory = 64;
};

} // namespace charts::ui
