#pragma once

#include <stun/graphlayout/orthogonal.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <thread>
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

// M2a: compound identity belongs to the logical document. The routing view
// never owns these original nodes or their edges. No nested-group transform
// or automatic proxy-port synthesis is implemented in this first slice.
struct GraphEditGroupProxyPort {
  std::string id;               // Stable, globally unique visible proxy port.
  std::string original_port_id; // Preserved endpoint of the logical edge.
  stun::graphlayout::Side side = stun::graphlayout::Side::Auto;
  double local_x = 0.0;
  double local_y = 0.0;
};

struct GraphEditGroup {
  std::string id;
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;
  bool collapsed = false;
  std::vector<std::string> member_node_ids;
  std::vector<GraphEditGroupProxyPort> proxy_ports;
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
  // Stored even while collapsed. Internal nodes/ports/edges keep their IDs.
  std::vector<GraphEditGroup> groups;
};

enum class GraphEditError {
  None,
  InvalidInput,
  InvalidGeometry,
  MissingNode,
  MissingGroup,
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
  std::size_t indexed_segments = 0; // Size of NEW committed route index.
  std::size_t queried_segments = 0; // Size of OLD index used by the query.
  std::size_t segment_candidates = 0;
  std::size_t index_queries = 0;
  std::size_t y_axis_queries = 0; // Selected more selective Y-index queries.
};

class GraphRouteWorker;

// One owner-thread edit session. M1a reuses previously validated paths only
// after conservative swept-obstacle invalidation and independent complete-graph
// postcondition validation. No asynchronous solver, cache server, or second
// GraphLayout engine is introduced.
class GraphEditDocument {
public:
  class PreparedMove;
  GraphEditStatus initialize(GraphEditSnapshot initial);
  const GraphEditSnapshot &snapshot() const noexcept { return committed_; }
  // Build an ephemeral flat routing/painting view. Group proxy geometry and
  // explicitly measured boundary ports replace hidden children; internal
  // edges stay in the logical snapshot but disappear from the visible view.
  GraphEditStatus visible_snapshot(GraphEditSnapshot &view) const;
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
  GraphEditStatus add_group(GraphEditGroup group);
  GraphEditStatus remove_group(std::string_view group_id);
  GraphEditStatus set_group_collapsed(std::string_view group_id,
                                      bool collapsed);
  GraphEditStatus undo();
  GraphEditStatus redo();

  // Optional M1b owner-thread acceptance of geometry prepared from an
  // immutable worker snapshot. This is independent from the synchronous
  // editor's default gesture path.
  GraphEditStatus accept_prepared_move(PreparedMove &&prepared,
                                       std::uint64_t current_mount_generation);
  std::size_t undo_count() const noexcept { return undo_.size(); }
  std::size_t redo_count() const noexcept { return redo_.size(); }

private:
  friend class GraphRouteWorker;
  const std::thread::id owner_thread_ = std::this_thread::get_id();
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
    double prefix_max_bottom = 0.0;
  };

public:
  // An opaque result created only by GraphRouteWorker. No application code
  // may forge route buffers or rewrite the version/mount preconditions.
  class PreparedMove {
  public:
    PreparedMove() = default;
    const GraphEditStatus &status() const noexcept { return status_; }
    std::uint64_t baseline_revision() const noexcept {
      return expected_revision_;
    }
  private:
    friend class GraphEditDocument;
    friend class GraphRouteWorker;
    GraphEditSnapshot next_;
    GraphRouteUpdateStats stats_;
    std::vector<IndexedRouteSegment> index_;
    std::vector<IndexedRouteSegment> index_y_;
    GraphEditStatus status_{GraphEditError::Conflict, "empty worker result"};
    std::shared_ptr<std::atomic_bool> cancelled_;
    std::uint64_t expected_revision_ = 0;
    std::uint64_t expected_document_revision_ = 0;
    std::uint64_t expected_geometry_revision_ = 0;
    std::uint64_t mount_generation_ = 0;
  };

private:
  static GraphEditStatus build_route_index(
      const GraphEditSnapshot &snapshot,
      std::vector<IndexedRouteSegment> &index,
      std::vector<IndexedRouteSegment> &index_y);
  static GraphEditStatus validate_and_route(
      GraphEditSnapshot &next, const GraphEditSnapshot *previous,
      const std::vector<IndexedRouteSegment> *previous_index,
      const std::vector<IndexedRouteSegment> *previous_index_y,
      GraphRouteUpdateStats &stats);
  GraphEditStatus publish(GraphEditSnapshot candidate);

  GraphEditSnapshot committed_;
  GraphRouteUpdateStats route_stats_;
  std::vector<IndexedRouteSegment> route_segments_;
  std::vector<IndexedRouteSegment> route_segments_y_;
  std::uint64_t route_index_revision_ = 0;
  std::optional<MovePreview> move_;
  std::vector<GraphEditSnapshot> undo_;
  std::vector<GraphEditSnapshot> redo_;
  static constexpr std::size_t kMaxHistory = 64;
};

// A single-flight, opt-in worker. It copies GraphEditSnapshot and the
// revision-owned segment index while on the document's UI owner thread.
// The worker never reads a GraphEditDocument, Box, Element or GPU handle.
// Cancellation is cooperative before/after the bounded native A* call;
// it cannot preempt an in-progress native GraphOrthogonal search.
class GraphRouteWorker {
public:
  GraphRouteWorker() = default;
  ~GraphRouteWorker();
  GraphRouteWorker(const GraphRouteWorker &) = delete;
  GraphRouteWorker &operator=(const GraphRouteWorker &) = delete;

  GraphEditStatus submit_move(const GraphEditDocument &owner,
                              std::string node_id, double dx, double dy,
                              std::uint64_t mount_generation);
  // Nonblocking. A ready result must still pass owner revision+generation
  // validation. A canceled job cannot be published.
  std::optional<GraphEditDocument::PreparedMove> poll();
  void cancel() noexcept;
  // Explicit join, also performed on destruction; no detached threads.
  void wait() noexcept;
  bool pending() const noexcept { return future_.valid(); }

private:
  std::future<GraphEditDocument::PreparedMove> future_;
  std::shared_ptr<std::atomic_bool> cancellation_;
};

} // namespace charts::ui
