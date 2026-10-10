#include "stun/graphlayout/polyline.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace stun::graphlayout {
namespace {

constexpr double kMaxCoordinate = 1.0e11;
constexpr std::size_t kMissing = std::numeric_limits<std::size_t>::max();

struct Rectangle { double left, top, right, bottom; };
struct Terminal { Point anchor, stub; Side side; };
struct Visit { double estimate, cost; std::size_t vertex; };
struct GreaterVisit {
    bool operator()(const Visit& a, const Visit& b) const {
        if (a.estimate != b.estimate) return a.estimate > b.estimate;
        if (a.cost != b.cost) return a.cost > b.cost;
        return a.vertex > b.vertex;
    }
};

bool finite_coordinate(double v) {
    return std::isfinite(v) && std::abs(v) <= kMaxCoordinate;
}
bool same(Point a, Point b) { return a.x == b.x && a.y == b.y; }
bool allowed_side(Side side) {
    return side == Side::Auto || side == Side::North || side == Side::East ||
           side == Side::South || side == Side::West;
}
RouteStatus error(RouteError type, std::string message, std::size_t route = 0) {
    return {type, std::move(message), route};
}
bool options_ok(const PolylineOptions& o) {
    return finite_coordinate(o.clearance) && o.clearance >= 0 &&
           o.max_nodes > 0 && o.max_routes > 0 &&
           o.max_visibility_vertices >= 2 && o.max_expansions > 0 &&
           o.max_segment_candidates > 0 && o.max_obstacle_tests > 0 &&
           o.max_queue_entries > 0 && o.max_total_points > 0;
}
bool placement_ok(const Layout& l) {
    for (const auto& n : l.nodes) {
        if (!finite_coordinate(n.x) || !finite_coordinate(n.y) ||
            !finite_coordinate(n.width) || !finite_coordinate(n.height) ||
            n.width <= 0 || n.height <= 0 ||
            !finite_coordinate(n.x + n.width) ||
            !finite_coordinate(n.y + n.height)) return false;
    }
    for (std::size_t i = 0; i < l.nodes.size(); ++i)
        for (std::size_t j = i + 1; j < l.nodes.size(); ++j) {
            const auto& a = l.nodes[i];
            const auto& b = l.nodes[j];
            if (a.x < b.x + b.width && b.x < a.x + a.width &&
                a.y < b.y + b.height && b.y < a.y + a.height) return false;
        }
    return true;
}
Rectangle rectangle(const PlacedNode& n, double padding) {
    return {n.x - padding, n.y - padding,
            n.x + n.width + padding, n.y + n.height + padding};
}

// Open-interior segment/axis-aligned-rectangle intersection, with exact
// boundary tangencies admitted. Open intervals avoid falsely treating a route
// along the obstacle boundary as penetrating the obstacle.
bool penetrates(Point a, Point b, const Rectangle& r) {
    if (same(a,b)) return false;
    long double low = 0.0L, high = 1.0L;
    const auto clip = [&](long double start, long double delta,
                          long double lo, long double hi) {
        if (delta == 0.0L) return start > lo && start < hi;
        long double t0 = (lo - start) / delta;
        long double t1 = (hi - start) / delta;
        if (t0 > t1) std::swap(t0, t1);
        low = std::max(low, t0);
        high = std::min(high, t1);
        return low < high;
    };
    return clip(a.x, static_cast<long double>(b.x) - a.x, r.left, r.right) &&
           clip(a.y, static_cast<long double>(b.y) - a.y, r.top, r.bottom) &&
           low < high;
}
bool interior_point(Point p, const Rectangle& r) {
    return p.x > r.left && p.x < r.right && p.y > r.top && p.y < r.bottom;
}

Side choose_side(const Layout& l, std::size_t self, std::size_t other,
                 bool source) {
    if (self == other) return source ? Side::East : Side::North;
    const auto& n = l.nodes[self];
    const auto& m = l.nodes[other];
    const double dx = (m.x + m.width * 0.5) - (n.x + n.width * 0.5);
    const double dy = (m.y + m.height * 0.5) - (n.y + n.height * 0.5);
    if (std::abs(dx) >= std::abs(dy)) return dx >= 0 ? Side::East : Side::West;
    return dy >= 0 ? Side::South : Side::North;
}
Terminal endpoint(const Layout& l, const Port& p, std::size_t opposite,
                  bool at_source, double clearance) {
    const PlacedNode& n = l.nodes[p.node];
    const Side side = p.side == Side::Auto ?
                      choose_side(l, p.node, opposite, at_source) : p.side;
    Point anchor;
    switch (side) {
    case Side::North: anchor = {n.x + n.width * p.offset, n.y}; break;
    case Side::East: anchor = {n.x + n.width, n.y + n.height * p.offset}; break;
    case Side::South: anchor = {n.x + n.width * p.offset, n.y + n.height}; break;
    case Side::West: anchor = {n.x, n.y + n.height * p.offset}; break;
    default: return {{}, {}, Side::Auto};
    }
    Point stub = anchor;
    const double lead = std::max(1.0, clearance);
    switch (side) {
    case Side::North: stub.y -= lead; break;
    case Side::East: stub.x += lead; break;
    case Side::South: stub.y += lead; break;
    case Side::West: stub.x -= lead; break;
    default: break;
    }
    return {anchor, stub, side};
}
bool request_ok(const Layout& l, const RouteRequest& r) {
    return r.source.node < l.nodes.size() && r.target.node < l.nodes.size() &&
           std::isfinite(r.source.offset) && std::isfinite(r.target.offset) &&
           r.source.offset >= 0 && r.source.offset <= 1 &&
           r.target.offset >= 0 && r.target.offset <= 1 &&
           allowed_side(r.source.side) && allowed_side(r.target.side);
}

bool clear_segment(Point a, Point b, const std::vector<Rectangle>& rectangles,
                   std::size_t exclude = kMissing) {
    for (std::size_t i = 0; i < rectangles.size(); ++i)
        if (i != exclude && penetrates(a, b, rectangles[i])) return false;
    return true;
}
bool clear_point(Point p, const std::vector<Rectangle>& rectangles) {
    for (const auto& rect : rectangles) if (interior_point(p, rect)) return false;
    return true;
}
bool finite_point(Point p) {
    return finite_coordinate(p.x) && finite_coordinate(p.y);
}

RouteStatus one_route(const Layout& layout, const std::vector<Rectangle>& obstacles,
                      const RouteRequest& request, const PolylineOptions& opt,
                      std::size_t route_index, Route& out) {
    const Terminal a = endpoint(layout, request.source, request.target.node,
                                true, opt.clearance);
    const Terminal b = endpoint(layout, request.target, request.source.node,
                                false, opt.clearance);
    if (!finite_point(a.anchor) || !finite_point(a.stub) ||
        !finite_point(b.anchor) || !finite_point(b.stub))
        return error(RouteError::InvalidInput, "polyline endpoint overflows coordinate range", route_index);
    if (!clear_segment(a.anchor, a.stub, obstacles, request.source.node) ||
        !clear_segment(b.stub, b.anchor, obstacles, request.target.node) ||
        !clear_point(a.stub, obstacles) || !clear_point(b.stub, obstacles))
        return error(RouteError::NoPath, "polyline terminal clearance obstructed", route_index);
    if (request.source.node == request.target.node && same(a.stub, b.stub))
        return error(RouteError::NoPath, "coincident self-loop ports have no nondegenerate route", route_index);

    // Stable geometry-order vertices give identical decisions when input nodes
    // are permuted but their IDs, rectangles and endpoint references stay fixed.
    std::vector<Point> vertices;
    if (obstacles.size() > (opt.max_visibility_vertices - 2) / 4)
        return error(RouteError::CapacityExceeded, "polyline visibility vertex budget exceeded", route_index);
    vertices.reserve(obstacles.size() * 4 + 2);
    vertices.push_back(a.stub);
    vertices.push_back(b.stub);
    for (const auto& rect : obstacles) {
        vertices.push_back({rect.left, rect.top});
        vertices.push_back({rect.left, rect.bottom});
        vertices.push_back({rect.right, rect.top});
        vertices.push_back({rect.right, rect.bottom});
    }
    std::sort(vertices.begin(), vertices.end(), [](Point x, Point y) {
        return x.x < y.x || (x.x == y.x && x.y < y.y);
    });
    vertices.erase(std::unique(vertices.begin(), vertices.end(), same), vertices.end());
    if (vertices.size() > opt.max_visibility_vertices)
        return error(RouteError::CapacityExceeded, "polyline visibility vertex budget exceeded", route_index);

    const auto find = [&](Point p) -> std::size_t {
        auto it = std::lower_bound(vertices.begin(), vertices.end(), p,
            [](Point x, Point y) {
                return x.x < y.x || (x.x == y.x && x.y < y.y);
            });
        return static_cast<std::size_t>(it - vertices.begin());
    };
    const std::size_t source = find(a.stub), target = find(b.stub);
    const std::size_t n = vertices.size();
    std::vector<unsigned char> allowed(n, 0), visited(n, 0);
    for (std::size_t i = 0; i < n; ++i) allowed[i] = clear_point(vertices[i], obstacles) ? 1 : 0;
    const double infinity = std::numeric_limits<double>::infinity();
    std::vector<double> distance(n, infinity);
    std::vector<std::size_t> parent(n, kMissing);
    std::priority_queue<Visit, std::vector<Visit>, GreaterVisit> open;
    const auto heuristic = [&](std::size_t i) { return std::hypot(
        vertices[i].x - vertices[target].x,
        vertices[i].y - vertices[target].y); };
    distance[source] = 0;
    open.push({heuristic(source), 0, source});
    std::size_t expansions = 0, candidates = 0, obstacle_tests = 0;
    while (!open.empty()) {
        auto current = open.top(); open.pop();
        if (visited[current.vertex] || current.cost != distance[current.vertex]) continue;
        if (++expansions > opt.max_expansions)
            return error(RouteError::CapacityExceeded, "polyline A* expansion budget exceeded", route_index);
        visited[current.vertex] = 1;
        if (current.vertex == target) break;
        const Point p = vertices[current.vertex];
        for (std::size_t i = 0; i < n; ++i) {
            if (i == current.vertex || !allowed[i] || visited[i]) continue;
            if (++candidates > opt.max_segment_candidates)
                return error(RouteError::CapacityExceeded, "polyline visibility candidate budget exceeded", route_index);
            bool clear = true;
            for (const Rectangle& rect : obstacles) {
                if (++obstacle_tests > opt.max_obstacle_tests)
                    return error(RouteError::CapacityExceeded, "polyline obstacle test budget exceeded", route_index);
                if (penetrates(p, vertices[i], rect)) { clear = false; break; }
            }
            if (!clear) continue;
            const double segment = std::hypot(p.x - vertices[i].x, p.y - vertices[i].y);
            const double next = distance[current.vertex] + segment;
            if (!finite_coordinate(next))
                return error(RouteError::CapacityExceeded, "polyline cumulative distance overflow", route_index);
            if (next < distance[i]) {
                distance[i] = next;
                parent[i] = current.vertex;
                open.push({next + heuristic(i), next, i});
                if (open.size() > opt.max_queue_entries)
                    return error(RouteError::CapacityExceeded, "polyline A* queue budget exceeded", route_index);
            }
        }
    }
    if (!visited[target])
        return error(RouteError::NoPath, "no polyline path in visibility graph", route_index);
    std::vector<Point> interior;
    for (std::size_t p = target; p != kMissing; p = parent[p]) {
        if (interior.size() == n)
            return error(RouteError::InternalInvariant, "polyline predecessor cycle", route_index);
        interior.push_back(vertices[p]);
    }
    std::reverse(interior.begin(), interior.end());
    out.points.reserve(interior.size() + 2);
    out.points.push_back(a.anchor);
    out.points.insert(out.points.end(), interior.begin(), interior.end());
    out.points.push_back(b.anchor);
    // Stubs are preserved even if a route is collinear. This lets the validator
    // verify exact exit/entry direction without reverse-engineering tangents.
    return {};
}

RouteStatus validate_layout_and_options(const Layout& l,
                                        std::size_t count, const PolylineOptions& opt) {
    if (!options_ok(opt)) return error(RouteError::InvalidOptions, "invalid polyline options");
    if (l.nodes.size() > opt.max_nodes || count > opt.max_routes)
        return error(RouteError::CapacityExceeded, "polyline node/route capacity exceeded");
    if (!placement_ok(l))
        return error(RouteError::InvalidInput, "invalid or overlapping polyline obstacles");
    return {};
}
RouteStatus inflate(const Layout& l, const PolylineOptions& opt,
                    std::vector<Rectangle>& result) {
    result.reserve(l.nodes.size());
    for (const auto& n : l.nodes) {
        const Rectangle r = rectangle(n, opt.clearance);
        if (!finite_coordinate(r.left) || !finite_coordinate(r.top) ||
            !finite_coordinate(r.right) || !finite_coordinate(r.bottom))
            return error(RouteError::InvalidInput, "polyline obstacle inflation overflow");
        result.push_back(r);
    }
    return {};
}

} // namespace

RouteStatus validate_polyline_routes(const Layout& layout,
                                      const std::vector<RouteRequest>& requests,
                                      const Routes& routes,
                                      const PolylineOptions& options) {
    auto status = validate_layout_and_options(layout, requests.size(), options);
    if (!status) return status;
    if (requests.size() != routes.edges.size())
        return error(RouteError::InvalidInput, "polyline route count mismatch");
    std::vector<Rectangle> inflated;
    status = inflate(layout, options, inflated);
    if (!status) return status;
    std::size_t total_points = 0;
    for (std::size_t i = 0; i < requests.size(); ++i) {
        const auto& request = requests[i];
        if (!request_ok(layout, request))
            return error(RouteError::InvalidInput, "invalid polyline port", i);
        const auto a = endpoint(layout, request.source, request.target.node, true, options.clearance);
        const auto b = endpoint(layout, request.target, request.source.node, false, options.clearance);
        const auto& pts = routes.edges[i].points;
        if (pts.size() > options.max_total_points - total_points)
            return error(RouteError::CapacityExceeded, "polyline batch waypoint budget exceeded", i);
        total_points += pts.size();
        if (pts.size() < 3 ||
            !same(pts.front(), a.anchor) || !same(pts.back(), b.anchor) ||
            !same(pts[1], a.stub) || !same(pts[pts.size()-2], b.stub) ||
            (request.source.node == request.target.node && pts.size() < 5))
            return error(RouteError::InternalInvariant, "polyline anchors, stubs or self-loop invalid", i);
        for (std::size_t j = 1; j < pts.size(); ++j) {
            const Point prev = pts[j-1], next = pts[j];
            if (!finite_point(prev) || !finite_point(next) || same(prev, next))
                return error(RouteError::InternalInvariant, "degenerate or nonfinite polyline segment", i);
            const std::size_t terminal_exemption = j == 1 ? request.source.node :
                (j + 1 == pts.size() ? request.target.node : kMissing);
            if (!clear_segment(prev, next, inflated, terminal_exemption))
                return error(RouteError::InternalInvariant, "polyline enters inflated obstacle interior", i);
            if (penetrates(prev, next, rectangle(layout.nodes[request.source.node], 0)) ||
                penetrates(prev, next, rectangle(layout.nodes[request.target.node], 0)))
                return error(RouteError::InternalInvariant, "polyline penetrates terminal rectangle", i);
        }
    }
    return {};
}

RouteStatus route_polyline(const Layout& layout,
                           const std::vector<RouteRequest>& requests,
                           Routes& out,
                           const PolylineOptions& options) {
    out = {};
    auto status = validate_layout_and_options(layout, requests.size(), options);
    if (!status) return status;
    std::vector<Rectangle> inflated;
    status = inflate(layout, options, inflated);
    if (!status) return status;
    Routes candidate;
    candidate.edges.reserve(requests.size());
    std::size_t point_count = 0;
    for (std::size_t i = 0; i < requests.size(); ++i) {
        if (!request_ok(layout, requests[i]))
            return error(RouteError::InvalidInput, "invalid polyline port", i);
        candidate.edges.emplace_back();
        status = one_route(layout, inflated, requests[i], options, i, candidate.edges.back());
        if (!status) return status;
        const auto next_points = candidate.edges.back().points.size();
        if (next_points > options.max_total_points - point_count)
            return error(RouteError::CapacityExceeded, "polyline total waypoint budget exceeded", i);
        point_count += next_points;
    }
    status = validate_polyline_routes(layout, requests, candidate, options);
    if (!status) return status;
    out = std::move(candidate);
    return {};
}

} // namespace stun::graphlayout
