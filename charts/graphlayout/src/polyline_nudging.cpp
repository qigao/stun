#include "stun/graphlayout/polyline_nudging.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace stun::graphlayout {
namespace {

using Real = long double;
constexpr Real kEpsilon = 1e-12L;

RouteStatus error(RouteError code, const char* message, std::size_t index = 0) {
    return {code, message, index};
}

bool valid(const PolylineNudgingOptions& o) {
    return std::isfinite(o.lane_spacing) && o.lane_spacing > 0.0 &&
           o.max_routes > 0 && o.max_passes > 0 && o.max_passes <= 32 &&
           o.max_lanes > 0 && o.max_lanes <= 32 &&
           o.max_candidates > 0 && o.max_segment_pair_checks > 0 &&
           o.max_points_per_route >= 4 && o.max_total_points >= 4 &&
           o.max_total_points >= o.max_points_per_route;
}

bool bounds_ok(const Routes& routes, const PolylineNudgingOptions& options) {
    std::size_t points = 0;
    for (const Route& route : routes.edges) {
        if (route.points.size() > options.max_points_per_route ||
            route.points.size() > options.max_total_points - points) return false;
        points += route.points.size();
    }
    return true;
}

Real cross(Point a, Point b, Point c) {
    const Real dx = static_cast<Real>(b.x) - a.x;
    const Real dy = static_cast<Real>(b.y) - a.y;
    const Real ex = static_cast<Real>(c.x) - a.x;
    const Real ey = static_cast<Real>(c.y) - a.y;
    return dx * ey - dy * ex;
}

// Signed area with a length-dependent numeric guard. This is a numerical
// geometric audit (not an exact arithmetic predicate for arbitrary doubles).
int orientation(Point a, Point b, Point c) {
    const Real area = cross(a,b,c);
    const Real ab = std::hypot(static_cast<Real>(b.x)-a.x,
                               static_cast<Real>(b.y)-a.y);
    const Real ac = std::hypot(static_cast<Real>(c.x)-a.x,
                               static_cast<Real>(c.y)-a.y);
    const Real tol = kEpsilon * std::max(1.0L,ab*ac);
    if (area > tol) return 1;
    if (area < -tol) return -1;
    return 0;
}

struct Interaction { bool crossing = false; double overlap = 0.0; };
Interaction compare_segments(Point a, Point b, Point c, Point d) {
    const int s1 = orientation(a,b,c), s2 = orientation(a,b,d);
    const int s3 = orientation(c,d,a), s4 = orientation(c,d,b);
    if (s1*s2 < 0 && s3*s4 < 0) return {true,0.0};
    if (s1 != 0 || s2 != 0 || s3 != 0 || s4 != 0) return {};

    const Real dx = static_cast<Real>(b.x)-a.x;
    const Real dy = static_cast<Real>(b.y)-a.y;
    const Real length = std::hypot(dx,dy);
    if (length == 0) return {};
    const auto projection = [&](Point p) {
        return ((static_cast<Real>(p.x)-a.x)*dx+
                (static_cast<Real>(p.y)-a.y)*dy)/length;
    };
    const Real t0 = projection(c), t1 = projection(d);
    const Real overlap = std::min(length,std::max(t0,t1))-
                         std::max(0.0L,std::min(t0,t1));
    if (overlap <= kEpsilon * std::max(1.0L,length)) return {};
    return {false,static_cast<double>(overlap)};
}

RouteStatus audit_unchecked(const Routes& routes, PolylineInteractions& metrics,
                            std::size_t& checks, std::size_t max_checks) {
    metrics = {};
    for (std::size_t i = 0; i < routes.edges.size(); ++i) {
        const auto& a = routes.edges[i].points;
        for (std::size_t j = i+1; j < routes.edges.size(); ++j) {
            const auto& b = routes.edges[j].points;
            for (std::size_t x = 1; x < a.size(); ++x)
                for (std::size_t y = 1; y < b.size(); ++y) {
                    if (checks >= max_checks)
                        return error(RouteError::CapacityExceeded,"polyline segment-pair audit budget exceeded",j);
                    ++checks;
                    const auto result = compare_segments(a[x-1],a[x],b[y-1],b[y]);
                    if (result.crossing) ++metrics.proper_crossings;
                    metrics.shared_length += result.overlap;
                    if (!std::isfinite(metrics.shared_length))
                        return error(RouteError::CapacityExceeded,"polyline shared-length sum overflow",j);
                }
        }
    }
    return {};
}

bool improves(const PolylineInteractions& next, const PolylineInteractions& old) {
    if (next.proper_crossings < old.proper_crossings) return true;
    if (next.proper_crossings > old.proper_crossings) return false;
    // Deterministic floating-point guard avoids accepting tiny cancellation
    // differences that might not survive cross-compiler iteration ordering.
    const double tol = 1e-8 * std::max(1.0,old.shared_length);
    return next.shared_length < old.shared_length - tol;
}

} // namespace

RouteStatus audit_polyline_interactions(
    const Layout& layout, const std::vector<RouteRequest>& requests,
    const Routes& paths, PolylineInteractions& out,
    const PolylineNudgingOptions& options) {
    out = {};
    if (!valid(options)) return error(RouteError::InvalidOptions,"invalid polyline nudging options");
    if (requests.size() > options.max_routes)
        return error(RouteError::CapacityExceeded,"polyline batch exceeds nudging capacity");
    if (!bounds_ok(paths,options))
        return error(RouteError::CapacityExceeded,"polyline waypoint audit budget exceeded");
    const auto status = validate_polyline_routes(layout,requests,paths,options.routing);
    if (!status) return status;
    std::size_t checks=0;
    PolylineInteractions candidate;
    const auto audit = audit_unchecked(paths,candidate,checks,options.max_segment_pair_checks);
    if (!audit) return audit;
    out=candidate;
    return {};
}

RouteStatus route_polyline_nudged(
    const Layout& layout, const std::vector<RouteRequest>& requests,
    Routes& out, const PolylineNudgingOptions& options,
    PolylineNudgingReport* report) {
    out = {};
    if (report) *report = {};
    if (!valid(options)) return error(RouteError::InvalidOptions,"invalid polyline nudging options");
    if (requests.size() > options.max_routes)
        return error(RouteError::CapacityExceeded,"polyline batch exceeds nudging capacity");
    Routes current;
    auto status = route_polyline(layout,requests,current,options.routing);
    if (!status) return status;
    if (!bounds_ok(current,options))
        return error(RouteError::CapacityExceeded,"polyline baseline waypoint budget exceeded");
    PolylineNudgingReport candidate;
    std::size_t checks=0;
    status = audit_unchecked(current,candidate.initial,checks,options.max_segment_pair_checks);
    if (!status) return status;
    candidate.final=candidate.initial;

    if (requests.size() > 1) {
        for (std::size_t pass=0; pass < options.max_passes; ++pass) {
            bool any = false;
            for (std::size_t route_index=0; route_index < requests.size(); ++route_index) {
                const auto original = current.edges[route_index];
                bool accepted=false;
                // Do not propose changes on the terminal stub segments.
                for (std::size_t segment=1;
                     segment+2 < original.points.size() && !accepted; ++segment) {
                    const Point a=original.points[segment],b=original.points[segment+1];
                    const double dx=b.x-a.x, dy=b.y-a.y;
                    const double length=std::hypot(dx,dy);
                    if (!std::isfinite(length) || length<=0) continue;
                    for (std::size_t lane=1;lane<=options.max_lanes && !accepted;++lane)
                        for (int sign : {1,-1}) {
                            if (candidate.candidate_trials >= options.max_candidates)
                                return error(RouteError::CapacityExceeded,"polyline nudging candidate budget exceeded",route_index);
                            ++candidate.candidate_trials;
                            const double distance=options.lane_spacing*static_cast<double>(lane);
                            const Point waypoint{a.x+(b.x-a.x)*0.5+sign*(-dy/length)*distance,
                                                 a.y+(b.y-a.y)*0.5+sign*(dx/length)*distance};
                            if (!std::isfinite(waypoint.x) || !std::isfinite(waypoint.y))
                                continue;
                            PolylineCheckpointRequest request{requests[route_index],{waypoint}};
                            Routes alternative;
                            status = route_polyline_checkpoints(layout,{request},alternative,options.routing);
                            if (!status) {
                                if (status.error == RouteError::InvalidInput ||
                                    status.error == RouteError::NoPath) continue;
                                status.route_index=route_index;
                                return status;
                            }
                            if (alternative.edges.size() != 1) return error(
                                RouteError::InternalInvariant,"polyline trial route count invariant",route_index);
                            Routes trial=current;
                            trial.edges[route_index]=std::move(alternative.edges[0]);
                            if (!bounds_ok(trial,options)) return error(
                                RouteError::CapacityExceeded,"polyline nudging waypoint budget exceeded",route_index);
                            // Avoid trusting our own search's geometry checks:
                            // validate every candidate with the public checker.
                            status=validate_polyline_routes(layout,requests,trial,options.routing);
                            if (!status) {
                                status.route_index=route_index;
                                return status;
                            }
                            PolylineInteractions metrics;
                            status=audit_unchecked(trial,metrics,checks,options.max_segment_pair_checks);
                            if (!status) return status;
                            if (improves(metrics,candidate.final)) {
                                current=std::move(trial);
                                candidate.final=metrics;
                                ++candidate.accepted_moves;
                                accepted=true;
                                any=true;
                                break;
                            }
                        }
                }
            }
            if (!any) break;
        }
    }
    candidate.segment_pair_checks=checks;
    for (std::size_t i=0;i<current.edges.size();++i) {
        // Count changed routes relative to the pristine Polyline A* baseline.
        Routes initial_one;
        status=route_polyline(layout,{requests[i]},initial_one,options.routing);
        if (!status) {
            status.route_index=i;
            return status;
        }
        const auto& base=initial_one.edges[0].points;
        const auto& now=current.edges[i].points;
        if (base.size()!=now.size() || !std::equal(base.begin(),base.end(),now.begin(),
            [](Point x, Point y) { return x.x==y.x && x.y==y.y; }))
            ++candidate.changed_routes;
    }
    status=validate_polyline_routes(layout,requests,current,options.routing);
    if (!status) return status;
    out=std::move(current);
    if (report) *report=candidate;
    return {};
}

} // namespace stun::graphlayout
