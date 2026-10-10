#include "stun/graphlayout/orthogonal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace stun::graphlayout {
namespace {

constexpr double kLimit = 1.0e11;
constexpr std::size_t kDirs = 5; // 0..3: E,S,W,N; 4: initial state
constexpr std::size_t kNoParent = std::numeric_limits<std::size_t>::max();

struct Rect {
    double l, t, r, b;
};
struct Term {
    Point anchor, stub;
    Side side;
};

bool number_ok(double value) { return std::isfinite(value) && std::abs(value) <= kLimit; }
bool nonnegative_ok(double value) { return number_ok(value) && value >= 0; }
bool same(const Point& a, const Point& b) { return a.x == b.x && a.y == b.y; }

RouteStatus fail(RouteError error, std::string message, std::size_t index = 0) {
    return {error, std::move(message), index};
}

bool valid_options(const RouteOptions& o) {
    return nonnegative_ok(o.clearance) && nonnegative_ok(o.bend_penalty) &&
           o.max_nodes > 0 && o.max_routes > 0 &&
           o.max_grid_vertices > 0 && o.max_expansions > 0 && o.max_queue_entries > 0 &&
           o.max_grid_vertices <= std::numeric_limits<std::size_t>::max() / kDirs;
}

bool valid_placement(const Layout& layout, std::size_t max_nodes) {
    if (layout.nodes.size() > max_nodes) return false;
    for (const auto& n : layout.nodes) {
        if (!number_ok(n.x) || !number_ok(n.y) || !number_ok(n.width) ||
            !number_ok(n.height) || n.width <= 0 || n.height <= 0 ||
            !number_ok(n.x + n.width) || !number_ok(n.y + n.height)) return false;
    }
    // Reject invalid overlapping node obstacles rather than pretending they can
    // be independently routed. Boundary contact is allowed.
    for (std::size_t i = 0; i < layout.nodes.size(); ++i) {
        const auto& a = layout.nodes[i];
        for (std::size_t j = i + 1; j < layout.nodes.size(); ++j) {
            const auto& b = layout.nodes[j];
            if (a.x < b.x + b.width && b.x < a.x + a.width &&
                a.y < b.y + b.height && b.y < a.y + a.height) return false;
        }
    }
    return true;
}

Rect rect_for(const PlacedNode& n, double inflation) {
    return {n.x - inflation, n.y - inflation,
            n.x + n.width + inflation, n.y + n.height + inflation};
}

bool interior_collision(Point a, Point b, const Rect& r) {
    // The caller checks axis alignment. Touching obstacle boundary is allowed.
    if (a.x == b.x) {
        return a.x > r.l && a.x < r.r &&
               std::max(a.y, b.y) > r.t && std::min(a.y, b.y) < r.b;
    }
    if (a.y == b.y) {
        return a.y > r.t && a.y < r.b &&
               std::max(a.x, b.x) > r.l && std::min(a.x, b.x) < r.r;
    }
    return true; // Fail-closed for non-orthogonal segments.
}

bool free_segment(Point a, Point b, const std::vector<Rect>& obstacles,
                  std::size_t exempt = kNoParent, std::size_t exempt_second = kNoParent) {
    if (a.x != b.x && a.y != b.y) return false;
    for (std::size_t i = 0; i < obstacles.size(); ++i) {
        if (i != exempt && i != exempt_second && interior_collision(a, b, obstacles[i])) return false;
    }
    return true;
}

bool point_inside(Point p, const Rect& r) {
    return p.x > r.l && p.x < r.r && p.y > r.t && p.y < r.b;
}

std::size_t outward_dir(Side side) {
    switch (side) {
    case Side::East: return 0;
    case Side::South: return 1;
    case Side::West: return 2;
    case Side::North: return 3;
    default: return kNoParent;
    }
}

Side auto_side(const Layout& l, std::size_t source, std::size_t target, bool at_source) {
    if (source == target) return at_source ? Side::East : Side::North;
    const auto& a = l.nodes[source];
    const auto& b = l.nodes[target];
    const double dx = b.x + b.width / 2 - (a.x + a.width / 2);
    const double dy = b.y + b.height / 2 - (a.y + a.height / 2);
    // A deterministic, relative preferred axis; tie goes horizontal.
    if (std::abs(dx) >= std::abs(dy))
        return dx >= 0 ? Side::East : Side::West;
    return dy >= 0 ? Side::South : Side::North;
}

Term endpoint(const Layout& l, const Port& port, std::size_t other,
              bool at_source, double clearance) {
    const auto& node = l.nodes[port.node];
    Side side = port.side == Side::Auto ? auto_side(l, port.node, other, at_source) : port.side;
    Point anchor;
    switch (side) {
    case Side::North: anchor = {node.x + node.width * port.offset, node.y}; break;
    case Side::East: anchor = {node.x + node.width, node.y + node.height * port.offset}; break;
    case Side::South: anchor = {node.x + node.width * port.offset, node.y + node.height}; break;
    case Side::West: anchor = {node.x, node.y + node.height * port.offset}; break;
    default: return {{}, {}, Side::Auto};
    }
    Point stub = anchor;
    // The A* grid uses the inflated obstacle boundary. Always provide a short
    // external stub if clearance is zero, to ensure a nonzero port direction.
    const double lead = std::max(clearance, 1.0);
    switch (side) {
    case Side::North: stub.y -= lead; break;
    case Side::East: stub.x += lead; break;
    case Side::South: stub.y += lead; break;
    case Side::West: stub.x -= lead; break;
    default: break;
    }
    return {anchor, stub, side};
}

void sorted_unique(std::vector<double>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

struct Step {
    double f = 0, g = 0;
    std::size_t state = 0;
};
struct StepLess {
    bool operator()(const Step& a, const Step& b) const {
        if (a.f != b.f) return a.f > b.f;
        if (a.g != b.g) return a.g > b.g;
        return a.state > b.state;
    }
};

void simplify(std::vector<Point>& p) {
    std::vector<Point> compact;
    compact.reserve(p.size());
    for (Point q : p) {
        if (!compact.empty() && same(compact.back(), q)) continue;
        while (compact.size() >= 2) {
            const Point a = compact[compact.size() - 2], b = compact.back();
            if ((a.x == b.x && b.x == q.x) || (a.y == b.y && b.y == q.y))
                compact.pop_back();
            else break;
        }
        compact.push_back(q);
    }
    p.swap(compact);
}

RouteStatus route_one(const Layout& l, const std::vector<Rect>& inflated,
                      const RouteRequest& req, std::size_t route_index,
                      const RouteOptions& opt, Route& out) {
    const auto src = endpoint(l, req.source, req.target.node, true, opt.clearance);
    const auto dst = endpoint(l, req.target, req.source.node, false, opt.clearance);
    for (Point p : {src.anchor, src.stub, dst.anchor, dst.stub})
        if (!number_ok(p.x) || !number_ok(p.y))
            return fail(RouteError::InvalidInput, "port or stub coordinate overflow", route_index);
    if (!free_segment(src.anchor, src.stub, inflated, req.source.node) ||
        !free_segment(dst.stub, dst.anchor, inflated, req.target.node))
        return fail(RouteError::NoPath, "terminal stub intersects another obstacle", route_index);

    std::vector<double> xs, ys;
    xs.reserve(inflated.size() * 2 + 4);
    ys.reserve(inflated.size() * 2 + 4);
    for (const Rect& r : inflated) {
        xs.push_back(r.l); xs.push_back(r.r);
        ys.push_back(r.t); ys.push_back(r.b);
    }
    xs.push_back(src.stub.x); xs.push_back(dst.stub.x);
    ys.push_back(src.stub.y); ys.push_back(dst.stub.y);
    sorted_unique(xs); sorted_unique(ys);
    if (xs.empty() || ys.empty() || xs.size() > opt.max_grid_vertices / ys.size())
        return fail(RouteError::CapacityExceeded, "orthogonal visibility grid exceeds configured capacity", route_index);
    const std::size_t cols = xs.size(), rows = ys.size();
    const std::size_t vertices = cols * rows;

    auto exact_index = [](const std::vector<double>& sorted, double value) {
        auto it = std::lower_bound(sorted.begin(), sorted.end(), value);
        return static_cast<std::size_t>(it - sorted.begin());
    };
    const auto vertex = [cols](std::size_t x, std::size_t y) { return y * cols + x; };
    const std::size_t start = vertex(exact_index(xs, src.stub.x), exact_index(ys, src.stub.y));
    const std::size_t goal = vertex(exact_index(xs, dst.stub.x), exact_index(ys, dst.stub.y));
    auto point_of = [&](std::size_t v) { return Point{xs[v % cols], ys[v / cols]}; };
    auto state_of = [](std::size_t v, std::size_t dir) { return v * kDirs + dir; };
    auto valid = [&](Point p) {
        for (const auto& rect : inflated) if (point_inside(p, rect)) return false;
        return true;
    };
    if (!valid(src.stub) || !valid(dst.stub))
        return fail(RouteError::NoPath, "terminal stub lies inside an inflated obstacle", route_index);

    const std::size_t states = vertices * kDirs;
    const double infinity = std::numeric_limits<double>::infinity();
    std::vector<double> cost(states, infinity);
    std::vector<std::size_t> predecessor(states, kNoParent);
    std::vector<unsigned char> closed(states, 0);
    std::vector<signed char> valid_cache(vertices, -1);
    auto is_valid = [&](std::size_t v) {
        auto& c = valid_cache[v];
        if (c == -1) c = valid(point_of(v)) ? 1 : 0;
        return c != 0;
    };
    auto heuristic = [&](std::size_t v) {
        const Point p = point_of(v);
        return std::abs(p.x - dst.stub.x) + std::abs(p.y - dst.stub.y);
    };
    std::priority_queue<Step, std::vector<Step>, StepLess> pending;
    const std::size_t initial = state_of(start, outward_dir(src.side));
    cost[initial] = 0;
    pending.push({heuristic(start), 0, initial});
    std::size_t iterations = 0;
    std::size_t winner = kNoParent;
    while (!pending.empty()) {
        if (++iterations > opt.max_expansions)
            return fail(RouteError::CapacityExceeded, "orthogonal A* expansion capacity exceeded", route_index);
        const Step step = pending.top(); pending.pop();
        const std::size_t state = step.state, v = state / kDirs;
        if (closed[state] || step.g != cost[state]) continue;
        closed[state] = 1;
        if (v == goal) { winner = state; break; }

        const std::size_t x = v % cols, y = v / cols;
        // Clockwise direction order makes tie resolution reproducible.
        const std::array<bool, 4> available{{x + 1 < cols, y + 1 < rows, x > 0, y > 0}};
        const std::array<std::size_t, 4> next{{x + 1 < cols ? v + 1 : v,
                                                y + 1 < rows ? v + cols : v,
                                                x > 0 ? v - 1 : v,
                                                y > 0 ? v - cols : v}};
        for (std::size_t dir = 0; dir < 4; ++dir) {
            if (!available[dir]) continue;
            const auto nv = next[dir];
            if (!is_valid(nv) || !free_segment(point_of(v), point_of(nv), inflated)) continue;
            const Point p = point_of(v), q = point_of(nv);
            const double length = std::abs(p.x - q.x) + std::abs(p.y - q.y);
            if (!(length > 0)) continue;
            const double ng = cost[state] + length + (state % kDirs != dir ? opt.bend_penalty : 0.0);
            if (!number_ok(ng))
                return fail(RouteError::CapacityExceeded, "route cost exceeds supported numeric range", route_index);
            const std::size_t ns = state_of(nv, dir);
            if (ng < cost[ns]) {
                cost[ns] = ng;
                predecessor[ns] = state;
                pending.push({ng + heuristic(nv), ng, ns});
                if (pending.size() > opt.max_queue_entries)
                    return fail(RouteError::CapacityExceeded, "orthogonal A* queue capacity exceeded", route_index);
            }
        }
    }
    if (winner == kNoParent)
        return fail(RouteError::NoPath, "no unobstructed orthogonal path exists for selected ports", route_index);

    std::vector<Point> reversed;
    for (std::size_t state = winner; state != kNoParent; state = predecessor[state]) {
        reversed.push_back(point_of(state / kDirs));
        if (reversed.size() > vertices * kDirs)
            return fail(RouteError::InternalInvariant, "route reconstruction cycle", route_index);
    }
    std::reverse(reversed.begin(), reversed.end());
    out.points.reserve(reversed.size() + 2);
    out.points.push_back(src.anchor);
    out.points.insert(out.points.end(), reversed.begin(), reversed.end());
    out.points.push_back(dst.anchor);
    simplify(out.points);
    if (out.points.size() < 2 || (req.source.node == req.target.node && out.points.size() < 4))
        return fail(RouteError::InternalInvariant, "degenerate route", route_index);
    return {};
}

} // namespace

RouteStatus validate_orthogonal_routes(const Layout& layout,
                                       const std::vector<RouteRequest>& requests,
                                       const Routes& routes,
                                       const RouteOptions& options) {
    if (!valid_options(options)) return fail(RouteError::InvalidOptions, "invalid routing options");
    if (!valid_placement(layout, options.max_nodes))
        return fail(RouteError::InvalidInput, "invalid, overlapping or over-capacity layout rectangles");
    if (requests.size() > options.max_routes) return fail(RouteError::CapacityExceeded, "route count exceeds configured capacity");
    if (requests.size() != routes.edges.size()) return fail(RouteError::InvalidInput, "route count mismatch");
    std::vector<Rect> inflated;
    inflated.reserve(layout.nodes.size());
    for (const auto& n : layout.nodes) {
        const auto r = rect_for(n, options.clearance);
        if (!number_ok(r.l) || !number_ok(r.t) || !number_ok(r.r) || !number_ok(r.b))
            return fail(RouteError::InvalidInput, "inflated obstacle overflow");
        inflated.push_back(r);
    }
    for (std::size_t i = 0; i < requests.size(); ++i) {
        const auto& req = requests[i];
        if (req.source.node >= layout.nodes.size() || req.target.node >= layout.nodes.size() ||
            !std::isfinite(req.source.offset) || !std::isfinite(req.target.offset) ||
            req.source.offset < 0 || req.source.offset > 1 ||
            req.target.offset < 0 || req.target.offset > 1 ||
            (req.source.side != Side::Auto && outward_dir(req.source.side) == kNoParent) ||
            (req.target.side != Side::Auto && outward_dir(req.target.side) == kNoParent))
            return fail(RouteError::InvalidInput, "invalid routing endpoint", i);
        const Term a = endpoint(layout, req.source, req.target.node, true, options.clearance);
        const Term b = endpoint(layout, req.target, req.source.node, false, options.clearance);
        const auto& pts = routes.edges[i].points;
        if (pts.size() < 2 || !same(pts.front(), a.anchor) || !same(pts.back(), b.anchor))
            return fail(RouteError::InternalInvariant, "invalid route endpoints", i);
        for (std::size_t j = 1; j < pts.size(); ++j) {
            if (!number_ok(pts[j-1].x) || !number_ok(pts[j-1].y) ||
                !number_ok(pts[j].x) || !number_ok(pts[j].y) ||
                same(pts[j-1], pts[j]) ||
                (pts[j-1].x != pts[j].x && pts[j-1].y != pts[j].y))
                return fail(RouteError::InternalInvariant, "invalid orthogonal route segment", i);
            // First/last segments may traverse their own clearance envelope,
            // but may never enter any *other* inflated obstacle interior.
            const std::size_t except = j == 1 ? req.source.node :
                                       (j + 1 == pts.size() ? req.target.node : kNoParent);
            if (!free_segment(pts[j-1], pts[j], inflated, except,
                              pts.size() == 2 ? req.target.node : kNoParent))
                return fail(RouteError::InternalInvariant, "route enters an obstacle clearance envelope", i);
        }
        // Also check non-penetration of both *actual* terminal rectangles;
        // this is stronger than allowing unrestricted terminal stubs.
        const Rect s = rect_for(layout.nodes[req.source.node], 0.0);
        const Rect t = rect_for(layout.nodes[req.target.node], 0.0);
        for (std::size_t j = 1; j < pts.size(); ++j)
            if (interior_collision(pts[j - 1], pts[j], s) ||
                interior_collision(pts[j - 1], pts[j], t))
                return fail(RouteError::InternalInvariant, "route enters terminal shape", i);
        if ((a.side == Side::East && pts[1].x <= a.anchor.x) ||
            (a.side == Side::West && pts[1].x >= a.anchor.x) ||
            (a.side == Side::South && pts[1].y <= a.anchor.y) ||
            (a.side == Side::North && pts[1].y >= a.anchor.y) ||
            (b.side == Side::East && pts[pts.size()-2].x <= b.anchor.x) ||
            (b.side == Side::West && pts[pts.size()-2].x >= b.anchor.x) ||
            (b.side == Side::South && pts[pts.size()-2].y <= b.anchor.y) ||
            (b.side == Side::North && pts[pts.size()-2].y >= b.anchor.y))
            return fail(RouteError::InternalInvariant, "route violates an endpoint direction", i);
    }
    return {};
}

RouteStatus route_orthogonal(const Layout& layout,
                             const std::vector<RouteRequest>& requests,
                             Routes& out,
                             const RouteOptions& options) {
    out = {};
    if (!valid_options(options)) return fail(RouteError::InvalidOptions, "invalid routing options");
    if (layout.nodes.size() > options.max_nodes || requests.size() > options.max_routes)
        return fail(RouteError::CapacityExceeded, "routing input exceeds configured capacity");
    if (!valid_placement(layout, options.max_nodes))
        return fail(RouteError::InvalidInput, "invalid or overlapping obstacle geometry");
    std::vector<Rect> inflated;
    inflated.reserve(layout.nodes.size());
    for (const auto& n : layout.nodes) {
        const Rect r = rect_for(n, options.clearance);
        if (!number_ok(r.l) || !number_ok(r.t) || !number_ok(r.r) || !number_ok(r.b))
            return fail(RouteError::InvalidInput, "inflated obstacle overflow");
        inflated.push_back(r);
    }
    Routes candidate;
    candidate.edges.reserve(requests.size());
    for (std::size_t i = 0; i < requests.size(); ++i) {
        const auto& req = requests[i];
        if (req.source.node >= layout.nodes.size() || req.target.node >= layout.nodes.size() ||
            !std::isfinite(req.source.offset) || !std::isfinite(req.target.offset) ||
            req.source.offset < 0 || req.source.offset > 1 ||
            req.target.offset < 0 || req.target.offset > 1 ||
            (req.source.side != Side::Auto && outward_dir(req.source.side) == kNoParent) ||
            (req.target.side != Side::Auto && outward_dir(req.target.side) == kNoParent))
            return fail(RouteError::InvalidInput, "invalid routing endpoint", i);
        candidate.edges.emplace_back();
        auto status = route_one(layout, inflated, req, i, options, candidate.edges.back());
        if (!status) return status;
    }
    const auto checked = validate_orthogonal_routes(layout, requests, candidate, options);
    if (!checked) return checked;
    out = std::move(candidate);
    return {};
}

} // namespace stun::graphlayout
