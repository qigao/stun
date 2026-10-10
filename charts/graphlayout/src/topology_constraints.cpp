#include "stun/graphlayout/topology_constraints.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace stun::graphlayout {
namespace {

using Real = long double;

TopologyGuardStatus fail(TopologyGuardError error, const char* message) {
    return {error, message};
}

bool bounded(double x, const TopologyGuardOptions& o) {
    return std::isfinite(x) && std::abs(x) <= o.coordinate_limit;
}

bool valid_options(const TopologyGuardOptions& o) {
    return std::isfinite(o.coordinate_limit) && o.coordinate_limit > 0.0 &&
           o.coordinate_limit <= 1e12 &&
           std::isfinite(o.node_segment_clearance) &&
           o.node_segment_clearance >= 0.0 &&
           o.node_segment_clearance <= o.coordinate_limit &&
           std::isfinite(o.min_bend_area_ratio) &&
           o.min_bend_area_ratio >= 0.0 && o.min_bend_area_ratio <= 0.1 &&
           std::isfinite(o.event_backoff) &&
           o.event_backoff >= 0.0 && o.event_backoff <= 0.1 &&
           o.max_constraints > 0 && o.max_evaluations > 0 &&
           o.max_nodes > 0 && o.max_routes > 0 && o.max_route_points >= 2;
}

Real value(const TopologyScalarTrack& t, Real alpha) {
    return static_cast<Real>(t.initial) +
           alpha * (static_cast<Real>(t.desired) - t.initial);
}

bool valid(const TopologyScalarTrack& t, const TopologyGuardOptions& o) {
    return bounded(t.initial, o) && bounded(t.desired, o);
}

bool valid(const TopologyPointTrack& t, const TopologyGuardOptions& o) {
    return valid(t.x, o) && valid(t.y, o);
}

Real cross(Real ax, Real ay, Real bx, Real by) {
    return ax * by - ay * bx;
}

Real area(const TopologyBendConstraint& t, Real a) {
    return cross(value(t.bend.x,a)-value(t.first.x,a),
                 value(t.bend.y,a)-value(t.first.y,a),
                 value(t.last.x,a)-value(t.bend.x,a),
                 value(t.last.y,a)-value(t.bend.y,a));
}

// For an analytic boundary event, do not return the exact zero-slack instant:
// boundary tangency is ambiguous to the topology analyzer.
double earlier(Real first, const TopologyGuardOptions& o) {
    if (first <= 0.0L) return 0.0;
    if (first >= 1.0L) first = 1.0L;
    const Real candidate = first * (1.0L - static_cast<Real>(o.event_backoff));
    const double rounded = static_cast<double>(candidate);
    return std::max(0.0, std::nextafter(rounded, 0.0));
}

TopologyGuardStatus tri_bound(const TopologyTriConstraint& c, double& out,
                              const TopologyGuardOptions& opt) {
    if (!valid(c.u,opt) || !valid(c.v,opt) || !valid(c.w,opt) ||
        !std::isfinite(c.p) || c.p < 0.0 || c.p > 1.0 ||
        !std::isfinite(c.gap) || c.gap < 0.0 || c.gap > opt.coordinate_limit)
        return fail(TopologyGuardError::InvalidInput,
                    "invalid triangle separation track, interpolation ratio, or gap");
    const Real p=c.p;
    const Real sign=c.below ? 1.0L : -1.0L;
    const auto slack=[&](Real alpha) {
        return sign * (value(c.u,alpha)*(1.0L-p) +
                       value(c.v,alpha)*p - value(c.w,alpha)) - c.gap;
    };
    const Real s0=slack(0.0L), s1=slack(1.0L);
    if (!std::isfinite(s0) || !std::isfinite(s1))
        return fail(TopologyGuardError::InvalidGeometry,"nonfinite triangle separation slack");
    // Reject a baseline that is not feasible. A signed initial slack of zero
    // with decreasing slack has no positive safe movement.
    if (s0 < 0.0L)
        return fail(TopologyGuardError::InfeasibleBaseline,
                    "baseline violates a declared triangle separation");
    out = (s1 > 0.0L) ? 1.0 :
          ((s0 == s1) ? 0.0 : earlier(s0 / (s0-s1), opt));
    return {};
}

TopologyGuardStatus bend_bound(const TopologyBendConstraint& c, double& out,
                               const TopologyGuardOptions& opt) {
    if (!valid(c.first,opt) || !valid(c.bend,opt) || !valid(c.last,opt))
        return fail(TopologyGuardError::InvalidInput,"invalid bend point motion track");
    const Real original=area(c,0.0L);
    if (!std::isfinite(original))
        return fail(TopologyGuardError::InvalidGeometry,"nonfinite initial bend area");
    if (original == 0.0L) {out=1.0;return {};}
    const Real sign=original>0 ? 1.0L : -1.0L;
    const Real floor=std::abs(original)*opt.min_bend_area_ratio;
    // Reconstruct the signed-area quadratic from f(0), f(1), f(-1).
    const Real f0=sign*original-floor;
    const Real f1=sign*area(c,1.0L)-floor;
    const Real fm=sign*area(c,-1.0L)-floor;
    if (!std::isfinite(f0) || !std::isfinite(f1) || !std::isfinite(fm))
        return fail(TopologyGuardError::InvalidGeometry,"nonfinite bend deformation area");
    const Real quadratic=(f1+fm-2.0L*f0)/2.0L;
    const Real linear=(f1-fm)/2.0L;
    const Real constant=f0;
    if (constant <= 0.0L)
        return fail(TopologyGuardError::InfeasibleBaseline,"degenerate signed bend at baseline");
    Real root=2.0L;
    if (quadratic == 0.0L) {
        if (linear < 0.0L) root = -constant/linear;
    } else {
        const Real disc=linear*linear-4.0L*quadratic*constant;
        if (disc >= 0.0L) {
            const Real square=std::sqrt(disc);
            const Real denom=-linear+std::copysign(square, -linear);
            if (denom != 0.0L) {
                const Real r1=denom/(2.0L*quadratic);
                const Real r2=(2.0L*constant)/denom;
                if (r1 >= 0.0L)root=std::min(root,r1);
                if (r2 >= 0.0L)root=std::min(root,r2);
            }
        }
    }
    out=(root <= 1.0L) ? earlier(root,opt) : 1.0;
    return {};
}

TopologyScalarTrack make_track(double initial, Real motion,
                               const TopologyGuardOptions& o,
                               bool& valid_motion) {
    const Real destination=static_cast<Real>(initial)+motion;
    if (!std::isfinite(destination) ||
        std::abs(destination)>o.coordinate_limit) valid_motion=false;
    return {initial,static_cast<double>(destination)};
}

TopologyPointTrack make_point_track(Point original, Real dx, Real dy,
                                    const TopologyGuardOptions& o, bool& valid_motion) {
    return {make_track(original.x,dx,o,valid_motion),
            make_track(original.y,dy,o,valid_motion)};
}

// The path's k-th point follows exactly the same source/target arclength
// interpolation strategy as the existing Topology interpolator.
TopologyGuardStatus moving_route_points(const Edge& edge, const Route& route,
                                         const Layout& start, const Layout& desired,
                                         const TopologyGuardOptions& opt,
                                         std::vector<TopologyPointTrack>& out) {
    if (route.points.size() < 2 || route.points.size() > opt.max_route_points)
        return fail(TopologyGuardError::CapacityExceeded,"route point budget exceeded");
    const auto& s=start.nodes[edge.source];const auto& t=start.nodes[edge.target];
    const auto& ds=desired.nodes[edge.source];const auto& dt=desired.nodes[edge.target];
    const Real dxs=static_cast<Real>(ds.x)-s.x;
    const Real dys=static_cast<Real>(ds.y)-s.y;
    const Real dxt=static_cast<Real>(dt.x)-t.x;
    const Real dyt=static_cast<Real>(dt.y)-t.y;
    std::vector<Real> lengths(route.points.size(),0.0L);
    for(std::size_t j=1;j<route.points.size();++j){
        const auto& a=route.points[j-1];const auto& b=route.points[j];
        if(!bounded(a.x,opt)||!bounded(a.y,opt)||!bounded(b.x,opt)||!bounded(b.y,opt))
            return fail(TopologyGuardError::InvalidGeometry,"unbounded route point coordinates");
        const Real dx=static_cast<Real>(b.x)-a.x, dy=static_cast<Real>(b.y)-a.y;
        const Real d=std::sqrt(dx*dx+dy*dy);
        if(!(d>0.0L)||!std::isfinite(d))
            return fail(TopologyGuardError::InvalidGeometry,"degenerate route arclength");
        lengths[j]=lengths[j-1]+d;
    }
    const Real total=lengths.back();
    if(!(total>0.0L))return fail(TopologyGuardError::InvalidGeometry,"zero route total length");
    bool ok=true;
    out.clear();out.reserve(route.points.size());
    for(std::size_t j=0;j<route.points.size();++j){
        const Real w=lengths[j]/total;
        const Real dx=(1.0L-w)*dxs+w*dxt;
        const Real dy=(1.0L-w)*dys+w*dyt;
        out.push_back(make_point_track(route.points[j],dx,dy,opt,ok));
    }
    if(!ok)return fail(TopologyGuardError::InvalidGeometry,"route motion exceeds coordinate limit");
    return {};
}

} // namespace

TopologyGuardStatus limit_topology_tri(const TopologyTriConstraint& constraint,
                                       double& max_safe_fraction,
                                       const TopologyGuardOptions& options) {
    if(!valid_options(options))return fail(TopologyGuardError::InvalidOptions,"invalid topology guard budgets");
    double value_out=0;
    const auto status=tri_bound(constraint,value_out,options);
    if(status)max_safe_fraction=value_out;
    return status;
}

TopologyGuardStatus limit_topology_straight(const TopologyStraightConstraint& constraint,
                                            double& max_safe_fraction,
                                            const TopologyGuardOptions& options) {
    return limit_topology_tri(constraint.separation, max_safe_fraction, options);
}

TopologyGuardStatus limit_topology_bend(const TopologyBendConstraint& constraint,
                                        double& max_safe_fraction,
                                        const TopologyGuardOptions& options) {
    if(!valid_options(options))return fail(TopologyGuardError::InvalidOptions,"invalid topology guard budgets");
    double value_out=0;
    const auto status=bend_bound(constraint,value_out,options);
    if(status)max_safe_fraction=value_out;
    return status;
}

TopologyGuardStatus limit_topology_movement(
    const Graph& graph, const Layout& baseline, const Layout& desired,
    const Routes& routes, TopologyGuardReport& report,
    const TopologyGuardOptions& options) {
    report={};
    if(!valid_options(options))return fail(TopologyGuardError::InvalidOptions,"invalid topology guard budgets");
    if(graph.nodes.size()!=baseline.nodes.size() ||
       graph.nodes.size()!=desired.nodes.size() ||
       graph.edges.size()!=routes.edges.size())
        return fail(TopologyGuardError::InvalidInput,"graph/node/route cardinalities differ");
    if(graph.nodes.size()>options.max_nodes || graph.edges.size()>options.max_routes)
        return fail(TopologyGuardError::CapacityExceeded,"topology guard node/route budget exceeded");
    for(std::size_t i=0;i<graph.nodes.size();++i){
        const auto& n=graph.nodes[i];const auto& a=baseline.nodes[i];const auto& b=desired.nodes[i];
        if(n.id.empty() || a.width!=n.width || b.width!=n.width ||
           a.height!=n.height || b.height!=n.height ||
           !bounded(a.x,options) || !bounded(a.y,options) ||
           !bounded(b.x,options) || !bounded(b.y,options) ||
           !bounded(a.x+a.width,options) || !bounded(a.y+a.height,options) ||
           !bounded(b.x+b.width,options) || !bounded(b.y+b.height,options))
            return fail(TopologyGuardError::InvalidInput,"invalid topology guard node geometry");
    }
    TopologyGuardReport candidate;
    std::vector<std::vector<TopologyPointTrack>> route_tracks;
    route_tracks.reserve(graph.edges.size());
    std::size_t count=0;
    auto use=[&]() -> bool {
        if(candidate.evaluations==options.max_evaluations ||
           count==options.max_constraints) return false;
        ++candidate.evaluations;++count;return true;
    };
    for(std::size_t edge_idx=0;edge_idx<graph.edges.size();++edge_idx){
        const auto& edge=graph.edges[edge_idx];
        if(edge.source>=graph.nodes.size() || edge.target>=graph.nodes.size() ||
           edge.source==edge.target)
            return fail(TopologyGuardError::InvalidInput,"invalid topology edge endpoints");
        std::vector<TopologyPointTrack> moves;
        auto st=moving_route_points(edge,routes.edges[edge_idx],baseline,desired,options,moves);
        if(!st)return st;
        for(std::size_t segment=0;segment+1<moves.size();++segment){
            const auto& u=moves[segment];const auto& v=moves[segment+1];
            const Real dx=static_cast<Real>(v.x.initial)-u.x.initial;
            const Real dy=static_cast<Real>(v.y.initial)-u.y.initial;
            // Work in the axis transverse to the segment's dominant direction.
            const bool scan_y=std::abs(dy)>=std::abs(dx);
            const Real denom=scan_y?dy:dx;
            if(denom==0.0L)continue;
            for(std::size_t node=0;node<graph.nodes.size();++node){
                if(node==edge.source || node==edge.target)continue;
                if(candidate.evaluations==options.max_evaluations)
                    return fail(TopologyGuardError::CapacityExceeded,"segment/node scan budget exceeded");
                ++candidate.evaluations;
                const auto& box=baseline.nodes[node];const auto& moved=desired.nodes[node];
                const Real scan=scan_y ? static_cast<Real>(box.y)+box.height/2.0L :
                                         static_cast<Real>(box.x)+box.width/2.0L;
                const Real origin=scan_y ? u.y.initial : u.x.initial;
                const Real p=(scan-origin)/denom;
                if(p<=0.0L||p>=1.0L)continue;
                const Real line=(1.0L-p)*(scan_y?u.x.initial:u.y.initial)+
                                p*(scan_y?v.x.initial:v.y.initial);
                const Real mid=scan_y ? static_cast<Real>(box.x)+box.width/2.0L :
                                        static_cast<Real>(box.y)+box.height/2.0L;
                const bool above=mid<line;
                const Real boundary=scan_y ?
                    (above ? static_cast<Real>(box.x)+box.width : static_cast<Real>(box.x)) :
                    (above ? static_cast<Real>(box.y)+box.height : static_cast<Real>(box.y));
                if(std::abs(mid-line)<1e-12L)
                    return fail(TopologyGuardError::InfeasibleBaseline,"node and segment scan baseline is ambiguous");
                if(!use())return fail(TopologyGuardError::CapacityExceeded,"topology guard count budget exceeded");
                TopologyStraightConstraint guard;
                guard.node_index=node;
                guard.edge_index=edge_idx;
                guard.segment_index=segment;
                guard.separation.u=scan_y?u.x:u.y;
                guard.separation.v=scan_y?v.x:v.y;
                const Real bd=boundary+(scan_y?static_cast<Real>(moved.x)-box.x:
                                               static_cast<Real>(moved.y)-box.y);
                guard.separation.w={static_cast<double>(boundary),static_cast<double>(bd)};
                guard.separation.p=static_cast<double>(p);
                guard.separation.gap=options.node_segment_clearance;
                guard.separation.below=above;
                double safe=0;
                st=tri_bound(guard.separation,safe,options);
                if(!st)return st;
                candidate.max_safe_fraction=std::min(candidate.max_safe_fraction,safe);
                ++candidate.straight_constraints;
            }
        }
        for(std::size_t i=1;i+1<moves.size();++i){
            if(!use())return fail(TopologyGuardError::CapacityExceeded,"topology bend count budget exceeded");
            TopologyBendConstraint bend{moves[i-1],moves[i],moves[i+1]};
            const Real baseline_area=area(bend,0.0L);
            if(baseline_area==0.0L)continue;
            double safe=0;
            st=bend_bound(bend,safe,options);
            if(!st)return st;
            candidate.max_safe_fraction=std::min(candidate.max_safe_fraction,safe);
            ++candidate.bend_constraints;
        }
        route_tracks.push_back(std::move(moves));
    }
    // For independent segments AB and CD the four orientation predicates
    // orient(A,B,C), orient(A,B,D), orient(C,D,A), orient(C,D,B) are quadratic
    // in affine displacement. A proper crossing can only appear/disappear when
    // one of these orientations reaches zero, so each nonzero baseline
    // orientation provides an analytically bounded event. Collinear but
    // disjoint baselines remain outside this guard and require full auditing.
    for(std::size_t i=0;i<route_tracks.size();++i){
        const auto& a=route_tracks[i];
        for(std::size_t j=i+1;j<route_tracks.size();++j){
            const auto& b=route_tracks[j];
            for(std::size_t s=0;s+1<a.size();++s){
                for(std::size_t t=0;t+1<b.size();++t){
                    const TopologyBendConstraint guards[4]={
                        {a[s],a[s+1],b[t]},
                        {a[s],a[s+1],b[t+1]},
                        {b[t],b[t+1],a[s]},
                        {b[t],b[t+1],a[s+1]},
                    };
                    for(const auto& guard:guards){
                        if(!use())return fail(TopologyGuardError::CapacityExceeded,
                                              "topology segment-pair event budget exceeded");
                        const Real initial_area=area(guard,0.0L);
                        if(initial_area==0.0L)continue;
                        double safe=0;
                        const auto status=bend_bound(guard,safe,options);
                        if(!status)return status;
                        candidate.max_safe_fraction=std::min(candidate.max_safe_fraction,safe);
                        ++candidate.segment_pair_constraints;
                    }
                }
            }
        }
    }
    report=candidate;
    return {};
}

} // namespace stun::graphlayout
