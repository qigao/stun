#include "stun/graphlayout/orthogonal.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace stun::graphlayout;

namespace {
void check(bool ok, const char* msg) {
    if (!ok) throw std::runtime_error(msg);
}

void check_valid(const Layout& layout, const std::vector<RouteRequest>& req,
                 const Routes& routes, const RouteOptions& options = {}) {
    const auto result = validate_orthogonal_routes(layout, req, routes, options);
    if (!result) {
        std::cerr << "route[" << result.route_index << "]: " << result.message << '\n';
        throw std::runtime_error("geometric postcondition failed");
    }
}

Layout boxes(std::initializer_list<PlacedNode> init) {
    Layout l;
    l.nodes = init;
    for (const auto& n : l.nodes) {
        l.width = std::max(l.width, n.x + n.width);
        l.height = std::max(l.height, n.y + n.height);
    }
    return l;
}

bool equal(const Routes& a, const Routes& b) {
    if (a.edges.size() != b.edges.size()) return false;
    for (std::size_t i = 0; i < a.edges.size(); ++i) {
        const auto& x = a.edges[i].points;
        const auto& y = b.edges[i].points;
        if (x.size() != y.size()) return false;
        for (std::size_t j = 0; j < x.size(); ++j)
            if (x[j].x != y[j].x || x[j].y != y[j].y) return false;
    }
    return true;
}
}

int main() {
    try {
        {
            Routes result;
            check(static_cast<bool>(route_orthogonal({}, {}, result)), "empty route batch success");
            check(result.edges.empty(), "empty result");
        }
        {
            auto layout = boxes({{0, 0, 40, 40}, {120, 0, 60, 40}});
            std::vector<RouteRequest> requests{{{0}, {1}}};
            Routes route;
            auto status = route_orthogonal(layout, requests, route);
            if (!status) std::cerr << status.message << '\n';
            check(static_cast<bool>(status), "direct east-west route");
            check_valid(layout, requests, route);
            check(route.edges[0].points.size() == 2, "direct route collapses to two anchors");
            check(route.edges[0].points.front().x == 40, "source east port");
            check(route.edges[0].points.back().x == 120, "destination west port");
        }
        {
            auto layout = boxes({{0, 40, 40, 40}, {80, 20, 40, 80}, {160, 40, 40, 40}});
            std::vector<RouteRequest> requests{{{0, Side::East}, {2, Side::West}}};
            Routes route, repeat;
            auto status = route_orthogonal(layout, requests, route);
            if (!status) std::cerr << status.message << '\n';
            check(static_cast<bool>(status), "obstacle detour");
            check(route.edges[0].points.size() >= 4, "route must bend around obstacle");
            check_valid(layout, requests, route);
            check(static_cast<bool>(route_orthogonal(layout, requests, repeat)), "rerun");
            check(equal(route, repeat), "bit-exact deterministic route");
        }
        {
            auto layout = boxes({{20, 20, 80, 50}});
            std::vector<RouteRequest> requests{{{0}, {0}}};
            Routes route;
            auto status = route_orthogonal(layout, requests, route);
            if (!status) std::cerr << status.message << '\n';
            check(static_cast<bool>(status), "self loop");
            check_valid(layout, requests, route);
            check(route.edges[0].points.size() >= 4, "self loop wraps around its own node");
        }
        {
            auto layout = boxes({{0,0,35,35}, {80,85,35,35}, {160,0,35,35}});
            std::vector<RouteRequest> requests{
                {{0, Side::North, 0.25}, {1, Side::South, 0.75}},
                {{1, Side::West, 0.7}, {2, Side::East, 0.2}},
                {{2, Side::South, 1.0}, {0, Side::West, 0.0}}};
            Routes route;
            auto status = route_orthogonal(layout, requests, route);
            if (!status) std::cerr << status.route_index << ":" << status.message << '\n';
            check(static_cast<bool>(status), "fractional directional ports");
            check_valid(layout, requests, route);
        }
        {
            auto layout = boxes({{0, 0, 40, 40}, {45, 0, 40, 40}});
            std::vector<RouteRequest> requests{{{0, Side::East}, {1, Side::West}}};
            Routes result;
            result.edges.resize(10);
            auto status = route_orthogonal(layout, requests, result, {});
            check(status.error == RouteError::NoPath, "clearance conflict must be explicit");
            check(result.edges.empty(), "failure is transactional");
        }
        {
            auto layout = boxes({{0, 0, 40, 40}, {120, 0, 40, 40}});
            std::vector<RouteRequest> requests{{{0}, {1}}};
            Routes result;
            RouteOptions options;
            options.max_grid_vertices = 1;
            check(route_orthogonal(layout, requests, result, options).error == RouteError::CapacityExceeded,
                  "grid capacity gate");
            options = {};
            options.max_expansions = 1;
            check(route_orthogonal(layout, requests, result, options).error == RouteError::CapacityExceeded,
                  "expansion gate");
            options = {};
            options.max_routes = 0;
            check(route_orthogonal(layout, requests, result, options).error == RouteError::InvalidOptions,
                  "invalid capacity rejected");
            requests[0].source.node = 50;
            check(route_orthogonal(layout, requests, result).error == RouteError::InvalidInput,
                  "unknown node rejected");
            requests[0].source.node = 0;
            requests[0].source.offset = -0.1;
            check(route_orthogonal(layout, requests, result).error == RouteError::InvalidInput,
                  "invalid port fraction rejected");
            requests[0].source.offset = 0.5;
            layout.nodes[0].height = -1;
            check(route_orthogonal(layout, requests, result).error == RouteError::InvalidInput,
                  "bad rectangles rejected");
        }
        {
            Graph g;
            g.nodes = {{"x", 60, 40}, {"y", 60, 40}, {"z", 60, 40}, {"w", 60, 40}};
            g.edges = {{0,1}, {1,2}, {2,0}, {2,3}, {3,3}};
            Layout layout;
            check(static_cast<bool>(layout_layered(g, layout)), "SCC layout for routing");
            std::vector<RouteRequest> requests;
            for (const auto& e : g.edges) requests.push_back({{e.source}, {e.target}});
            Routes result;
            auto status = route_orthogonal(layout, requests, result);
            if (!status) std::cerr << "SCC route " << status.route_index << ": " << status.message << '\n';
            check(static_cast<bool>(status), "SCC and self-loop integration");
            check_valid(layout, requests, result);
        }
        {
            // A mesh of obstacles must not make the route generator lose
            // determinism, miss an obstacle, or select an implicit fallback.
            Layout l;
            for (std::size_t row = 0; row < 5; ++row)
                for (std::size_t col = 0; col < 5; ++col)
                    l.nodes.push_back({static_cast<double>(col * 95),
                                       static_cast<double>(row * 90),
                                       32.0 + (col % 3) * 5, 28.0 + (row % 4) * 7});
            std::vector<RouteRequest> requests;
            for (std::size_t i = 0; i < 32; ++i)
                requests.push_back({{(i * 11 + 2) % l.nodes.size()},
                                    {(i * 17 + 7) % l.nodes.size()}});
            Routes a, b;
            auto status = route_orthogonal(l, requests, a);
            if (!status) std::cerr << "mesh[" << status.route_index << "]: " << status.message << '\n';
            check(static_cast<bool>(status), "dense mesh routing succeeds");
            check_valid(l, requests, a);
            check(static_cast<bool>(route_orthogonal(l, requests, b)), "repeat dense mesh");
            check(equal(a, b), "all 32 routes repeat bit-exactly");
        }
        {
            Layout original = boxes({{0, 0, 50, 40}, {80, 90, 45, 50}, {160, 0, 60, 40}});
            std::vector<RouteRequest> req_a{{{0}, {2}}, {{1}, {0}}, {{2}, {2}}};
            Routes routes_a, routes_b;
            check(static_cast<bool>(route_orthogonal(original, req_a, routes_a)), "base permutation layout");
            Layout permuted = boxes({original.nodes[2], original.nodes[0], original.nodes[1]});
            std::vector<RouteRequest> req_b{{{1}, {0}}, {{2}, {1}}, {{0}, {0}}};
            check(static_cast<bool>(route_orthogonal(permuted, req_b, routes_b)), "permuted obstacle layout");
            check(equal(routes_a, routes_b), "obstacle input permutation does not change route geometry");
        }
        std::cout << "graphlayout orthogonal routing: all checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "graphlayout orthogonal routing: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
