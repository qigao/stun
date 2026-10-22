#include "stun/graphlayout/nudging.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <tuple>
#include <utility>
#include <vector>

namespace stun::graphlayout {
namespace {

constexpr double kMaxCoordinate = 1.0e11;
constexpr double kTolerance = 1.0e-9;

RouteStatus fail(RouteError error, const char* message, std::size_t index = 0) {
    return {error, message, index};
}

bool valid_options(const NudgingOptions& o) {
    return std::isfinite(o.lane_spacing) && o.lane_spacing > 0.0 &&
           o.lane_spacing <= 1.0e7 &&
           o.max_routes > 0 && o.max_routes <= o.routing.max_routes &&
           o.max_passes > 0 && o.max_passes <= 8 &&
           o.max_lanes > 0 && o.max_lanes <= 16 &&
           o.max_candidates > 0 && o.max_segment_pair_checks > 0 &&
           o.max_points_per_route >= 6 && o.max_total_points >= 6;
}

bool valid_shape(const Routes& r, const NudgingOptions& o) {
    std::size_t points = 0;
    for (const auto& path : r.edges) {
        if (path.points.size() < 2 || path.points.size() > o.max_points_per_route ||
            path.points.size() > o.max_total_points - points) return false;
        points += path.points.size();
    }
    return true;
}

struct Meter {
    std::size_t checks = 0;
    std::size_t ceiling = 0;
};

bool add_interaction(RouteInteractions& score, Point a, Point b,
                     Point c, Point d) {
    // The caller has separately checked that both paths are orthogonal.
    const bool h1 = a.y == b.y, h2 = c.y == d.y;
    if (h1 == h2) {
        if (h1 && a.y == c.y) {
            const double amount = std::min(std::max(a.x,b.x),std::max(c.x,d.x)) -
                                  std::max(std::min(a.x,b.x),std::min(c.x,d.x));
            if (amount > 0) score.shared_length += amount;
        } else if (!h1 && a.x == c.x) {
            const double amount = std::min(std::max(a.y,b.y),std::max(c.y,d.y)) -
                                  std::max(std::min(a.y,b.y),std::min(c.y,d.y));
            if (amount > 0) score.shared_length += amount;
        }
    } else {
        const Point h1p = h1 ? a : c, h2p = h1 ? b : d;
        const Point v1p = h1 ? c : a, v2p = h1 ? d : b;
        // Count proper crosses, not touching bends/port anchors at endpoints.
        if (v1p.x > std::min(h1p.x,h2p.x) && v1p.x < std::max(h1p.x,h2p.x) &&
            h1p.y > std::min(v1p.y,v2p.y) && h1p.y < std::max(v1p.y,v2p.y))
            ++score.crossings;
    }
    return std::isfinite(score.shared_length) && score.shared_length <= kMaxCoordinate &&
           score.crossings != std::numeric_limits<std::size_t>::max();
}

// Add pairwise contributions to 'score'; Meter is shared across the complete
// run, including every candidate so quadratic search cannot evade its budget.
RouteStatus accumulate(const Route& a, const Route& b,
                       RouteInteractions& score, Meter& meter,
                       std::size_t index) {
    for (std::size_t i = 1; i < a.points.size(); ++i) {
        for (std::size_t j = 1; j < b.points.size(); ++j) {
            if (meter.checks >= meter.ceiling)
                return fail(RouteError::CapacityExceeded,
                            "joint route segment-pair operation budget exhausted", index);
            ++meter.checks;
            if (!add_interaction(score,a.points[i-1],a.points[i],
                                 b.points[j-1],b.points[j]))
                return fail(RouteError::CapacityExceeded, "joint route metric overflow", index);
        }
    }
    return {};
}

RouteStatus measure_all(const Routes& routes, RouteInteractions& out,
                        Meter& meter) {
    RouteInteractions score;
    for (std::size_t i = 0; i < routes.edges.size(); ++i)
        for (std::size_t j = i + 1; j < routes.edges.size(); ++j) {
            const auto status = accumulate(routes.edges[i], routes.edges[j], score, meter, i);
            if (!status) return status;
        }
    out = score;
    return {};
}

RouteStatus relative_score(const Routes& routes, std::size_t route_index,
                           const Route& replacement, RouteInteractions& score,
                           Meter& meter) {
    RouteInteractions tally;
    for (std::size_t i = 0; i < routes.edges.size(); ++i) {
        if (i == route_index) continue;
        auto status = accumulate(replacement, routes.edges[i], tally, meter, route_index);
        if (!status) return status;
    }
    score = tally;
    return {};
}

bool improves(const RouteInteractions& a, const RouteInteractions& b) {
    // Never introduce additional proper edge crossings merely to unbundle.
    if (a.crossings < b.crossings) return true;
    return a.crossings == b.crossings && a.shared_length + kTolerance < b.shared_length;
}

bool equivalent(const RouteInteractions& a, const RouteInteractions& b) {
    return a.crossings == b.crossings &&
           std::abs(a.shared_length - b.shared_length) <= kTolerance;
}

double length(const Route& path) {
    double result = 0;
    for (std::size_t i = 1; i < path.points.size(); ++i)
        result += std::abs(path.points[i].x - path.points[i-1].x) +
                  std::abs(path.points[i].y - path.points[i-1].y);
    return result;
}

bool dogleg(const Route& base, std::size_t j, double delta,
            const NudgingOptions& opt, Route& candidate) {
    const Point a=base.points[j], b=base.points[j+1];
    const bool horizontal = a.y == b.y;
    const double span = horizontal ? std::abs(b.x-a.x) : std::abs(b.y-a.y);
    // Avoid a bend inside an endpoint's own clearance envelope. At least two
    // positive uninterrupted leads keep source/target directions unchanged.
    const double lead = std::max(opt.routing.clearance + 1.0,
                                 std::min(opt.lane_spacing, span / 4.0));
    if (!std::isfinite(lead) || span <= 2.0 * lead + 1.0) return false;
    const double sign = horizontal ? (b.x > a.x ? 1.0 : -1.0) :
                                     (b.y > a.y ? 1.0 : -1.0);
    Point p1=a, p2=a, p3=b, p4=b;
    if (horizontal) {
        p1.x += sign*lead; p2.x=p1.x; p2.y+=delta;
        p4.x-=sign*lead; p3.x=p4.x; p3.y+=delta;
    } else {
        p1.y+=sign*lead; p2.y=p1.y; p2.x+=delta;
        p4.y-=sign*lead; p3.y=p4.y; p3.x+=delta;
    }
    for (const auto p : {p1,p2,p3,p4})
        if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
            std::abs(p.x)>kMaxCoordinate || std::abs(p.y)>kMaxCoordinate)
            return false;
    if (base.points.size() > opt.max_points_per_route - 4) return false;
    candidate.points.clear();
    candidate.points.reserve(base.points.size()+4);
    candidate.points.insert(candidate.points.end(),base.points.begin(),base.points.begin()+j+1);
    for (auto p : {p1,p2,p3,p4}) candidate.points.push_back(p);
    candidate.points.insert(candidate.points.end(),base.points.begin()+j+1,base.points.end());
    return true;
}

// With distinct endpoint IDs/port geometry, choices do not depend on the input
// edge vector permutation. Identical requests are interchangeable by geometry.
std::vector<std::size_t> stable_request_order(const std::vector<RouteRequest>& requests) {
    std::vector<std::size_t> order(requests.size());
    std::iota(order.begin(), order.end(), 0);
    auto key = [&](std::size_t i) {
        const auto& r=requests[i];
        return std::make_tuple(r.source.node, static_cast<int>(r.source.side),
                               r.source.offset, r.target.node,
                               static_cast<int>(r.target.side), r.target.offset);
    };
    std::stable_sort(order.begin(),order.end(),[&](std::size_t a,std::size_t b){
        return key(a)<key(b);
    });
    return order;
}

} // namespace

RouteStatus audit_orthogonal_interactions(const Layout& layout,
                                          const std::vector<RouteRequest>& requests,
                                          const Routes& routes,
                                          RouteInteractions& out,
                                          const NudgingOptions& options) {
    out={};
    if (!valid_options(options))
        return fail(RouteError::InvalidOptions,"invalid joint routing options");
    if (requests.size()>options.max_routes || !valid_shape(routes,options))
        return fail(RouteError::CapacityExceeded,"joint route count or point budget exceeded");
    const auto verified=validate_orthogonal_routes(layout,requests,routes,options.routing);
    if (!verified) return verified;
    Meter meter{0,options.max_segment_pair_checks};
    return measure_all(routes,out,meter);
}

RouteStatus route_orthogonal_nudged(const Layout& layout,
                                    const std::vector<RouteRequest>& requests,
                                    Routes& out,
                                    const NudgingOptions& options,
                                    NudgingReport* report) {
    out={};
    if (report) *report={};
    if (!valid_options(options))
        return fail(RouteError::InvalidOptions,"invalid joint routing options");
    if (requests.size()>options.max_routes)
        return fail(RouteError::CapacityExceeded,"joint route count budget exceeded");
    Routes candidate;
    const auto base=route_orthogonal(layout,requests,candidate,options.routing);
    if (!base) return base;
    if (!valid_shape(candidate,options))
        return fail(RouteError::CapacityExceeded,"joint route base point budget exceeded");
    NudgingReport summary;
    Meter meter{0,options.max_segment_pair_checks};
    auto check=measure_all(candidate,summary.initial,meter);
    if (!check) return check;
    const auto order=stable_request_order(requests);
    std::vector<unsigned char> changed(requests.size(),0);
    std::size_t total_points=0;
    for (const auto& route : candidate.edges) total_points+=route.points.size();
    for (std::size_t pass=0;pass<options.max_passes;++pass) {
        bool progress=false;
        for (std::size_t index : order) {
            RouteInteractions original;
            check=relative_score(candidate,index,candidate.edges[index],original,meter);
            if (!check) return check;
            if (original.crossings==0 && original.shared_length <= kTolerance) continue;
            Route best;
            RouteInteractions best_score=original;
            double best_length=length(candidate.edges[index]);
            bool improved=false;
            const auto& current=candidate.edges[index];
            for (std::size_t j=0;j+1<current.points.size();++j) {
                for (std::size_t lane=1;lane<=options.max_lanes;++lane) {
                    for (int polarity : {-1,1}) {
                        if (summary.candidate_trials>=options.max_candidates)
                            return fail(RouteError::CapacityExceeded,
                                        "joint routing candidate budget exhausted",index);
                        ++summary.candidate_trials;
                        Route proposed;
                        if (!dogleg(current,j,options.lane_spacing * static_cast<double>(lane) *
                                     static_cast<double>(polarity),options,proposed)) continue;
                        if (total_points - current.points.size() + proposed.points.size() >
                            options.max_total_points) continue;
                        Routes one; one.edges.push_back(proposed);
                        const auto verified=validate_orthogonal_routes(
                            layout,{requests[index]},one,options.routing);
                        if (!verified) continue;
                        RouteInteractions trial;
                        check=relative_score(candidate,index,proposed,trial,meter);
                        if (!check) return check;
                        const double proposed_length=length(proposed);
                        const bool better=improves(trial,best_score) ||
                            (improved && equivalent(trial,best_score) &&
                             proposed_length + kTolerance < best_length);
                        if (better) {
                            best=std::move(proposed);
                            best_score=trial;
                            best_length=proposed_length;
                            improved=true;
                        }
                    }
                }
            }
            if (improved) {
                total_points=total_points-current.points.size()+best.points.size();
                candidate.edges[index]=std::move(best);
                ++summary.accepted_moves;
                if (!changed[index]) { changed[index]=1; ++summary.changed_routes; }
                progress=true;
            }
        }
        if (!progress) break;
    }
    check=measure_all(candidate,summary.final,meter);
    if (!check) return check;
    summary.segment_pair_checks=meter.checks;
    if (summary.final.crossings > summary.initial.crossings ||
        (summary.final.crossings == summary.initial.crossings &&
         summary.final.shared_length > summary.initial.shared_length+kTolerance))
        return fail(RouteError::InternalInvariant,"joint route interaction regression");
    check=validate_orthogonal_routes(layout,requests,candidate,options.routing);
    if (!check) return check;
    out=std::move(candidate);
    if (report) *report=summary;
    return {};
}

} // namespace stun::graphlayout
