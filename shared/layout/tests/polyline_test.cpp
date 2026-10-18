#include "stun/graphlayout/polyline.h"
#include "stun/graphlayout/orthogonal.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace stun::graphlayout;
namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
Layout layout(std::initializer_list<PlacedNode> boxes) {
    Layout result;
    result.nodes = boxes;
    return result;
}
bool same(const Routes& a, const Routes& b) {
    if (a.edges.size() != b.edges.size()) return false;
    for (std::size_t i = 0; i < a.edges.size(); ++i) {
        const auto& x = a.edges[i].points, &y = b.edges[i].points;
        if (x.size() != y.size()) return false;
        for (std::size_t j = 0; j < x.size(); ++j)
            if (x[j].x != y[j].x || x[j].y != y[j].y) return false;
    }
    return true;
}
void validate(const Layout& l, const std::vector<RouteRequest>& req, const Routes& r,
              const PolylineOptions& o = {}) {
    auto s = validate_polyline_routes(l, req, r, o);
    if (!s) throw std::runtime_error("polyline geometry: " + s.message +
        " at " + std::to_string(s.route_index));
}
double length(const Route& route) {
    double s = 0;
    for (std::size_t i = 1; i < route.points.size(); ++i)
        s += std::hypot(route.points[i].x-route.points[i-1].x,
                        route.points[i].y-route.points[i-1].y);
    return s;
}
}
int main() {
    try {
        {
            Routes out;
            check(static_cast<bool>(route_polyline({}, {}, out)), "empty graph works");
            check(out.edges.empty(), "empty routes retained");
        }
        {
            auto l = layout({{0,0,40,40}, {130,50,35,35}});
            std::vector<RouteRequest> req{{{0,Side::East}, {1,Side::West}}};
            Routes r, repeat;
            auto status = route_polyline(l, req, r);
            if (!status) std::cerr << status.message << '\n';
            check(static_cast<bool>(status), "unobstructed diagonal polyline");
            validate(l,req,r);
            check(r.edges[0].points.size()==4, "direct port-to-port with exposed stubs");
            check(r.edges[0].points[1].x==48 && r.edges[0].points[1].y==20,
                  "source stub exact");
            check(r.edges[0].points[2].x==122 && r.edges[0].points[2].y==67.5,
                  "target stub exact");
            check(std::abs(r.edges[0].points[1].x - r.edges[0].points[2].x)>0 &&
                  std::abs(r.edges[0].points[1].y - r.edges[0].points[2].y)>0,
                  "middle segment genuinely diagonal");
            check(static_cast<bool>(route_polyline(l,req,repeat)), "repeat route");
            check(same(r,repeat), "deterministic bit-identical output");
        }
        {
            auto l = layout({{0,35,30,30}, {50,0,80,100}, {170,35,30,30}});
            std::vector<RouteRequest> req{{{0,Side::East}, {2,Side::West}}};
            Routes r;
            auto s = route_polyline(l,req,r);
            if (!s) std::cerr << s.message << '\n';
            check(static_cast<bool>(s), "obstacle bypass");
            check(r.edges[0].points.size()>=5, "need at least one obstacle-corner waypoint");
            validate(l,req,r);
            Routes forged = r;
            // A path through the center of the inflated middle obstacle is not valid,
            // even if the first/last port stubs are correct.
            forged.edges[0].points = {r.edges[0].points.front(), r.edges[0].points[1],
                                      {90.0,50.0},r.edges[0].points[r.edges[0].points.size()-2],
                                      r.edges[0].points.back()};
            check(validate_polyline_routes(l,req,forged).error == RouteError::InternalInvariant,
                  "independent validator rejects a tunneling diagonal");
        }
        {
            auto l = layout({{20,20,80,50}});
            std::vector<RouteRequest> req{{{0}, {0}}};
            Routes r;
            auto s = route_polyline(l,req,r);
            if (!s) std::cerr << s.message << '\n';
            check(static_cast<bool>(s), "default self-loop wraps around shape");
            validate(l,req,r);
            check(r.edges[0].points.size()>=5, "self-loop must have a nontrivial bend");
            req[0] = {{0,Side::East,0.5}, {0,Side::East,0.5}};
            check(route_polyline(l,req,r).error==RouteError::NoPath,
                  "coincident self-loop endpoints fail explicitly");
            check(r.edges.empty(), "self-loop failure transactionally empties output");
        }
        {
            auto l=layout({{0,0,40,40}, {50,0,40,40}});
            std::vector<RouteRequest> req{{{0,Side::East}, {1,Side::West}}};
            Routes r;
            r.edges.resize(8);
            check(route_polyline(l,req,r).error==RouteError::NoPath,
                  "clearance-incompatible opposing ports must fail");
            check(r.edges.empty(), "NoPath clears prior result");
            PolylineOptions options;
            options.clearance=0;
            check(static_cast<bool>(route_polyline(l,req,r,options)),
                  "zero-clearance gap supports explicit stubs");
            validate(l,req,r,options);
        }
        {
            auto l=layout({{0,0,40,40}, {140,0,40,40}});
            std::vector<RouteRequest> req{{{0,Side::North,0}, {1,Side::South,1}}};
            Routes r;
            check(static_cast<bool>(route_polyline(l,req,r)), "corner ports accepted");
            validate(l,req,r);
            req[0].source.offset=-0.01;
            check(route_polyline(l,req,r).error==RouteError::InvalidInput,
                  "negative port fraction forbidden");
            req[0].source.offset=0.5;
            req[0].source.side=static_cast<Side>(99);
            check(route_polyline(l,req,r).error==RouteError::InvalidInput,
                  "undefined side forbidden");
        }
        {
            auto l=layout({{0,0,25,25}, {150,0,25,25}});
            std::vector<RouteRequest> req{{{0},{1}}};
            Routes r;
            PolylineOptions o;
            o.max_visibility_vertices=3;
            check(route_polyline(l,req,r,o).error==RouteError::CapacityExceeded,
                  "visibility vertices budget");
            o.max_visibility_vertices=2048;
            o.max_segment_candidates=1;
            check(route_polyline(l,req,r,o).error==RouteError::CapacityExceeded,
                  "candidate count budget");
            o.max_segment_candidates=100000;
            o.max_obstacle_tests=1;
            check(route_polyline(l,req,r,o).error==RouteError::CapacityExceeded,
                  "obstacle checks budget");
            o.max_obstacle_tests=100000;
            o.max_expansions=1;
            check(route_polyline(l,req,r,o).error==RouteError::CapacityExceeded,
                  "A* expansion budget");
            o.max_expansions=2048;
            o.max_total_points=2;
            check(route_polyline(l,req,r,o).error==RouteError::CapacityExceeded,
                  "batch waypoint budget");
            check(r.edges.empty(), "failed budget has no partial routes");
            o.max_total_points=100;
            o.clearance=-1;
            check(route_polyline(l,req,r,o).error==RouteError::InvalidOptions,
                  "negative clearance forbidden");
            o.clearance=8;
            req[0].target.node=2;
            check(route_polyline(l,req,r,o).error==RouteError::InvalidInput,
                  "out-of-bounds endpoint forbidden");
            l.nodes[1].x=5;
            req[0].target.node=1;
            check(route_polyline(l,req,r,o).error==RouteError::InvalidInput,
                  "overlapping boxes invalid");
        }
        {
            auto l=layout({{0,0,20,20}, {100,0,20,20}, {200,40,20,20}});
            std::vector<RouteRequest> req{{{0},{2}}, {{2},{1}}, {{1},{0}}};
            Routes result, repeat;
            check(static_cast<bool>(route_polyline(l,req,result)), "batched routing");
            validate(l,req,result);
            check(result.edges.size()==3, "stable order and edge cardinality");
            check(static_cast<bool>(route_polyline(l,req,repeat)), "batch repeated");
            check(same(result,repeat), "batch repeat determinism");
        }
        {
            auto original=layout({{0,20,20,25}, {110,100,20,25}, {60,0,20,25}, {60,60,20,25}});
            auto permuted=layout({original.nodes[2],original.nodes[3],original.nodes[0],original.nodes[1]});
            std::vector<RouteRequest> a{{{0,Side::East},{1,Side::North}}};
            std::vector<RouteRequest> b{{{2,Side::East},{3,Side::North}}};
            Routes ra, rb;
            check(static_cast<bool>(route_polyline(original,a,ra)), "node order base route");
            check(static_cast<bool>(route_polyline(permuted,b,rb)), "node order permuted route");
            validate(original,a,ra); validate(permuted,b,rb);
            check(same(ra,rb), "permutation-invariant geometric tie-breaking");
        }
        {
            // A bounded, deterministic obstacle-field corpus. These are
            // numerical/geometry regressions, not global optimality proofs.
            Layout l;
            for(std::size_t y=0;y<5;++y)
                for(std::size_t x=0;x<5;++x)
                    l.nodes.push_back({static_cast<double>(x*70),
                                       static_cast<double>(y*70),20,25});
            for(std::size_t k=0;k<24;++k) {
                const std::size_t start=(k*7)%25;
                std::size_t goal=(start+9+k*3)%25;
                if(start==goal) goal=(goal+1)%25;
                std::vector<RouteRequest> req{{{start}, {goal}}};
                Routes r;
                auto s=route_polyline(l,req,r);
                if (!s) std::cerr << "mesh["<<k<<"] "<<s.message<<'\n';
                check(static_cast<bool>(s), "obstacle field route");
                validate(l,req,r);
                check(std::isfinite(length(r.edges.front())) && length(r.edges.front())>0,
                      "finite positive route length");
            }
        }
        std::cout << "polyline: all checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "polyline: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
