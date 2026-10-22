#include "stun/graphlayout/port_bindings.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace stun::graphlayout {
namespace {
constexpr double kGeometryLimit = 1e11; // Follows native route geometry limits.

bool coordinate(double x) { return std::isfinite(x) && std::abs(x) <= kGeometryLimit; }

NamedPortStatus fail(NamedPortError error, const char* message, std::size_t index = 0) {
    return {error, message, index};
}

bool is_real_side(Side side) {
    return side == Side::North || side == Side::East ||
           side == Side::South || side == Side::West;
}

bool approx(double a, double b, double tolerance) {
    return std::abs(a - b) <= tolerance;
}

} // namespace

NamedPortStatus bind_named_ports(const Layout& layout,
                                 const std::vector<MeasuredNamedPort>& measurements,
                                 NamedPortBindings& out,
                                 const NamedPortOptions& options) {
    out = {};
    if (!options.max_nodes || !options.max_ports || !options.max_name_bytes ||
        !std::isfinite(options.boundary_tolerance) ||
        options.boundary_tolerance < 0.0 || options.boundary_tolerance > 1e-4)
        return fail(NamedPortError::InvalidOptions, "invalid named port limits/tolerance");
    if (layout.nodes.size() > options.max_nodes || measurements.size() > options.max_ports)
        return fail(NamedPortError::CapacityExceeded, "named port node/measurement budget exceeded");
    for (const auto& n : layout.nodes) {
        if (!coordinate(n.x) || !coordinate(n.y) ||
            !coordinate(n.width) || !coordinate(n.height) ||
            n.width <= 0.0 || n.height <= 0.0 ||
            !coordinate(n.x + n.width) || !coordinate(n.y + n.height))
            return fail(NamedPortError::InvalidLayout, "invalid placed node rectangle");
    }

    NamedPortBindings candidate;
    candidate.ports.reserve(measurements.size());
    const double tol = options.boundary_tolerance;
    for (std::size_t i = 0; i < measurements.size(); ++i) {
        const auto& m = measurements[i];
        if (m.node >= layout.nodes.size() || m.name.empty() ||
            m.name.size() > options.max_name_bytes || !is_real_side(m.side) ||
            !coordinate(m.local_anchor.x) || !coordinate(m.local_anchor.y))
            return fail(NamedPortError::InvalidMeasurement, "invalid named port identity, direction or coordinates", i);
        const auto& n = layout.nodes[m.node];
        double fixed = 0.0;
        double measured_fixed = 0.0;
        double along = 0.0;
        double extent = 0.0;
        switch (m.side) {
        case Side::North:
            fixed = 0.0; measured_fixed = m.local_anchor.y;
            along = m.local_anchor.x; extent = n.width;
            break;
        case Side::East:
            fixed = n.width; measured_fixed = m.local_anchor.x;
            along = m.local_anchor.y; extent = n.height;
            break;
        case Side::South:
            fixed = n.height; measured_fixed = m.local_anchor.y;
            along = m.local_anchor.x; extent = n.width;
            break;
        case Side::West:
            fixed = 0.0; measured_fixed = m.local_anchor.x;
            along = m.local_anchor.y; extent = n.height;
            break;
        default:
            return fail(NamedPortError::InvalidMeasurement, "named port needs explicit boundary side", i);
        }
        if (!approx(fixed, measured_fixed, tol) || along < -tol || along > extent + tol)
            return fail(NamedPortError::InvalidMeasurement,
                        "named port measurement does not lie on the specified rectangle side", i);
        const double bounded = std::max(0.0, std::min(extent, along));
        const double fraction = bounded / extent;
        if (!std::isfinite(fraction) || fraction < 0.0 || fraction > 1.0)
            return fail(NamedPortError::InvalidMeasurement, "nonfinite named port offset", i);
        Point world;
        switch (m.side) {
        case Side::North: world = {n.x + n.width * fraction, n.y}; break;
        case Side::East: world = {n.x + n.width, n.y + n.height * fraction}; break;
        case Side::South: world = {n.x + n.width * fraction, n.y + n.height}; break;
        case Side::West: world = {n.x, n.y + n.height * fraction}; break;
        default: break;
        }
        if (!coordinate(world.x) || !coordinate(world.y) ||
            !approx(world.x, n.x + m.local_anchor.x, tol + 1e-12) ||
            !approx(world.y, n.y + m.local_anchor.y, tol + 1e-12))
            return fail(NamedPortError::InvalidMeasurement,
                        "measurement cannot be represented by a precise boundary port", i);
        candidate.ports.push_back({m.node, m.name, {m.node, m.side, fraction}, world});
    }
    std::sort(candidate.ports.begin(), candidate.ports.end(),
              [](const BoundNamedPort& a, const BoundNamedPort& b) {
                  if (a.node != b.node) return a.node < b.node;
                  return a.name < b.name;
              });
    for (std::size_t i = 1; i < candidate.ports.size(); ++i)
        if (candidate.ports[i - 1].node == candidate.ports[i].node &&
            candidate.ports[i - 1].name == candidate.ports[i].name)
            return fail(NamedPortError::DuplicateName, "duplicate named port for a node");
    out = std::move(candidate);
    return {};
}

const BoundNamedPort* find_named_port(const NamedPortBindings& bindings,
                                      std::size_t node,
                                      std::string_view name) {
    const auto it = std::lower_bound(bindings.ports.begin(), bindings.ports.end(),
        std::pair<std::size_t, std::string_view>{node, name},
        [](const BoundNamedPort& a, const std::pair<std::size_t, std::string_view>& key) {
            return a.node < key.first || (a.node == key.first && a.name < key.second);
        });
    if (it == bindings.ports.end() || it->node != node || it->name != name)
        return nullptr;
    return &*it;
}

} // namespace stun::graphlayout
