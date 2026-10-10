#pragma once

#include "stun/graphlayout/orthogonal.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace stun::graphlayout {

// Measurements come from the Chart's actual node geometry, in local node
// coordinates. The engine does not parse DOT/HTML/record labels or invent
// attachment positions. Only an explicitly selected outward rectangle side
// can be bound; arbitrary interior points are not exterior routing ports.
struct MeasuredNamedPort {
    std::size_t node = 0;
    std::string name;
    Side side = Side::Auto;
    Point local_anchor;
};

struct BoundNamedPort {
    std::size_t node = 0;
    std::string name;
    Port port;
    Point world_anchor; // Derived from the *actual* placed node rectangle.
};

struct NamedPortBindings {
    // Sorted by (node index, name), independent of measurement insertion order.
    std::vector<BoundNamedPort> ports;
};

struct NamedPortOptions {
    std::size_t max_nodes = 256;
    std::size_t max_ports = 4096;
    std::size_t max_name_bytes = 256;
    // Absolute tolerance for input measurement rounding. It does not permit
    // silently moving an interior port to the perimeter.
    double boundary_tolerance = 1e-8;
};

enum class NamedPortError {
    None,
    InvalidOptions,
    CapacityExceeded,
    InvalidLayout,
    InvalidMeasurement,
    DuplicateName,
};

struct NamedPortStatus {
    NamedPortError error = NamedPortError::None;
    std::string message;
    std::size_t measurement_index = 0;
    explicit operator bool() const { return error == NamedPortError::None; }
};

// Transactional conversion of measured, node-local attachment points into
// exact boundary Port(side,offset) contracts shared by Orthogonal/Polyline.
// Fail on missing node, duplicate name, nonfinite/outside/interior geometry,
// unsupported side or capacity. No corner/center or UI fallback.
NamedPortStatus bind_named_ports(const Layout& layout,
                                 const std::vector<MeasuredNamedPort>& measurements,
                                 NamedPortBindings& out,
                                 const NamedPortOptions& options = {});

// A lookup has no fallbacks: nullptr means the caller has not supplied a
// matching measured port. Returned pointers are valid while bindings live.
const BoundNamedPort* find_named_port(const NamedPortBindings& bindings,
                                      std::size_t node,
                                      std::string_view name);

} // namespace stun::graphlayout
