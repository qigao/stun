#include "stun/graphlayout/polyline.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace stun::graphlayout;

namespace {
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
bool eq(Point a, Point b) { return a.x == b.x && a.y == b.y; }
Layout boxes(std::initializer_list<PlacedNode> input) {
    Layout result;
    result.nodes.assign(input.begin(), input.end());
    return result;
}
void validate(const Layout& l, const std::vector<PolylineCheckpointRequest>& requests,
              const Routes& routes, const PolylineOptions& opt = {}) {
    const auto s = validate_polyline_checkpoint_routes(l, requests, routes, opt);
    if (!s) throw std::runtime_error("polyline checkpoint geometry: " + s.message);
}
bool equal_routes(const Routes& a, const Routes& b) {
    if (a.edges.size() != b.edges.size()) return false;
    for (std::size_t i = 0; i < a.edges.size(); ++i) {
        if (a.edges[i].points.size() != b.edges[i].points.size()) return false;
        for (std::size_t j = 0; j < a.edges[i].points.size(); ++j)
            if (!eq(a.edges[i].points[j], b.edges[i].points[j])) return false;
    }
    return true;
}
std::size_t index(const Route& route, Point p) {
    for (std::size_t i = 0; i < route.points.size(); ++i)
        if (eq(route.points[i], p)) return i;
    return route.points.size();
}
}

int main() {
    try {
        const Layout simple = boxes({{0,0,40,40}, {200,0,40,40}});
        const RouteRequest base{{0,Side::East},{1,Side::West}};
        const Point first{95,70}, second{135,90};
        std::vector<PolylineCheckpointRequest> requests{{base, {first, second}}};
        Routes routed, repeat;
        auto status = route_polyline_checkpoints(simple, requests, routed);
        if (!status) throw std::runtime_error("ordered checkpoints: " + status.message);
        validate(simple, requests, routed);
        check(index(routed.edges[0], first) > 1 &&
              index(routed.edges[0], first) < index(routed.edges[0], second) &&
              index(routed.edges[0], second) < routed.edges[0].points.size()-2,
              "required waypoints are exact and ordered");
        check(static_cast<bool>(route_polyline_checkpoints(simple, requests, repeat)),
              "checkpointed route reruns");
        check(equal_routes(routed, repeat), "checkpoint routing deterministic");
        Routes corrupted = routed;
        // Swapping the interior mandatory points invalidates at least one leg.
        const auto i = index(corrupted.edges[0], first);
        const auto j = index(corrupted.edges[0], second);
        std::swap(corrupted.edges[0].points[i], corrupted.edges[0].points[j]);
        check(validate_polyline_checkpoint_routes(simple,requests,corrupted).error ==
                  RouteError::InternalInvariant,
              "validator detects skipped/out-of-order mandatory checkpoints");

        {
            std::vector<RouteRequest> plain{base};
            std::vector<PolylineCheckpointRequest> no_checkpoints{{base,{}}};
            Routes a,b;
            check(static_cast<bool>(route_polyline(simple,plain,a)),"classic API succeeds");
            check(static_cast<bool>(route_polyline_checkpoints(simple,no_checkpoints,b)),
                  "checkpoint API with empty list succeeds");
            check(equal_routes(a,b),"classic Polyline exact geometry preserved");
        }
        {
            // A mandatory checkpoint forces a longer legitimate path through
            // open space rather than being silently ignored by the A* solver.
            const Layout blocked = boxes({{0,35,30,30},{60,0,60,110},{180,35,30,30}});
            std::vector<PolylineCheckpointRequest> req{{
                {{0,Side::East},{2,Side::West}}, {{140,-25},{140,130}}
            }};
            Routes result;
            const auto s = route_polyline_checkpoints(blocked,req,result);
            if (!s) throw std::runtime_error("obstacle checkpoint: " + s.message);
            validate(blocked,req,result);
            check(index(result.edges[0],{140,-25}) < index(result.edges[0],{140,130}),
                  "obstacle detour traverses checkpoints in order");
        }
        {
            const Layout one = boxes({{20,20,60,35}});
            std::vector<PolylineCheckpointRequest> req{{
                {{0,Side::East},{0,Side::North}}, {{125,80}}
            }};
            Routes out;
            const auto s = route_polyline_checkpoints(one,req,out);
            if (!s) throw std::runtime_error("self loop checkpoint: " + s.message);
            validate(one,req,out);
            check(index(out.edges[0],{125,80}) < out.edges[0].points.size()-2,
                  "self-loop preserves mandatory bend");
        }
        {
            Routes out;
            out.edges.resize(3);
            auto invalid = requests;
            invalid[0].checkpoints={{110,15},{110,15}};
            check(route_polyline_checkpoints(simple,invalid,out).error == RouteError::InvalidInput,
                  "duplicate adjacent checkpoint rejected");
            check(out.edges.empty(),"invalid request clears entire batch");
            invalid[0].checkpoints={{48,20}}; // exact source stub
            check(route_polyline_checkpoints(simple,invalid,out).error == RouteError::InvalidInput,
                  "checkpoint cannot alias source stub");
            invalid[0].checkpoints={{110,std::numeric_limits<double>::infinity()}};
            check(route_polyline_checkpoints(simple,invalid,out).error == RouteError::InvalidInput,
                  "nonfinite checkpoint rejected");
            invalid[0].checkpoints={{20,20}};
            check(route_polyline_checkpoints(simple,invalid,out).error == RouteError::InvalidInput,
                  "checkpoint inside obstacle rejected");
            invalid[0].checkpoints={{110,10},{130,20}};
            PolylineOptions opt;
            opt.max_checkpoints_per_route=1;
            check(route_polyline_checkpoints(simple,invalid,out,opt).error == RouteError::CapacityExceeded,
                  "per-route checkpoint cap");
            opt.max_checkpoints_per_route=32;
            opt.max_total_checkpoints=1;
            check(route_polyline_checkpoints(simple,invalid,out,opt).error == RouteError::CapacityExceeded,
                  "global checkpoint cap");
            opt.max_total_checkpoints=4096;
            opt.max_expansions=1;
            check(route_polyline_checkpoints(simple,invalid,out,opt).error == RouteError::CapacityExceeded,
                  "A* expansion cap applies across all checkpoint legs");
            opt.max_expansions=2048;
            opt.max_segment_candidates=1;
            check(route_polyline_checkpoints(simple,invalid,out,opt).error == RouteError::CapacityExceeded,
                  "visibility work cap applies across checkpoint legs");
            check(out.edges.empty(),"capacity failure clears output");
        }
        {
            std::vector<PolylineCheckpointRequest> multiple{
                {base, {{95,70},{135,90}}},
                {base, {{95,-50},{135,-50}}}
            };
            Routes out, reordered;
            check(static_cast<bool>(route_polyline_checkpoints(simple,multiple,out)),
                  "two independently checkpointed routes");
            validate(simple,multiple,out);
            std::swap(multiple[0],multiple[1]);
            check(static_cast<bool>(route_polyline_checkpoints(simple,multiple,reordered)),
                  "request reordering accepted");
            check(out.edges[0].points.size()==reordered.edges[1].points.size(),
                  "input request identity preserved");
            for(std::size_t i=0;i<out.edges[0].points.size();++i)
                check(eq(out.edges[0].points[i],reordered.edges[1].points[i]),
                      "request permutation reproduces exact path");
        }
        std::cout << "polyline-checkpoints: all checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "polyline-checkpoints: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
