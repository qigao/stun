#pragma once

#include "stun/graphlayout/graph.h"
#include "stun/graphlayout/vpsc.h"

#include <cstddef>
#include <string>
#include <vector>

namespace stun::graphlayout {

// Geometry-only graph constraint projection. This is neither UI layout nor
// a force-directed solver. All coordinates, sizes, and distances are in
// the caller's graph coordinate system.
enum class ProjectionAxis { X, Y };

struct NodePin {
    std::size_t node = 0;
    double x = 0.0; // Exact top-left coordinate of the pinned rectangle.
    double y = 0.0;
};

struct NodeAlignment {
    ProjectionAxis axis = ProjectionAxis::X;
    std::size_t first = 0;
    std::size_t second = 0;
    // Center(second) - Center(first) == center_offset.
    double center_offset = 0.0;
};

struct NodeSeparation {
    ProjectionAxis axis = ProjectionAxis::X;
    std::size_t before = 0;
    std::size_t after = 0;
    // Minimum gap between the node rectangles along the chosen axis.
    double gap = 0.0;
};

struct ProjectionConstraints {
    std::vector<NodePin> pins;
    std::vector<NodeAlignment> alignments;
    std::vector<NodeSeparation> separations;
    bool avoid_overlaps = true;
};

struct ProjectionOptions {
    double clearance = 8.0; // Additional gap when avoid_overlaps is true.
    std::size_t max_passes = 16;
    std::size_t max_generated_separations = 8192;
    std::size_t max_pair_checks = 2000000;
    // Independent bounded, certified VPSC solves along X and Y.
    VpscOptions vpsc;
};

struct ProjectionReport {
    std::size_t passes = 0;
    std::size_t generated_separations = 0;
    std::size_t pair_checks = 0;
    VpscCertificate x_certificate;
    VpscCertificate y_certificate;
};

enum class ProjectionError {
    None,
    InvalidInput,
    InvalidOptions,
    CapacityExceeded,
    Infeasible,
    IterationLimit,
    InvalidGeometry,
};

struct ProjectionStatus {
    ProjectionError error = ProjectionError::None;
    std::string message;
    // A VPSC error when a 1D axis solve failed (if any).
    VpscError vpsc_error = VpscError::None;
    explicit operator bool() const { return error == ProjectionError::None; }
};

// Project an existing graph layout onto hard pins, center alignment and
// minimum rectangle separations. When enabled, non-overlap constraints are
// added monotonically for detected rectangle overlaps (deterministic
// disjunctive-axis heuristic; NOT globally optimal graph compaction).
//
// On success all pins are exact, axis constraints are numerically certified
// by VPSC, and every pair of node rectangles is independently checked for
// non-overlap with clearance. All original node IDs/order/ranks/SCCs stay
// unchanged. Coordinates can be negative if exact pins require it.
//
// The output and optional report are reset on failure; no partial success,
// no artificial pin weights, and no third-party or straight-line fallback.
ProjectionStatus project_graph(const Graph& graph,
                               const Layout& desired,
                               const ProjectionConstraints& requirements,
                               Layout& out,
                               const ProjectionOptions& options = {},
                               ProjectionReport* report = nullptr);

} // namespace stun::graphlayout
