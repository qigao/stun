#include "stun/graphlayout/port_bindings.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace stun::graphlayout;

namespace {
void require(bool value, const char* why) {
    if (!value) throw std::runtime_error(why);
}
void close(double a, double b, const char* why) {
    if (std::abs(a - b) > 1e-8) throw std::runtime_error(why);
}
} // namespace

int main() {
    try {
        Layout l;
        l.nodes = {{20.0, 30.0, 80.0, 40.0}, {200.0, 90.0, 30.0, 60.0}};
        std::vector<MeasuredNamedPort> input{
            {1, "entry", Side::West, {0.0, 45.0}},
            {0, "south", Side::South, {16.0, 40.0}},
            {0, "exit", Side::East, {80.0, 10.0}},
            {0, "corner", Side::North, {80.0, 0.0}}
        };
        NamedPortBindings bound;
        require(static_cast<bool>(bind_named_ports(l, input, bound)), "bind precise measured ports");
        require(bound.ports.size() == 4, "port count");
        const auto* exit = find_named_port(bound, 0, "exit");
        const auto* entry = find_named_port(bound, 1, "entry");
        const auto* corner = find_named_port(bound, 0, "corner");
        require(exit && entry && corner, "all measured names found");
        require(exit->port.side == Side::East, "correct measured side");
        close(exit->port.offset, 0.25, "east offset from actual height");
        close(exit->world_anchor.x, 100.0, "exit world x");
        close(exit->world_anchor.y, 40.0, "exit world y");
        close(entry->port.offset, 0.75, "entry offset");
        close(entry->world_anchor.x, 200.0, "entry world x");
        close(entry->world_anchor.y, 135.0, "entry world y");
        close(corner->port.offset, 1.0, "corner selects north endpoint");
        require(!find_named_port(bound, 0, "missing"), "missing name is not guessed");
        require(!find_named_port(bound, 1, "exit"), "names are node scoped");
        auto other = input;
        std::reverse(other.begin(), other.end());
        NamedPortBindings reordered;
        require(static_cast<bool>(bind_named_ports(l, other, reordered)), "reordered measurements work");
        for (std::size_t i = 0; i < bound.ports.size(); ++i) {
            require(bound.ports[i].node == reordered.ports[i].node &&
                    bound.ports[i].name == reordered.ports[i].name,
                    "canonical ordering independent of input order");
        }

        auto bad = input;
        bad.push_back(input.front());
        require(bind_named_ports(l, bad, bound).error == NamedPortError::DuplicateName,
                "same name on a node is rejected");
        require(bound.ports.empty(), "failure clears all binding output");
        bad = input;
        bad[0].local_anchor.x = 4.0;
        require(bind_named_ports(l, bad, bound).error == NamedPortError::InvalidMeasurement,
                "interior attachment point is not moved to border");
        bad = input;
        bad[0].local_anchor.y = 90.0;
        require(bind_named_ports(l, bad, bound).error == NamedPortError::InvalidMeasurement,
                "out of bounds local measurement rejected");
        bad = input;
        bad[0].side = Side::Auto;
        require(bind_named_ports(l, bad, bound).error == NamedPortError::InvalidMeasurement,
                "Auto cannot substitute for a measured direction");
        bad = input;
        bad[0].local_anchor.y = std::numeric_limits<double>::infinity();
        require(bind_named_ports(l, bad, bound).error == NamedPortError::InvalidMeasurement,
                "nonfinite measurement rejected");
        bad = input;
        bad[0].name.clear();
        require(bind_named_ports(l, bad, bound).error == NamedPortError::InvalidMeasurement,
                "empty name rejected");
        bad = input;
        bad[0].node = 2;
        require(bind_named_ports(l, bad, bound).error == NamedPortError::InvalidMeasurement,
                "invalid node index rejected");
        bad = input;
        NamedPortOptions limited;
        limited.max_ports = 1;
        require(bind_named_ports(l, bad, bound, limited).error == NamedPortError::CapacityExceeded,
                "bounded port count");
        limited = {};
        limited.boundary_tolerance = 1.0;
        require(bind_named_ports(l, bad, bound, limited).error == NamedPortError::InvalidOptions,
                "excessively loose coordinate tolerance forbidden");
        l.nodes[0].width = std::numeric_limits<double>::quiet_NaN();
        require(bind_named_ports(l, input, bound).error == NamedPortError::InvalidLayout,
                "bad graph rectangles rejected");
        std::cout << "port bindings: all checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "port bindings: " << e.what() << '\n';
        return 1;
    }
}
