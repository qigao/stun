#include "stun/graphlayout/topology_constraints.h"
#include "topology_node_motion.h"

#include <algorithm>
#include <array>
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

struct PolynomialRoots {
    std::array<Real,2> values{};
    std::size_t count=0;
};

PolynomialRoots polynomial_roots(Real quadratic, Real linear, Real constant) {
    PolynomialRoots roots;
    if(quadratic==0.0L) {
        if(linear!=0.0L)roots.values[roots.count++]=-constant/linear;
    } else {
        const Real disc=linear*linear-4.0L*quadratic*constant;
        if(disc==0.0L)roots.values[roots.count++]=-linear/(2.0L*quadratic);
        else if(disc>0.0L) {
            const Real denom=-linear+std::copysign(std::sqrt(disc),-linear);
            roots.values[roots.count++]=denom/(2.0L*quadratic);
            roots.values[roots.count++]=(2.0L*constant)/denom;
        }
    }
    return roots;
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

// Intersect the open affine overlap intervals on both axes. The first
// intersection bounds continuous rectangle motion, including isolated nodes.
// O(1) time/space per pair; mere boundary contact remains admissible.
TopologyGuardStatus node_pair_bound(const PlacedNode& a, const PlacedNode& b,
                                    const PlacedNode& da, const PlacedNode& db,
                                    double& out, const TopologyGuardOptions& opt) {
    const std::array initial={
        static_cast<Real>(b.x)+b.width-a.x,
        static_cast<Real>(a.x)+a.width-b.x,
        static_cast<Real>(b.y)+b.height-a.y,
        static_cast<Real>(a.y)+a.height-b.y,
    };
    const std::array desired={
        static_cast<Real>(db.x)+db.width-da.x,
        static_cast<Real>(da.x)+da.width-db.x,
        static_cast<Real>(db.y)+db.height-da.y,
        static_cast<Real>(da.y)+da.height-db.y,
    };
    bool baseline_overlap=true;
    for (const auto slack:initial) baseline_overlap=baseline_overlap && slack>0.0L;
    if (baseline_overlap)
        return fail(TopologyGuardError::InfeasibleBaseline,"overlapping baseline node rectangles");
    Real lo=0.0L, hi=1.0L;
    for (std::size_t axis=0;axis<initial.size();++axis) {
        const Real delta=desired[axis]-initial[axis];
        if (delta==0.0L) {
            if (initial[axis]<=0.0L) {out=1.0;return {};}
        } else {
            const Real event=-initial[axis]/delta;
            if (delta>0.0L) lo=std::max(lo,event);
            else hi=std::min(hi,event);
        }
    }
    out=lo<hi ? earlier(lo,opt) : 1.0;
    return {};
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
    const auto roots=polynomial_roots(quadratic,linear,constant);
    for(std::size_t i=0;i<roots.count;++i)
        if(roots.values[i]>=0.0L)root=std::min(root,roots.values[i]);
    out=(root <= 1.0L) ? earlier(root,opt) : 1.0;
    return {};
}

// For a baseline-collinear disjoint pair, keep its projected intervals
// strictly separated on the first segment's dominant axis. All four endpoint
// gaps are affine; their first zero bounds contact even if endpoint order
// changes. This O(1) guard is conservative when transverse motion later
// separates the segments. Touching/shared baseline anchors need full auditing.
bool collinear_pair_bound(const TopologyPointTrack& a, const TopologyPointTrack& b,
                           const TopologyPointTrack& c, const TopologyPointTrack& d,
                           double& out, const TopologyGuardOptions& opt) {
    const bool scan_x=std::abs(static_cast<Real>(b.x.initial)-a.x.initial)>=
                      std::abs(static_cast<Real>(b.y.initial)-a.y.initial);
    std::array<TopologyScalarTrack,2> left={scan_x?a.x:a.y,scan_x?b.x:b.y};
    std::array<TopologyScalarTrack,2> right={scan_x?c.x:c.y,scan_x?d.x:d.y};
    const auto lower=[](const auto& pair) {return std::min(pair[0].initial,pair[1].initial);};
    const auto upper=[](const auto& pair) {return std::max(pair[0].initial,pair[1].initial);};
    if(upper(right)<lower(left))std::swap(left,right);
    else if(!(upper(left)<lower(right)))return false;
    out=1.0;
    for(const auto& l:left)for(const auto& r:right) {
        const Real initial=static_cast<Real>(r.initial)-l.initial;
        const Real desired=static_cast<Real>(r.desired)-l.desired;
        if(desired<=0.0L)
            out=std::min(out,earlier(initial/(initial-desired),opt));
    }
    return true;
}

struct RelativePointMotion {
    Real x, y, dx, dy;
};

RelativePointMotion relative_motion(const TopologyPointTrack& p,
                                    const PlacedNode& box, const PlacedNode& moved) {
    return {static_cast<Real>(p.x.initial)-box.x,static_cast<Real>(p.y.initial)-box.y,
            (static_cast<Real>(p.x.desired)-p.x.initial)-(static_cast<Real>(moved.x)-box.x),
            (static_cast<Real>(p.y.desired)-p.y.initial)-(static_cast<Real>(moved.y)-box.y)};
}

struct SegmentNodeMotion {
    std::array<RelativePointMotion,2> endpoints;
    Real width, height;

    bool penetrates(Real alpha) const {
        const auto& a=endpoints[0];const auto& b=endpoints[1];
        const Real ax=a.x+alpha*a.dx, ay=a.y+alpha*a.dy;
        const Real bx=b.x+alpha*b.dx, by=b.y+alpha*b.dy;
        if(std::max(ax,bx)<=0.0L || std::min(ax,bx)>=width ||
           std::max(ay,by)<=0.0L || std::min(ay,by)>=height)return false;
        const Real dx=bx-ax, dy=by-ay;
        if(dx==0.0L && dy==0.0L)return true;
        // Rectangle axes and the segment normal are the separating axes.
        // Strict opposite corner signs mean the supporting line cuts the
        // open rectangle; boundary-only tangencies remain admissible.
        bool negative=false, positive=false;
        for(const Real x:{0.0L,width})for(const Real y:{0.0L,height}) {
            const Real side=cross(dx,dy,x-ax,y-ay);
            negative=negative || side<0.0L;positive=positive || side>0.0L;
        }
        return negative && positive;
    }

    bool swept_axis_separation() const {
        const auto& a=endpoints[0];const auto& b=endpoints[1];
        return std::max({a.x,b.x,a.x+a.dx,b.x+b.dx})<=0.0L ||
               std::min({a.x,b.x,a.x+a.dx,b.x+b.dx})>=width ||
               std::max({a.y,b.y,a.y+a.dy,b.y+b.dy})<=0.0L ||
               std::min({a.y,b.y,a.y+a.dy,b.y+b.dy})>=height;
    }
};

constexpr std::size_t endpoint_face_events=8, rectangle_corners=4;
constexpr const char* segment_node_root_budget_message="segment/node root budget exceeded";
struct SegmentNodeEvents {
    // Eight affine endpoint/face roots, two roots per corner orientation,
    // and the interval endpoints. No allocation or unbounded event storage.
    std::array<Real,2+endpoint_face_events+2*rectangle_corners> values{0.0L,1.0L};
    std::size_t count=2;
    void add(Real quadratic, Real linear, Real constant) {
        const auto roots=polynomial_roots(quadratic,linear,constant);
        for(std::size_t i=0;i<roots.count;++i)
            if(roots.values[i]>=0.0L && roots.values[i]<=1.0L)values[count++]=roots.values[i];
    }
};

// Between endpoint/face and corner/line events all separating-axis signs are
// fixed. Inspect each open event interval to bound the first penetration.
// O(1) time/space per pair; work includes every polynomial and interval test.
TopologyGuardStatus segment_node_bound(const SegmentNodeMotion& motion, double& out,
                                       std::size_t& evaluations, const TopologyGuardOptions& opt) {
    out=1.0;
    if(motion.swept_axis_separation())return {};
    if(motion.penetrates(0.0L))
        return fail(TopologyGuardError::InfeasibleBaseline,"baseline segment penetrates a node rectangle");
    const auto& a=motion.endpoints[0];const auto& b=motion.endpoints[1];
    if(a.dx==0.0L && a.dy==0.0L && b.dx==0.0L && b.dy==0.0L)return {};
    auto use_evaluation=[&]() {
        if(evaluations==opt.max_evaluations)return false;
        ++evaluations;return true;
    };
    SegmentNodeEvents events;
    for(const auto& p:motion.endpoints) {
        for(const Real x:{0.0L,motion.width}) {
            if(!use_evaluation())return fail(TopologyGuardError::CapacityExceeded,segment_node_root_budget_message);
            events.add(0.0L,p.dx,p.x-x);
        }
        for(const Real y:{0.0L,motion.height}) {
            if(!use_evaluation())return fail(TopologyGuardError::CapacityExceeded,segment_node_root_budget_message);
            events.add(0.0L,p.dy,p.y-y);
        }
    }
    const Real rx=b.x-a.x, ry=b.y-a.y, drx=b.dx-a.dx, dry=b.dy-a.dy;
    for(const Real x:{0.0L,motion.width})for(const Real y:{0.0L,motion.height}) {
        if(!use_evaluation())return fail(TopologyGuardError::CapacityExceeded,segment_node_root_budget_message);
        events.add(cross(drx,dry,-a.dx,-a.dy),
                   cross(drx,dry,x-a.x,y-a.y)+cross(rx,ry,-a.dx,-a.dy),
                   cross(rx,ry,x-a.x,y-a.y));
    }
    std::sort(events.values.begin(),events.values.begin()+events.count);
    const auto end=std::unique(events.values.begin(),events.values.begin()+events.count);
    events.count=static_cast<std::size_t>(end-events.values.begin());
    for(std::size_t i=1;i<events.count;++i) {
        if(!use_evaluation())return fail(TopologyGuardError::CapacityExceeded,"segment/node interval budget exceeded");
        const Real lo=events.values[i-1], hi=events.values[i];
        if(motion.penetrates(lo+(hi-lo)/2.0L)) {out=earlier(lo,opt);return {};}
    }
    return {};
}

using Quadratic = std::array<Real,3>;
constexpr std::size_t order_degree=4;
using OrderPolynomial = std::array<Real,order_degree+1>;
constexpr std::size_t order_subdivision_depth=std::numeric_limits<double>::digits;
constexpr Real order_roundoff_factor=128.0L;

RelativePointMotion point_difference(const TopologyPointTrack& a, const TopologyPointTrack& b) {
    return {static_cast<Real>(a.x.initial)-b.x.initial,static_cast<Real>(a.y.initial)-b.y.initial,
            (static_cast<Real>(a.x.desired)-a.x.initial)-(static_cast<Real>(b.x.desired)-b.x.initial),
            (static_cast<Real>(a.y.desired)-a.y.initial)-(static_cast<Real>(b.y.desired)-b.y.initial)};
}

Quadratic cross_polynomial(const RelativePointMotion& a, const RelativePointMotion& b) {
    return {cross(a.x,a.y,b.x,b.y),cross(a.dx,a.dy,b.x,b.y)+cross(a.x,a.y,b.dx,b.dy),
            cross(a.dx,a.dy,b.dx,b.dy)};
}

struct CrossingTrack {
    std::size_t edge, segment;
    Quadratic numerator, denominator;
    Real initial_parameter() const {return numerator[0]/denominator[0];}
    bool valid() const {
        if(denominator[0]==0.0L)return false;
        const Real parameter=initial_parameter();
        return std::isfinite(parameter) && parameter>0.0L && parameter<1.0L;
    }
};

CrossingTrack crossing_track(std::size_t edge, std::size_t segment,
                             const TopologyPointTrack& a, const TopologyPointTrack& b,
                             const TopologyPointTrack& c, const TopologyPointTrack& d) {
    const auto direction=point_difference(d,c);
    return {edge,segment,cross_polynomial(point_difference(c,a),direction),
            cross_polynomial(point_difference(b,a),direction)};
}

OrderPolynomial bernstein_coefficients(const OrderPolynomial& power) {
    // b_i = sum_{k<=i} c_k * choose(i,k)/choose(4,k).
    constexpr Real weights[order_degree+1][order_degree+1]={
        {1,0,0,0,0},{1,1.0L/4,0,0,0},{1,1.0L/2,1.0L/6,0,0},
        {1,3.0L/4,1.0L/2,1.0L/4,0},{1,1,1,1,1}};
    OrderPolynomial result{};
    for(std::size_t i=0;i<result.size();++i)
        for(std::size_t k=0;k<=i;++k)result[i]+=power[k]*weights[i][k];
    return result;
}

// A positive Bernstein control hull bounds the entire interval, not sampled
// frames. Visit left intervals first; stop conservatively at the first interval
// unresolved at double-fraction precision. O(evaluations) time, O(depth) stack.
TopologyGuardStatus positive_order_prefix(const OrderPolynomial& controls, Real floor,
                                          Real lo, Real hi, std::size_t depth,
                                          std::size_t& evaluations, const TopologyGuardOptions& opt,
                                          Real& first) {
    if(evaluations==opt.max_evaluations)
        return fail(TopologyGuardError::CapacityExceeded,"crossing-order subdivision budget exceeded");
    ++evaluations;
    if(std::all_of(controls.begin(),controls.end(),[&](Real v){return v>floor;}))return {};
    const Real mid=lo+(hi-lo)/2.0L;
    if(depth==order_subdivision_depth || mid==lo || mid==hi) {first=lo;return {};}
    OrderPolynomial work=controls, left{}, right{};
    left.front()=work.front();right.back()=work.back();
    for(std::size_t level=1;level<=order_degree;++level) {
        for(std::size_t i=0;i<=order_degree-level;++i)work[i]=(work[i]+work[i+1])/2.0L;
        left[level]=work.front();right[order_degree-level]=work[order_degree-level];
    }
    auto st=positive_order_prefix(left,floor,lo,mid,depth+1,evaluations,opt,first);
    if(!st || first<=1.0L)return st;
    return positive_order_prefix(right,floor,mid,hi,depth+1,evaluations,opt,first);
}

TopologyGuardStatus crossing_order_bound(const CrossingTrack& left, const CrossingTrack& right,
                                         double& out, std::size_t& evaluations,
                                         const TopologyGuardOptions& opt) {
    OrderPolynomial order{};Real scale=0.0L;
    const Real sign=(left.denominator[0]>0)==(right.denominator[0]>0) ? 1.0L : -1.0L;
    // t_right - t_left = (N_right*D_left - N_left*D_right)/(D_left*D_right).
    // Existing proper-crossing orientation guards keep the denominator signs.
    for(std::size_t i=0;i<left.numerator.size();++i)
        for(std::size_t j=0;j<right.numerator.size();++j) {
            const Real a=right.numerator[i]*left.denominator[j];
            const Real b=left.numerator[i]*right.denominator[j];
            order[i+j]+=sign*(a-b);scale+=std::abs(a)+std::abs(b);
        }
    if(order[0]<=0.0L)
        return fail(TopologyGuardError::InfeasibleBaseline,"unseparated baseline crossing order");
    // Retain a machine-precision margin for coefficient cancellation and
    // repeated subdivision. This is a numerical guard, not interval arithmetic.
    const Real floor=order_roundoff_factor*std::numeric_limits<Real>::epsilon()*scale;
    Real first=2.0L;
    const auto st=positive_order_prefix(bernstein_coefficients(order),floor,0.0L,1.0L,0,
                                       evaluations,opt,first);
    if(!st)return st;
    out=first<=1.0L ? earlier(first,opt) : 1.0;
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

TopologyGuardStatus detail::limit_bend_node_motion(
    const std::array<TopologyPointTrack,3>& bend, const Layout& nodes,
    double& safe_fraction, TopologyNodeMotionWork& work,
    const TopologyGuardOptions& options) {
    if(!valid_options(options))return fail(TopologyGuardError::InvalidOptions,"invalid bend-node guard options");
    if(nodes.nodes.size()>options.max_nodes)
        return fail(TopologyGuardError::CapacityExceeded,"bend-node guard node budget exceeded");
    for(const auto& point:bend)
        if(!valid(point,options))return fail(TopologyGuardError::InvalidGeometry,"unbounded bend-node motion");
    double candidate=1.0;
    // Two affected segments, O(nodes) time and O(1) auxiliary storage.
    // Each pair consumes one constraint and the same bounded evaluations
    // as the continuous segment/node guard used for graph movement.
    for(std::size_t segment=0;segment+1<bend.size();++segment)
        for(const auto& box:nodes.nodes) {
            if(work.constraints==options.max_constraints || work.evaluations==options.max_evaluations)
                return fail(TopologyGuardError::CapacityExceeded,"bend-node guard count budget exceeded");
            ++work.constraints;++work.evaluations;
            const SegmentNodeMotion motion{{relative_motion(bend[segment],box,box),
                                             relative_motion(bend[segment+1],box,box)},box.width,box.height};
            double safe=1.0;
            const auto st=segment_node_bound(motion,safe,work.evaluations,options);
            if(!st)return st;
            candidate=std::min(candidate,safe);
        }
    safe_fraction=candidate;
    return {};
}

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
           !bounded(n.width,options) || !bounded(n.height,options) ||
           n.width<=0.0 || n.height<=0.0 ||
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
    for(std::size_t i=0;i<graph.nodes.size();++i)
        for(std::size_t j=i+1;j<graph.nodes.size();++j) {
            if(!use())return fail(TopologyGuardError::CapacityExceeded,
                                  "topology node-pair event budget exceeded");
            double safe=0.0;
            const auto st=node_pair_bound(baseline.nodes[i],baseline.nodes[j],
                                           desired.nodes[i],desired.nodes[j],safe,options);
            if(!st)return st;
            candidate.max_safe_fraction=std::min(candidate.max_safe_fraction,safe);
        }
    for(std::size_t edge_idx=0;edge_idx<graph.edges.size();++edge_idx){
        const auto& edge=graph.edges[edge_idx];
        if(edge.source>=graph.nodes.size() || edge.target>=graph.nodes.size() ||
           edge.source==edge.target)
            return fail(TopologyGuardError::InvalidInput,"invalid topology edge endpoints");
        std::vector<TopologyPointTrack> moves;
        auto st=moving_route_points(edge,routes.edges[edge_idx],baseline,desired,options,moves);
        if(!st)return st;
        // Internal waypoints can pass through a rectangle without the fixed
        // segment scan noticing. A point is a zero-extent rectangle in the
        // same continuous open-overlap calculation used for node pairs.
        // Check incident nodes too. Terminal anchors stay on their own node
        // boundaries; their collisions with other nodes have node-pair guards.
        // O(nodes * internal-waypoints) time, O(1) additional storage.
        for(std::size_t point=1;point+1<moves.size();++point) {
            const PlacedNode initial_point{moves[point].x.initial,moves[point].y.initial,0,0};
            const PlacedNode desired_point{moves[point].x.desired,moves[point].y.desired,0,0};
            for(std::size_t node=0;node<graph.nodes.size();++node) {
                if(!use())return fail(TopologyGuardError::CapacityExceeded,
                                      "topology waypoint-node event budget exceeded");
                double safe=1.0;
                st=node_pair_bound(baseline.nodes[node],initial_point,
                                   desired.nodes[node],desired_point,safe,options);
                if(st.error==TopologyGuardError::InfeasibleBaseline)
                    return fail(st.error,"baseline internal waypoint penetrates a node rectangle");
                if(!st)return st;
                candidate.max_safe_fraction=std::min(candidate.max_safe_fraction,safe);
            }
        }
        for(std::size_t segment=0;segment+1<moves.size();++segment){
            const auto& u=moves[segment];const auto& v=moves[segment+1];
            const Real dx=static_cast<Real>(v.x.initial)-u.x.initial;
            const Real dy=static_cast<Real>(v.y.initial)-u.y.initial;
            // Work in the axis transverse to the segment's dominant direction.
            const bool scan_y=std::abs(dy)>=std::abs(dx);
            const Real denom=scan_y?dy:dx;
            if(denom==0.0L)continue;
            for(std::size_t node=0;node<graph.nodes.size();++node){
                const auto& box=baseline.nodes[node];const auto& moved=desired.nodes[node];
                if(!use())return fail(TopologyGuardError::CapacityExceeded,"topology segment/node count budget exceeded");
                const SegmentNodeMotion motion{{relative_motion(u,box,moved),relative_motion(v,box,moved)},
                                                box.width,box.height};
                double segment_safe=1.0;
                st=segment_node_bound(motion,segment_safe,candidate.evaluations,options);
                if(!st)return st;
                candidate.max_safe_fraction=std::min(candidate.max_safe_fraction,segment_safe);
                if(node==edge.source || node==edge.target)continue;
                if(candidate.evaluations==options.max_evaluations)
                    return fail(TopologyGuardError::CapacityExceeded,"segment/node scan budget exceeded");
                ++candidate.evaluations;
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
    // For independent segments AB and CD (including nonadjacent segments
    // of one route) the four orientation predicates
    // orient(A,B,C), orient(A,B,D), orient(C,D,A), orient(C,D,B) are quadratic
    // in affine displacement. A proper crossing can only appear/disappear when
    // one of these orientations reaches zero, so each nonzero baseline
    // orientation provides an analytically bounded event. For an entirely
    // collinear disjoint baseline, affine projected gaps bound first contact.
    std::vector<CrossingTrack> crossings;
    for(std::size_t i=0;i<route_tracks.size();++i){
        const auto& a=route_tracks[i];
        for(std::size_t j=i;j<route_tracks.size();++j){
            const auto& b=route_tracks[j];
            for(std::size_t s=0;s+1<a.size();++s){
                // Adjacent segments share their moving waypoint and already
                // have a bend guard; each independent pair is examined once.
                const std::size_t first=(i==j) ? s+2 : 0;
                for(std::size_t t=first;t+1<b.size();++t){
                    const TopologyBendConstraint guards[4]={
                        {a[s],a[s+1],b[t]},
                        {a[s],a[s+1],b[t+1]},
                        {b[t],b[t+1],a[s]},
                        {b[t],b[t+1],a[s+1]},
                    };
                    bool collinear=true;
                    std::array<Real,4> initial_areas{};
                    for(std::size_t k=0;k<initial_areas.size();++k){
                        const auto& guard=guards[k];
                        if(!use())return fail(TopologyGuardError::CapacityExceeded,
                                              "topology segment-pair event budget exceeded");
                        const Real initial_area=area(guard,0.0L);
                        initial_areas[k]=initial_area;
                        if(initial_area==0.0L)continue;
                        collinear=false;
                        double safe=0;
                        const auto status=bend_bound(guard,safe,options);
                        if(!status)return status;
                        candidate.max_safe_fraction=std::min(candidate.max_safe_fraction,safe);
                        ++candidate.segment_pair_constraints;
                    }
                    const auto opposite=[](Real x, Real y) {
                        return (x<0.0L && y>0.0L) || (x>0.0L && y<0.0L);
                    };
                    if(i!=j && opposite(initial_areas[0],initial_areas[1]) &&
                       opposite(initial_areas[2],initial_areas[3])) {
                        // Two records per proper crossing are bounded by the
                        // four already-consumed segment-pair constraints.
                        const auto first_crossing=crossing_track(i,s,a[s],a[s+1],b[t],b[t+1]);
                        const auto second_crossing=crossing_track(j,t,b[t],b[t+1],a[s],a[s+1]);
                        if(!first_crossing.valid() || !second_crossing.valid())
                            return fail(TopologyGuardError::InvalidGeometry,"invalid baseline proper-crossing parameter");
                        crossings.push_back(first_crossing);crossings.push_back(second_crossing);
                    }
                    if(collinear) {
                        if(!use())return fail(TopologyGuardError::CapacityExceeded,
                                              "topology collinear-pair event budget exceeded");
                        double safe=1.0;
                        if(collinear_pair_bound(a[s],a[s+1],b[t],b[t+1],safe,options)) {
                            candidate.max_safe_fraction=std::min(candidate.max_safe_fraction,safe);
                            ++candidate.segment_pair_constraints;
                        }
                    }
                }
            }
        }
    }
    std::sort(crossings.begin(),crossings.end(),[](const auto& a,const auto& b) {
        if(a.edge!=b.edge)return a.edge<b.edge;
        if(a.segment!=b.segment)return a.segment<b.segment;
        return a.initial_parameter()<b.initial_parameter();
    });
    // Preserving adjacent order gaps preserves the entire sequence. Crossings
    // on different carrier segments cannot exchange without an endpoint event,
    // already bounded by the pair-orientation guards.
    for(std::size_t i=1;i<crossings.size();++i) {
        const auto& left=crossings[i-1];const auto& right=crossings[i];
        if(left.edge!=right.edge || left.segment!=right.segment)continue;
        if(!use())return fail(TopologyGuardError::CapacityExceeded,"crossing-order constraint budget exceeded");
        double safe=1.0;
        const auto st=crossing_order_bound(left,right,safe,candidate.evaluations,options);
        if(!st)return st;
        candidate.max_safe_fraction=std::min(candidate.max_safe_fraction,safe);
        ++candidate.segment_pair_constraints;
    }
    report=candidate;
    return {};
}

} // namespace stun::graphlayout
