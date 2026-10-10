#include "stun/graphlayout/topology.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace stun::graphlayout {
namespace {

using Real = long double;

TopologyStatus fail(TopologyError error, const std::string& message,
                    std::size_t edge = 0) {
    return {error, message, edge};
}

bool finite_bounded(double x, double limit) {
    return std::isfinite(x) && std::abs(x) <= limit;
}

bool valid_options(const TopologyOptions& o) {
    return std::isfinite(o.coordinate_limit) && o.coordinate_limit > 0.0 &&
           o.coordinate_limit <= 1e12 && std::isfinite(o.orientation_epsilon) &&
           o.orientation_epsilon >= 0.0 && o.orientation_epsilon <= 1e-6 &&
           o.max_nodes > 0 && o.max_edges > 0 &&
           o.max_points_per_route >= 2 && o.max_total_points > 0 &&
           o.max_segment_checks > 0 && o.max_trials > 0 &&
           o.max_trials <= 64 && o.samples_per_trial > 0 &&
           o.samples_per_trial <= 64;
}

bool valid_placement(const Graph& g, const Layout& l,
                     const TopologyOptions& o) {
    if (g.nodes.size() != l.nodes.size() || g.nodes.size() > o.max_nodes ||
        g.edges.size() > o.max_edges) return false;
    std::unordered_set<std::string> ids;
    ids.reserve(g.nodes.size());
    for (std::size_t i = 0; i < g.nodes.size(); ++i) {
        const auto& node = g.nodes[i];
        const auto& p = l.nodes[i];
        if (node.id.empty() || !ids.insert(node.id).second ||
            !finite_bounded(node.width, o.coordinate_limit) ||
            !finite_bounded(node.height, o.coordinate_limit) ||
            node.width <= 0 || node.height <= 0 ||
            p.width != node.width || p.height != node.height ||
            !finite_bounded(p.x, o.coordinate_limit) ||
            !finite_bounded(p.y, o.coordinate_limit) ||
            !finite_bounded(p.x + p.width, o.coordinate_limit) ||
            !finite_bounded(p.y + p.height, o.coordinate_limit)) return false;
    }
    for (const auto& edge : g.edges)
        if (edge.source >= g.nodes.size() || edge.target >= g.nodes.size() ||
            edge.source == edge.target) return false;
    return true;
}

struct Budget {
    std::size_t checks = 0;
    std::size_t limit = 0;
    bool use() {
        if (checks == limit) return false;
        ++checks;
        return true;
    }
};

bool box_overlap(const PlacedNode& a, const PlacedNode& b) {
    return a.x < b.x + b.width && b.x < a.x + a.width &&
           a.y < b.y + b.height && b.y < a.y + a.height;
}

Point port_anchor(const Layout& l, const Port& p) {
    const auto& node = l.nodes[p.node];
    switch (p.side) {
    case Side::North: return {node.x + node.width * p.offset, node.y};
    case Side::East: return {node.x + node.width, node.y + node.height * p.offset};
    case Side::South: return {node.x + node.width * p.offset, node.y + node.height};
    case Side::West: return {node.x, node.y + node.height * p.offset};
    default: return {std::numeric_limits<double>::quiet_NaN(), 0};
    }
}

bool same(Point a, Point b) { return a.x == b.x && a.y == b.y; }

// Route terminals must travel outward from the selected boundary side.
bool leaves_outward(const Port& port, Point anchor, Point next) {
    switch (port.side) {
    case Side::North: return next.y < anchor.y;
    case Side::East: return next.x > anchor.x;
    case Side::South: return next.y > anchor.y;
    case Side::West: return next.x < anchor.x;
    default: return false;
    }
}

Real cross(Point a, Point b, Point c) {
    return (static_cast<Real>(b.x) - a.x) *
               (static_cast<Real>(c.y) - a.y) -
           (static_cast<Real>(b.y) - a.y) *
               (static_cast<Real>(c.x) - a.x);
}

Real relative_orientation_scale(Point a, Point b, Point c) {
    const Real dx1 = static_cast<Real>(b.x) - a.x;
    const Real dy1 = static_cast<Real>(b.y) - a.y;
    const Real dx2 = static_cast<Real>(c.x) - a.x;
    const Real dy2 = static_cast<Real>(c.y) - a.y;
    return std::max(Real(1),
             std::max(std::abs(dx1), std::abs(dy1)) *
             std::max(std::abs(dx2), std::abs(dy2)));
}

int orientation(Point a, Point b, Point c, double eps) {
    const Real value = cross(a,b,c);
    const Real tol = static_cast<Real>(eps) * relative_orientation_scale(a,b,c);
    if (std::abs(value) <= tol) return 0;
    return value > 0 ? 1 : -1;
}

bool on_segment(Point a, Point b, Point c) {
    // c is assumed collinear with [a,b].
    return c.x >= std::min(a.x,b.x) && c.x <= std::max(a.x,b.x) &&
           c.y >= std::min(a.y,b.y) && c.y <= std::max(a.y,b.y);
}

// True only for a line passing through an OPEN rectangle interior; touching
// a boundary is permissible. Liang-Barsky interval geometry in long double.
bool penetrates(Point a, Point b, const PlacedNode& box) {
    Real lo = 0.0L, hi = 1.0L;
    const Real origin[2] = {a.x, a.y};
    const Real delta[2] = {static_cast<Real>(b.x) - a.x,
                            static_cast<Real>(b.y) - a.y};
    const Real lower[2] = {box.x, box.y};
    const Real upper[2] = {static_cast<Real>(box.x) + box.width,
                           static_cast<Real>(box.y) + box.height};
    for (std::size_t axis = 0; axis < 2; ++axis) {
        if (delta[axis] == 0.0L) {
            if (origin[axis] <= lower[axis] || origin[axis] >= upper[axis])
                return false;
            continue;
        }
        Real t1 = (lower[axis] - origin[axis]) / delta[axis];
        Real t2 = (upper[axis] - origin[axis]) / delta[axis];
        if (t1 > t2) std::swap(t1,t2);
        lo = std::max(lo,t1);
        hi = std::min(hi,t2);
        if (lo >= hi) return false;
    }
    return lo < hi;
}

struct Intersection {
    enum class Type { None, Proper, Ambiguous } type = Type::None;
    Real t = 0.0L, u = 0.0L;
    int sign = 0;
};

Intersection intersect(Point a, Point b, Point c, Point d, double eps) {
    const int o1=orientation(a,b,c,eps), o2=orientation(a,b,d,eps);
    const int o3=orientation(c,d,a,eps), o4=orientation(c,d,b,eps);
    if (o1*o2 < 0 && o3*o4 < 0) {
        const Real ux = static_cast<Real>(b.x)-a.x;
        const Real uy = static_cast<Real>(b.y)-a.y;
        const Real vx = static_cast<Real>(d.x)-c.x;
        const Real vy = static_cast<Real>(d.y)-c.y;
        const Real det=ux*vy - uy*vx;
        if (det == 0.0L) return {Intersection::Type::Ambiguous,0,0,0};
        const Real wx = static_cast<Real>(c.x)-a.x;
        const Real wy = static_cast<Real>(c.y)-a.y;
        const Real t=(wx*vy-wy*vx)/det;
        const Real u=(wx*uy-wy*ux)/det;
        if (t <= 0.0L || t >= 1.0L || u <= 0.0L || u >= 1.0L)
            return {Intersection::Type::Ambiguous,0,0,0};
        return {Intersection::Type::Proper,t,u,det>0 ? 1 : -1};
    }
    if ((o1 == 0 && on_segment(a,b,c)) ||
        (o2 == 0 && on_segment(a,b,d)) ||
        (o3 == 0 && on_segment(c,d,a)) ||
        (o4 == 0 && on_segment(c,d,b)))
        return {Intersection::Type::Ambiguous,0,0,0};
    return {};
}

struct Event { std::size_t other; int sign; Real distance; };

TopologyStatus analyze(const Graph& graph, const Layout& layout,
                       const std::vector<RouteRequest>& ports,
                       const Routes& routes, const TopologyOptions& options,
                       Budget& budget, TopologySignature& out) {
    out = {};
    if (!valid_placement(graph,layout,options))
        return fail(TopologyError::InvalidInput, "invalid graph IDs, endpoints or placement geometry");
    if (graph.nodes.size() > options.max_nodes ||
        graph.edges.size() > options.max_edges)
        return fail(TopologyError::CapacityExceeded,"topology graph budget exceeded");
    if (ports.size() != graph.edges.size() ||
        routes.edges.size() != graph.edges.size())
        return fail(TopologyError::InvalidInput, "graph, ports and routes differ in edge counts");

    for (std::size_t i=0;i<layout.nodes.size();++i)
        for (std::size_t j=i+1;j<layout.nodes.size();++j) {
            if (!budget.use()) return fail(TopologyError::CapacityExceeded,"topology node-pair budget exceeded");
            if (box_overlap(layout.nodes[i],layout.nodes[j]))
                return fail(TopologyError::InvalidGeometry,"overlapping node rectangles");
        }

    std::size_t points=0;
    std::vector<std::vector<Real>> distances(routes.edges.size());
    for (std::size_t i=0;i<routes.edges.size();++i) {
        const auto& request=ports[i];
        const auto& edge=graph.edges[i];
        if (request.source.node != edge.source || request.target.node != edge.target ||
            request.source.side == Side::Auto || request.target.side == Side::Auto ||
            !std::isfinite(request.source.offset) ||
            !std::isfinite(request.target.offset) ||
            request.source.offset < 0.0 || request.source.offset > 1.0 ||
            request.target.offset < 0.0 || request.target.offset > 1.0)
            return fail(TopologyError::InvalidInput,"topology requires explicit boundary-side ports",i);
        const auto& p=routes.edges[i].points;
        if (p.size() < 2 || p.size() > options.max_points_per_route ||
            p.size() > options.max_total_points ||
            points > options.max_total_points - p.size())
            return fail(TopologyError::CapacityExceeded,"topology route point budget exceeded",i);
        points += p.size();
        if (!same(p.front(),port_anchor(layout,request.source)) ||
            !same(p.back(),port_anchor(layout,request.target)))
            return fail(TopologyError::InvalidGeometry,"route endpoint does not match port anchor",i);
        if (!leaves_outward(request.source,p.front(),p[1]) ||
            !leaves_outward(request.target,p.back(),p[p.size()-2]))
            return fail(TopologyError::InvalidGeometry,"route terminal violates outward port side",i);
        distances[i].reserve(p.size());
        distances[i].push_back(0.0L);
        for(std::size_t s=1;s<p.size();++s){
            const auto& a=p[s-1];const auto& b=p[s];
            if (!finite_bounded(a.x,options.coordinate_limit) ||
                !finite_bounded(a.y,options.coordinate_limit) ||
                !finite_bounded(b.x,options.coordinate_limit) ||
                !finite_bounded(b.y,options.coordinate_limit) || same(a,b))
                return fail(TopologyError::InvalidGeometry,"nonfinite or degenerate connector segment",i);
            const Real dx=static_cast<Real>(b.x)-a.x;
            const Real dy=static_cast<Real>(b.y)-a.y;
            const Real length=std::sqrt(dx*dx+dy*dy);
            if (!(length>0) || !std::isfinite(length))
                return fail(TopologyError::InvalidGeometry,"bad connector segment length",i);
            distances[i].push_back(distances[i].back()+length);
            for(const auto& box:layout.nodes){
                if (!budget.use()) return fail(TopologyError::CapacityExceeded,"topology segment-obstacle budget exceeded",i);
                if (penetrates(a,b,box))
                    return fail(TopologyError::InvalidGeometry,"edge penetrates a node rectangle",i);
            }
        }
        for(std::size_t s=0;s+1<p.size();++s)
            for(std::size_t t=s+2;t+1<p.size();++t){
                if (!budget.use()) return fail(TopologyError::CapacityExceeded,"topology self-contact budget exceeded",i);
                const auto hit=intersect(p[s],p[s+1],p[t],p[t+1],options.orientation_epsilon);
                if(hit.type!=Intersection::Type::None)
                    return fail(TopologyError::AmbiguousTopology,"self-contacting edge polyline",i);
            }
    }

    std::vector<std::vector<Event>> events(routes.edges.size());
    for(std::size_t i=0;i<routes.edges.size();++i){
        const auto& a=routes.edges[i].points;
        for(std::size_t j=i+1;j<routes.edges.size();++j){
            const auto& b=routes.edges[j].points;
            for(std::size_t s=0;s+1<a.size();++s)
                for(std::size_t t=0;t+1<b.size();++t){
                    if(!budget.use())return fail(TopologyError::CapacityExceeded,"topology edge-pair budget exceeded",i);
                    const auto hit=intersect(a[s],a[s+1],b[t],b[t+1],options.orientation_epsilon);
                    if(hit.type==Intersection::Type::None) continue;
                    if(hit.type==Intersection::Type::Ambiguous){
                        // A shared graph endpoint anchor is an allowed common
                        // vertex. Repeated terminal stubs are NOT exempt.
                        bool common_vertex=false;
                        const auto& e=graph.edges[i];const auto& f=graph.edges[j];
                        for(auto ni:{e.source,e.target})for(auto nj:{f.source,f.target}){
                            if(ni != nj)continue;
                            Point pi=ni==e.source?a.front():a.back();
                            Point pj=nj==f.source?b.front():b.back();
                            if(!same(pi,pj))continue;
                            const bool seg_i=(ni==e.source?s==0:s+2==a.size());
                            const bool seg_j=(nj==f.source?t==0:t+2==b.size());
                            if(seg_i && seg_j){
                                // Shared only at the common anchor, with no
                                // collinear overlap of its incident segments.
                                const auto& other_i=ni==e.source?a[s+1]:a[s];
                                const auto& other_j=nj==f.source?b[t+1]:b[t];
                                if(orientation(pi,other_i,other_j,options.orientation_epsilon)!=0)
                                    common_vertex=true;
                            }
                        }
                        if(common_vertex)continue;
                        return fail(TopologyError::AmbiguousTopology,"edge contact or collinear overlap",i);
                    }
                    const Real di=distances[i][s]+(distances[i][s+1]-distances[i][s])*hit.t;
                    const Real dj=distances[j][t]+(distances[j][t+1]-distances[j][t])*hit.u;
                    events[i].push_back({j,hit.sign,di});
                    events[j].push_back({i,-hit.sign,dj});
                }
        }
    }
    TopologySignature signature;
    signature.ordered_crossings.resize(routes.edges.size());
    std::size_t total=0;
    for(std::size_t i=0;i<events.size();++i){
        auto& ev=events[i];
        std::sort(ev.begin(),ev.end(),[](const Event& a,const Event& b){
            if(a.distance!=b.distance)return a.distance<b.distance;
            if(a.other!=b.other)return a.other<b.other;
            return a.sign<b.sign;
        });
        for(std::size_t j=0;j<ev.size();++j){
            if(j && std::abs(ev[j].distance-ev[j-1].distance) <=
                static_cast<Real>(options.orientation_epsilon) *
                std::max(Real(1),distances[i].back()))
                return fail(TopologyError::AmbiguousTopology,"multiple edges intersect at one unseparated point",i);
            signature.ordered_crossings[i].push_back({ev[j].other,ev[j].sign});
        }
        total+=ev.size();
    }
    signature.crossing_count=total/2;
    out=std::move(signature);
    return {};
}

bool equal_signature(const TopologySignature& a,const TopologySignature& b){
    if(a.crossing_count!=b.crossing_count || a.ordered_crossings.size()!=b.ordered_crossings.size())return false;
    for(std::size_t i=0;i<a.ordered_crossings.size();++i){
        const auto& ai=a.ordered_crossings[i];const auto& bi=b.ordered_crossings[i];
        if(ai.size()!=bi.size())return false;
        for(std::size_t j=0;j<ai.size();++j)
            if(ai[j].other_edge!=bi[j].other_edge || ai[j].sign!=bi[j].sign)return false;
    }
    return true;
}

TopologyStatus interpolate(const Graph& g, const Layout& base,
                           const Routes& routes, const Layout& desired,
                           const std::vector<RouteRequest>& ports, double fraction, const TopologyOptions& o,
                           Layout& positions, Routes& paths){
    positions=base;
    paths=routes;
    for(std::size_t i=0;i<base.nodes.size();++i){
        const auto& a=base.nodes[i];const auto& b=desired.nodes[i];
        const Real nx=static_cast<Real>(a.x)+fraction*(static_cast<Real>(b.x)-a.x);
        const Real ny=static_cast<Real>(a.y)+fraction*(static_cast<Real>(b.y)-a.y);
        if(!std::isfinite(nx)||!std::isfinite(ny)||
           std::abs(nx)>o.coordinate_limit||std::abs(ny)>o.coordinate_limit)
            return fail(TopologyError::InvalidGeometry,"node interpolation exceeds coordinate limit");
        positions.nodes[i].x=static_cast<double>(nx);
        positions.nodes[i].y=static_cast<double>(ny);
    }
    for(std::size_t e=0;e<routes.edges.size();++e){
        const auto& source=g.edges[e];
        const auto& a=base.nodes[source.source];
        const auto& b=base.nodes[source.target];
        const auto& sa=desired.nodes[source.source];
        const auto& sb=desired.nodes[source.target];
        const Real dxs=static_cast<Real>(sa.x)-a.x;
        const Real dys=static_cast<Real>(sa.y)-a.y;
        const Real dxt=static_cast<Real>(sb.x)-b.x;
        const Real dyt=static_cast<Real>(sb.y)-b.y;
        const auto& points=routes.edges[e].points;
        std::vector<Real> arclength(points.size(),0.0L);
        for(std::size_t k=1;k<points.size();++k){
            const Real dx=static_cast<Real>(points[k].x)-points[k-1].x;
            const Real dy=static_cast<Real>(points[k].y)-points[k-1].y;
            arclength[k]=arclength[k-1]+std::sqrt(dx*dx+dy*dy);
        }
        const Real total=arclength.back();
        if(!(total>0.0L))return fail(TopologyError::InvalidGeometry,"zero route arclength",e);
        for(std::size_t k=0;k<points.size();++k){
            const Real weight=arclength[k]/total;
            const Real dx=(1.0L-weight)*dxs+weight*dxt;
            const Real dy=(1.0L-weight)*dys+weight*dyt;
            const Real nx=static_cast<Real>(points[k].x)+fraction*dx;
            const Real ny=static_cast<Real>(points[k].y)+fraction*dy;
            if(!std::isfinite(nx)||!std::isfinite(ny)||
               std::abs(nx)>o.coordinate_limit||std::abs(ny)>o.coordinate_limit)
                return fail(TopologyError::InvalidGeometry,"route interpolation exceeds coordinate limit",e);
            paths.edges[e].points[k]={static_cast<double>(nx),static_cast<double>(ny)};
        }
        // Preserve exact computed boundary-anchor values even under rounding.
        paths.edges[e].points.front()=port_anchor(positions,ports[e].source);
        paths.edges[e].points.back()=port_anchor(positions,ports[e].target);
    }
    if(!positions.nodes.empty()){
        Real minx=positions.nodes.front().x,miny=positions.nodes.front().y;
        Real maxx=minx+positions.nodes.front().width;
        Real maxy=miny+positions.nodes.front().height;
        for(const auto& p:positions.nodes){
            minx=std::min(minx,static_cast<Real>(p.x));
            miny=std::min(miny,static_cast<Real>(p.y));
            maxx=std::max(maxx,static_cast<Real>(p.x)+p.width);
            maxy=std::max(maxy,static_cast<Real>(p.y)+p.height);
        }
        positions.width=static_cast<double>(maxx-minx);
        positions.height=static_cast<double>(maxy-miny);
    }
    return {};
}
} // namespace

TopologyStatus audit_topology(const Graph& graph, const Layout& layout,
                              const std::vector<RouteRequest>& ports,
                              const Routes& routes, TopologySignature& out,
                              const TopologyOptions& options){
    out={};
    if(!valid_options(options))return fail(TopologyError::InvalidOptions,"invalid topology search options");
    Budget b{0,options.max_segment_checks};
    return analyze(graph,layout,ports,routes,options,b,out);
}

TopologyStatus move_topology_preserving(
    const Graph& graph, const Layout& baseline,
    const std::vector<RouteRequest>& ports, const Routes& original_routes,
    const Layout& desired, Layout& out_layout, Routes& out_routes,
    const TopologyOptions& options, TopologyReport* report){
    out_layout={};out_routes={};if(report)*report={};
    if(!valid_options(options))return fail(TopologyError::InvalidOptions,"invalid topology search options");
    if(graph.nodes.size()>options.max_nodes || graph.edges.size()>options.max_edges)
        return fail(TopologyError::CapacityExceeded,"topology graph budget exceeded");
    if(!valid_placement(graph,baseline,options)||
       !valid_placement(graph,desired,options))
        return fail(TopologyError::InvalidInput,"invalid current or desired graph geometry");
    Budget budget{0,options.max_segment_checks};
    TopologySignature initial;
    auto status=analyze(graph,baseline,ports,original_routes,options,budget,initial);
    if(!status)return status;
    double fraction=1.0;
    TopologyReport candidate;
    candidate.baseline_crossings=initial.crossing_count;
    for(std::size_t trial=0;trial<options.max_trials;++trial,fraction*=0.5){
        ++candidate.trials;
        Layout next;
        Routes paths;
        bool good=true;
        for(std::size_t sample=1;sample<=options.samples_per_trial;++sample){
            const double f=fraction*static_cast<double>(sample)/
                           static_cast<double>(options.samples_per_trial);
            status=interpolate(graph,baseline,original_routes,desired,ports,f,options,next,paths);
            if(!status){good=false;break;}
            // Exact original port values are supplied to the audit: the
            // interpolator's endpoint assignment must be consistent with them.
            TopologySignature current;
            status=analyze(graph,next,ports,paths,options,budget,current);
            ++candidate.sampled_frames;
            if(status.error==TopologyError::CapacityExceeded){
                return fail(TopologyError::CapacityExceeded,"topology move exhausted global comparison budget");
            }
            if(!status || !equal_signature(current,initial)){
                good=false;
                break;
            }
        }
        if(good){
            candidate.accepted_fraction=fraction;
            candidate.segment_checks=budget.checks;
            out_layout=std::move(next);
            out_routes=std::move(paths);
            if(report)*report=candidate;
            return {};
        }
    }
    return fail(TopologyError::NoAdmissibleStep,"no topology-preserving displacement found within bounded search");
}

} // namespace stun::graphlayout
