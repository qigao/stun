#include "dotgraph_renderer.h"
#include "dotgraph/dotgraph_ast.h"
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>
#include <iomanip>
#include <queue>
#include <deque>
#include <unordered_map>
#include "libavoid/libavoid.h"
#include "mustache/mustache.h"
#include "dotgraph_template.h"
#include "flex/runtime/expr.h"
#include <iostream>
#include <stdexcept>

#include <unordered_set>

namespace dotgraph {

// ============================================================================
// Geometry evaluator — thin layer over flex::Expr with shape variable bindings
// ============================================================================

class GeometryEval {
public:
    GeometryEval() {
        for (auto& name : {"x", "y", "w", "h", "ox", "oy",
                           "x0", "y0", "x1", "y1",
                           "len", "idx", "n"})
            expr_.add_variable(name);
    }

    void bind_node(double x, double y, double w, double h, double ox, double oy) {
        expr_.set({{"x",(float)x}, {"y",(float)y}, {"w",(float)w}, {"h",(float)h},
                   {"ox",(float)ox}, {"oy",(float)oy}});
    }

    void bind_edge(double x0, double y0, double x1, double y1, double ox, double oy, double len) {
        expr_.set({{"x0",(float)x0}, {"y0",(float)y0}, {"x1",(float)x1}, {"y1",(float)y1},
                   {"ox",(float)ox}, {"oy",(float)oy}, {"len",(float)len}});
    }

    void bind_cluster(double x, double y, double w, double h, double ox, double oy) {
        expr_.set({{"x",(float)x}, {"y",(float)y}, {"w",(float)w}, {"h",(float)h},
                   {"ox",(float)ox}, {"oy",(float)oy}});
    }

    void bind_dsl(float idx, float n) {
        expr_.set({{"idx", idx}, {"n", n}});
    }

    double eval(const std::string& s)           { return expr_.eval(s); }
    std::string eval_fmt(const std::string& s)   { return expr_.eval_fmt(s); }

    std::optional<double> try_eval(const std::string& s) {
        auto result = expr_.try_eval(s);
        if (!result || !std::isfinite(*result)) return std::nullopt;
        return result;
    }

    std::string eval_points(const std::vector<std::pair<std::string,std::string>>& pairs) {
        return expr_.eval_points(pairs);
    }

    static std::string fmt_d(double v) { return flex::Expr::format_number(v); }

private:
    flex::Expr expr_;
};

// ============================================================================
// Attribute helpers for extra map population
// ============================================================================

static const std::unordered_set<std::string> kNumericAttrs = {
    "penwidth", "fontsize", "opacity", "rx", "box_rx", "stroke_opacity",
    "arrow_opacity", "label_rect_h", "label_rect_rx",
    "r_inner_offset", "label_y_offset", "fontweight"
};

static const std::unordered_set<std::string> kSkipNodeAttrs = {
    "label", "shape", "color", "fillcolor", "fontcolor", "style", "width", "height"
};
static const std::unordered_set<std::string> kSkipEdgeAttrs = {
    "label", "color", "style"
};

static void attrs_to_map(std::unordered_map<std::string, std::string>& map,
                          const DotGraphAttr* attrs,
                          const std::unordered_set<std::string>& skip) {
    for (auto* a = attrs; a; a = a->next) {
        if (!a->key || !a->value) continue;
        if (skip.count(a->key)) continue;
        map[a->key] = a->value;
    }
}

static void eval_numeric_attrs(std::unordered_map<std::string, std::string>& map,
                                GeometryEval& geo) {
    for (auto& [key, val] : map) {
        if (!kNumericAttrs.count(key)) continue;
        const auto result = geo.try_eval(val);
        if (!result) {
            throw std::invalid_argument("invalid numeric DOT attribute '" +
                                        key + "': " + val);
        }
        val = GeometryEval::fmt_d(*result);
    }
}

static const char* attr_value(const DotGraphAttr* attrs, const char* key) {
    for (auto* attr = attrs; attr; attr = attr->next) {
        if (attr->key && attr->value && strcmp(attr->key, key) == 0) {
            return attr->value;
        }
    }
    return nullptr;
}

static const char* cascaded_attr(const DotGraphAttr* local,
                                 const DotGraphAttr* defaults,
                                 const char* key) {
    const char* value = attr_value(local, key);
    return value ? value : attr_value(defaults, key);
}

static DotGraphShape resolve_shape(const char* name) {
    if (!name) return DG_SHAPE_ELLIPSE;
    if (strcmp(name, "box") == 0 || strcmp(name, "rect") == 0 ||
        strcmp(name, "rectangle") == 0) return DG_SHAPE_BOX;
    if (strcmp(name, "circle") == 0) return DG_SHAPE_CIRCLE;
    if (strcmp(name, "diamond") == 0) return DG_SHAPE_DIAMOND;
    if (strcmp(name, "record") == 0 || strcmp(name, "Mrecord") == 0)
        return DG_SHAPE_RECORD;
    if (strcmp(name, "plaintext") == 0 || strcmp(name, "plain") == 0 ||
        strcmp(name, "none") == 0) return DG_SHAPE_PLAINTEXT;
    if (strcmp(name, "doublecircle") == 0) return DG_SHAPE_DOUBLECIRCLE;
    if (strcmp(name, "triangle") == 0) return DG_SHAPE_TRIANGLE;
    if (strcmp(name, "hexagon") == 0) return DG_SHAPE_HEXAGON;
    if (strcmp(name, "parallelogram") == 0) return DG_SHAPE_PARALLELOGRAM;
    if (strcmp(name, "cylinder") == 0) return DG_SHAPE_CYLINDER;
    if (strcmp(name, "note") == 0) return DG_SHAPE_NOTE;
    if (strcmp(name, "component") == 0) return DG_SHAPE_COMPONENT;
    if (strcmp(name, "folder") == 0) return DG_SHAPE_FOLDER;
    return DG_SHAPE_ELLIPSE;
}

class DotGraphRenderer::Impl {
public:
    LayoutSnapshot layout(const DotGraphDiagram* diagram) {
        if (!diagram || !diagram->nodes) return {};
        LayoutSnapshot snapshot;

        // Graph-level extra defaults
        snapshot.graph_extra = {
            {"bgcolor", "#ffffff"},
            {"arrow_fill", "#4b5563"},
            {"arrow_opacity", "0.6"}
        };
        // Cascade graph defaults
        {
            static const std::unordered_set<std::string> kSkipGraph = {"rankdir", "splines", "routing_mode"};
            attrs_to_map(snapshot.graph_extra, diagram->graph_defaults, kSkipGraph);
        }

        std::vector<DotGraphNode*> nodes;
        std::unordered_map<std::string, size_t> index_of;
        for (auto* n = diagram->nodes; n; n = n->next) {
            index_of[n->id] = nodes.size();
            nodes.push_back(n);
        }

        const size_t ncount = nodes.size();
        const double DEFAULT_W = 120.0;
        const double DEFAULT_H = 50.0;
        const double H_GAP = 60.0;
        const double V_GAP = 80.0;

        std::vector<double> widths(ncount, DEFAULT_W);
        std::vector<double> heights(ncount, DEFAULT_H);
        for (size_t i = 0; i < ncount; ++i) {
            if (nodes[i]->layout_width > 0) widths[i] = nodes[i]->layout_width;
            if (nodes[i]->layout_height > 0) heights[i] = nodes[i]->layout_height;
        }

        // Attribute expressions use the measured/default size as their input and
        // override it only after both dimensions have been evaluated.
        GeometryEval attr_geo;
        for (size_t i = 0; i < ncount; ++i) {
            const double base_width = widths[i];
            const double base_height = heights[i];
            std::optional<double> evaluated_width;
            std::optional<double> evaluated_height;
            attr_geo.bind_node(0, 0, base_width, base_height, 0, 0);
            attr_geo.bind_dsl((float)i, (float)ncount);
            if (const char* value = cascaded_attr(
                    nodes[i]->attrs, diagram->node_defaults, "width")) {
                evaluated_width = attr_geo.try_eval(value);
                if (!evaluated_width || *evaluated_width <= 0.0) {
                    throw std::invalid_argument("invalid node width expression: " +
                                                std::string(value));
                }
            }
            if (const char* value = cascaded_attr(
                    nodes[i]->attrs, diagram->node_defaults, "height")) {
                evaluated_height = attr_geo.try_eval(value);
                if (!evaluated_height || *evaluated_height <= 0.0) {
                    throw std::invalid_argument("invalid node height expression: " +
                                                std::string(value));
                }
            }
            if (evaluated_width) widths[i] = *evaluated_width;
            if (evaluated_height) heights[i] = *evaluated_height;
        }

        // Build adjacency
        std::vector<std::vector<size_t>> out(ncount), in(ncount);
        std::vector<int> indeg(ncount, 0);
        for (auto* e = diagram->edges; e; e = e->next) {
            auto fi = index_of.find(e->from ? e->from : "");
            auto ti = index_of.find(e->to ? e->to : "");
            if (fi == index_of.end() || ti == index_of.end()) continue;
            out[fi->second].push_back(ti->second);
            in[ti->second].push_back(fi->second);
            indeg[ti->second]++;
        }

        // Topological sort
        std::vector<size_t> topo;
        topo.reserve(ncount);
        std::deque<size_t> q;
        std::vector<int> indeg_work = indeg;
        std::vector<bool> processed(ncount, false);
        for (size_t i = 0; i < ncount; ++i)
            if (indeg_work[i] == 0) q.push_back(i);
        while (topo.size() < ncount) {
            if (q.empty()) {
                for (size_t i = 0; i < ncount; ++i)
                    if (!processed[i]) { q.push_back(i); break; }
            }
            size_t u = q.front(); q.pop_front();
            if (processed[u]) continue;
            processed[u] = true;
            topo.push_back(u);
            for (size_t v : out[u]) {
                indeg_work[v]--;
                if (indeg_work[v] == 0) q.push_back(v);
            }
        }

        // Level assignment
        std::vector<int> level(ncount, 0);
        int max_level = 0;
        for (size_t u : topo) {
            for (size_t v : out[u]) {
                if (level[v] < level[u] + 1) {
                    level[v] = level[u] + 1;
                    if (level[v] > max_level) max_level = level[v];
                }
            }
        }

        std::vector<std::vector<size_t>> layers(max_level + 1);
        for (size_t i = 0; i < ncount; ++i)
            layers[level[i]].push_back(i);

        // Barycenter ordering
        auto order_layer = [&](int li, bool use_in) {
            if (li < 0 || li >= (int)layers.size()) return;
            const auto& prev = use_in ? layers[li - 1] : layers[li + 1];
            std::vector<int> pos(ncount, -1);
            for (size_t i = 0; i < prev.size(); ++i) pos[prev[i]] = (int)i;
            struct Item { size_t node; double bary; bool has; size_t orig; };
            std::vector<Item> items;
            for (size_t i = 0; i < layers[li].size(); ++i) {
                size_t u = layers[li][i];
                const auto& neigh = use_in ? in[u] : out[u];
                double sum = 0; int cnt = 0;
                for (size_t v : neigh) if (pos[v] >= 0) { sum += pos[v]; cnt++; }
                items.push_back({u, cnt > 0 ? sum / cnt : 0.0, cnt > 0, i});
            }
            std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
                if (a.has != b.has) return a.has > b.has;
                if (!a.has) return a.orig < b.orig;
                return a.bary < b.bary;
            });
            for (size_t i = 0; i < items.size(); ++i) layers[li][i] = items[i].node;
        };
        for (int iter = 0; iter < 2; ++iter) {
            for (int l = 1; l <= max_level; ++l) order_layer(l, true);
            for (int l = max_level - 1; l >= 0; --l) order_layer(l, false);
        }

        // Coordinate assignment
        const bool horizontal = (diagram->rankdir == DG_RANKDIR_LR || diagram->rankdir == DG_RANKDIR_RL);
        std::vector<std::pair<double,double>> coords(ncount);
        double cursor = 0.0;
        for (int l = 0; l <= max_level; ++l) {
            double max_primary = 0;
            for (size_t idx : layers[l])
                max_primary = std::max(max_primary, horizontal ? widths[idx] : heights[idx]);
            double cross_cursor = 0.0;
            for (size_t i = 0; i < layers[l].size(); ++i) {
                size_t idx = layers[l][i];
                if (horizontal) {
                    coords[idx] = {cursor, cross_cursor};
                } else {
                    coords[idx] = {cross_cursor, cursor};
                }
                cross_cursor += (horizontal ? heights[idx] : widths[idx]) + H_GAP;
            }
            cursor += max_primary + V_GAP;
        }

        if (diagram->rankdir == DG_RANKDIR_BT) {
            double extent = 0.0;
            for (size_t i = 0; i < ncount; ++i)
                extent = std::max(extent, coords[i].second + heights[i]);
            for (size_t i = 0; i < ncount; ++i)
                coords[i].second = extent - coords[i].second - heights[i];
        } else if (diagram->rankdir == DG_RANKDIR_RL) {
            double extent = 0.0;
            for (size_t i = 0; i < ncount; ++i)
                extent = std::max(extent, coords[i].first + widths[i]);
            for (size_t i = 0; i < ncount; ++i)
                coords[i].first = extent - coords[i].first - widths[i];
        }

        // Center layers
        double total_cross = 0;
        for (int l = 0; l <= max_level; ++l) {
            if (layers[l].empty()) continue;
            size_t last = layers[l].back();
            double end_val = horizontal
                ? coords[last].second + heights[last]
                : coords[last].first + widths[last];
            total_cross = std::max(total_cross, end_val);
        }
        for (int l = 0; l <= max_level; ++l) {
            if (layers[l].empty()) continue;
            size_t last = layers[l].back();
            double layer_end = horizontal
                ? coords[last].second + heights[last]
                : coords[last].first + widths[last];
            double offset = (total_cross - layer_end) / 2.0;
            for (size_t idx : layers[l]) {
                if (horizontal) coords[idx].second += offset;
                else coords[idx].first += offset;
            }
        }

        // Build rendered nodes
        for (size_t i = 0; i < ncount; ++i) {
            RenderedNode rn;
            rn.id = nodes[i]->id;
            const char* label = cascaded_attr(
                nodes[i]->attrs, diagram->node_defaults, "label");
            const char* shape = cascaded_attr(
                nodes[i]->attrs, diagram->node_defaults, "shape");
            const char* color = cascaded_attr(
                nodes[i]->attrs, diagram->node_defaults, "color");
            const char* fillcolor = cascaded_attr(
                nodes[i]->attrs, diagram->node_defaults, "fillcolor");
            const char* fontcolor = cascaded_attr(
                nodes[i]->attrs, diagram->node_defaults, "fontcolor");
            const char* style = cascaded_attr(
                nodes[i]->attrs, diagram->node_defaults, "style");
            rn.label = label ? label
                             : (nodes[i]->label ? nodes[i]->label : nodes[i]->id);
            rn.shape = shape ? resolve_shape(shape) : nodes[i]->shape;
            rn.x = coords[i].first;
            rn.y = coords[i].second;
            rn.width = widths[i];
            rn.height = heights[i];
            rn.color = color ? color : "#4b5563";
            rn.fillcolor = fillcolor ? fillcolor : "#e8e8ff";
            rn.fontcolor = fontcolor ? fontcolor : "#1f2937";
            rn.style = style ? style : "";

            // Populate extra with defaults
            rn.extra = {
                {"penwidth", "1.5"},
                {"fontsize", "13"},
                {"fontweight", "500"},
                {"box_rx", "4"},
                {"opacity", "1"},
                {"r_inner_offset", "4"}
            };
            // Cascade: graph node_defaults → per-node attrs
            attrs_to_map(rn.extra, diagram->node_defaults, kSkipNodeAttrs);
            attrs_to_map(rn.extra, nodes[i]->attrs, kSkipNodeAttrs);
            // Evaluate numeric expressions
            attr_geo.bind_node(rn.x, rn.y, rn.width, rn.height, 0, 0);
            attr_geo.bind_dsl((float)i, (float)ncount);
            eval_numeric_attrs(rn.extra, attr_geo);

            snapshot.nodes.push_back(rn);
        }

        // Edge routing with libavoid
        route_edges(diagram, snapshot, nodes, index_of, coords, widths, heights, ncount, horizontal);

        // Cluster bounding boxes
        build_clusters(diagram, snapshot, index_of, coords, widths, heights, attr_geo);

        return snapshot;
    }

private:
    struct RoutedEndpoint {
        Avoid::Point point;
        Avoid::ConnDirFlags directions = Avoid::ConnDirAll;
    };

    static RoutedEndpoint endpoint_for(double x, double y,
                                       double width, double height,
                                       DotGraphCompass compass,
                                       DotGraphCompass fallback,
                                       double padding) {
        if (compass == DG_COMPASS_NONE) compass = fallback;
        const double cx = x + width / 2.0;
        const double cy = y + height / 2.0;
        switch (compass) {
            case DG_COMPASS_N:  return {{cx, y - padding}, Avoid::ConnDirUp};
            case DG_COMPASS_NE: return {{x + width + padding, y - padding},
                                        Avoid::ConnDirUp | Avoid::ConnDirRight};
            case DG_COMPASS_E:  return {{x + width + padding, cy}, Avoid::ConnDirRight};
            case DG_COMPASS_SE: return {{x + width + padding, y + height + padding},
                                        Avoid::ConnDirDown | Avoid::ConnDirRight};
            case DG_COMPASS_S:  return {{cx, y + height + padding}, Avoid::ConnDirDown};
            case DG_COMPASS_SW: return {{x - padding, y + height + padding},
                                        Avoid::ConnDirDown | Avoid::ConnDirLeft};
            case DG_COMPASS_W:  return {{x - padding, cy}, Avoid::ConnDirLeft};
            case DG_COMPASS_NW: return {{x - padding, y - padding},
                                        Avoid::ConnDirUp | Avoid::ConnDirLeft};
            case DG_COMPASS_C:  return {{cx, cy}, Avoid::ConnDirAll};
            case DG_COMPASS_NONE: break;
        }
        return {{cx, cy}, Avoid::ConnDirAll};
    }

    void route_edges(const DotGraphDiagram* diagram, LayoutSnapshot& snapshot,
                     const std::vector<DotGraphNode*>& nodes,
                     const std::unordered_map<std::string, size_t>& index_of,
                     const std::vector<std::pair<double,double>>& coords,
                     const std::vector<double>& widths, const std::vector<double>& heights,
                     size_t ncount, bool horizontal) {
        if (!diagram->edges) return;
        const double pad = 1.0;
        const bool use_ortho = (diagram->routing_mode == DG_ROUTE_ORTHOGONAL);
        Avoid::Router router(use_ortho ? Avoid::OrthogonalRouting : Avoid::PolyLineRouting);

        if (use_ortho)
            router.setRoutingParameter(Avoid::segmentPenalty, Avoid::chooseSensibleParamValue);
        else
            router.setRoutingParameter(Avoid::anglePenalty, Avoid::chooseSensibleParamValue);

        if (diagram->routing_shape_buffer >= 0.0)
            router.setRoutingParameter(Avoid::shapeBufferDistance, diagram->routing_shape_buffer);
        if (diagram->routing_nudging_distance >= 0.0)
            router.setRoutingParameter(Avoid::idealNudgingDistance, diagram->routing_nudging_distance);
        if (diagram->routing_segment_penalty >= 0.0)
            router.setRoutingParameter(Avoid::segmentPenalty, diagram->routing_segment_penalty);
        if (diagram->routing_angle_penalty >= 0.0)
            router.setRoutingParameter(Avoid::anglePenalty, diagram->routing_angle_penalty);
        if (diagram->routing_crossing_penalty >= 0.0)
            router.setRoutingParameter(Avoid::crossingPenalty, diagram->routing_crossing_penalty);
        if (diagram->routing_nudge_orthogonal_ends >= 0)
            router.setRoutingOption(Avoid::nudgeOrthogonalSegmentsConnectedToShapes,
                                    diagram->routing_nudge_orthogonal_ends != 0);
        if (diagram->routing_nudge_shared_paths >= 0)
            router.setRoutingOption(Avoid::nudgeSharedPathsWithCommonEndPoint,
                                    diagram->routing_nudge_shared_paths != 0);

        std::vector<Avoid::ShapeRef*> shapes;
        for (size_t i = 0; i < ncount; ++i) {
            Avoid::Point tl(coords[i].first, coords[i].second);
            Avoid::Point br(coords[i].first + widths[i], coords[i].second + heights[i]);
            Avoid::Rectangle rect(tl, br);
            shapes.push_back(new Avoid::ShapeRef(&router, rect));
        }

        std::vector<Avoid::ConnRef*> conns;
        std::vector<DotGraphEdge*> edge_list;
        GeometryEval edge_geo;
        for (auto* e = diagram->edges; e; e = e->next) {
            auto fi = index_of.find(e->from ? e->from : "");
            auto ti = index_of.find(e->to ? e->to : "");
            if (fi == index_of.end() || ti == index_of.end()) continue;
            size_t u = fi->second, v = ti->second;
            Avoid::ConnRef* cr = new Avoid::ConnRef(&router);
            if (horizontal) {
                const bool forward = coords[v].first >= coords[u].first;
                const auto src = endpoint_for(coords[u].first, coords[u].second,
                                              widths[u], heights[u], e->from_compass,
                                              forward ? DG_COMPASS_E : DG_COMPASS_W, pad);
                const auto dst = endpoint_for(coords[v].first, coords[v].second,
                                              widths[v], heights[v], e->to_compass,
                                              forward ? DG_COMPASS_W : DG_COMPASS_E, pad);
                cr->setEndpoints(Avoid::ConnEnd(src.point, src.directions),
                                 Avoid::ConnEnd(dst.point, dst.directions));
            } else {
                const bool forward = coords[v].second >= coords[u].second;
                const auto src = endpoint_for(coords[u].first, coords[u].second,
                                              widths[u], heights[u], e->from_compass,
                                              forward ? DG_COMPASS_S : DG_COMPASS_N, pad);
                const auto dst = endpoint_for(coords[v].first, coords[v].second,
                                              widths[v], heights[v], e->to_compass,
                                              forward ? DG_COMPASS_N : DG_COMPASS_S, pad);
                cr->setEndpoints(Avoid::ConnEnd(src.point, src.directions),
                                 Avoid::ConnEnd(dst.point, dst.directions));
            }
            conns.push_back(cr);
            edge_list.push_back(e);
        }

        if (!router.processTransaction()) {
            std::cerr << "libavoid processTransaction returned false (routing transaction failed)\n";
        }

        for (size_t i = 0; i < edge_list.size(); ++i) {
            auto* e = edge_list[i];
            RenderedEdge re;
            re.from = e->from;
            re.to = e->to;
            const char* label = cascaded_attr(
                e->attrs, diagram->edge_defaults, "label");
            const char* color = cascaded_attr(
                e->attrs, diagram->edge_defaults, "color");
            const char* style = cascaded_attr(
                e->attrs, diagram->edge_defaults, "style");
            re.label = label ? label : "";
            re.color = color ? color : "#4b5563";
            re.style = style ? style : "";
            re.directed = diagram->is_directed;

            // Populate extra with defaults
            re.extra = {
                {"penwidth", "1.5"},
                {"stroke_opacity", "0.6"},
                {"label_fontcolor", "#4b5563"},
                {"label_rect_rx", "4"},
                {"label_rect_h", "20"}
            };
            // Cascade: graph edge_defaults → per-edge attrs
            attrs_to_map(re.extra, diagram->edge_defaults, kSkipEdgeAttrs);
            attrs_to_map(re.extra, e->attrs, kSkipEdgeAttrs);

            Avoid::PolyLine& route = conns[i]->displayRoute();
            if (route.ps.size() >= 2) {
                for (const auto& p : route.ps)
                    re.points.push_back({p.x, p.y});
            } else {
                auto fi = index_of.find(e->from ? e->from : "");
                auto ti = index_of.find(e->to ? e->to : "");
                if (fi != index_of.end() && ti != index_of.end()) {
                    size_t u = fi->second, v = ti->second;
                    if (horizontal) {
                        const bool forward = coords[v].first >= coords[u].first;
                        re.points.push_back({forward ? coords[u].first + widths[u]
                                                     : coords[u].first,
                                             coords[u].second + heights[u] / 2.0});
                        re.points.push_back({forward ? coords[v].first
                                                     : coords[v].first + widths[v],
                                             coords[v].second + heights[v] / 2.0});
                    } else {
                        const bool forward = coords[v].second >= coords[u].second;
                        re.points.push_back({coords[u].first + widths[u] / 2.0,
                                             forward ? coords[u].second + heights[u]
                                                     : coords[u].second});
                        re.points.push_back({coords[v].first + widths[v] / 2.0,
                                             forward ? coords[v].second
                                                     : coords[v].second + heights[v]});
                    }
                }
            }
            // Evaluate numeric expressions in edge extras
            if (re.points.size() >= 2) {
                edge_geo.bind_edge(re.points[0].x, re.points[0].y,
                                   re.points.back().x, re.points.back().y,
                                   0, 0, (double)re.label.length());
                eval_numeric_attrs(re.extra, edge_geo);
            }
            snapshot.edges.push_back(re);
        }

        for (auto* cr : conns) router.deleteConnector(cr);
        for (auto* sr : shapes) router.deleteShape(sr);
    }

    struct ClusterExtent {
        double min_x = 0.0;
        double min_y = 0.0;
        double max_x = 0.0;
        double max_y = 0.0;
        bool found = false;
    };

    static void include_extent(ClusterExtent& target, const ClusterExtent& value) {
        if (!value.found) return;
        if (!target.found) {
            target = value;
            return;
        }
        target.min_x = std::min(target.min_x, value.min_x);
        target.min_y = std::min(target.min_y, value.min_y);
        target.max_x = std::max(target.max_x, value.max_x);
        target.max_y = std::max(target.max_y, value.max_y);
    }

    ClusterExtent build_cluster_recursive(
        const DotGraphSubGraph* sg,
        LayoutSnapshot& snapshot,
        const std::unordered_map<std::string, size_t>& index_of,
        const std::vector<std::pair<double,double>>& coords,
        const std::vector<double>& widths,
        const std::vector<double>& heights,
        GeometryEval& geometry) {
        constexpr double kClusterPadding = 20.0;
        ClusterExtent extent;
        for (auto* nr = sg->node_refs; nr; nr = nr->next) {
            auto it = index_of.find(nr->id ? nr->id : "");
            if (it == index_of.end()) continue;
            const size_t idx = it->second;
            ClusterExtent node_extent{coords[idx].first, coords[idx].second,
                                      coords[idx].first + widths[idx],
                                      coords[idx].second + heights[idx], true};
            include_extent(extent, node_extent);
        }
        for (auto* child = sg->children; child; child = child->next) {
            include_extent(extent, build_cluster_recursive(child, snapshot, index_of,
                                                           coords, widths, heights, geometry));
        }
        if (!sg->is_cluster || !extent.found) return extent;

            RenderedCluster rc;
            rc.id = sg->id ? sg->id : "";
            rc.label = sg->label ? sg->label : "";
            rc.x = extent.min_x - kClusterPadding;
            rc.y = extent.min_y - kClusterPadding;
            rc.width = (extent.max_x - extent.min_x) + 2 * kClusterPadding;
            rc.height = (extent.max_y - extent.min_y) + 2 * kClusterPadding;
            rc.color = sg->color ? sg->color : "#9ca3af";
            rc.style = sg->style ? sg->style : "";

            // Populate extra with defaults
            rc.extra = {
                {"penwidth", "1.5"},
                {"stroke_dasharray", "6 3"},
                {"opacity", "0.5"},
                {"label_fontcolor", "#374151"}
            };
            // Cascade subgraph attrs (node_defaults used as generic attrs for clusters)
            {
                static const std::unordered_set<std::string> kSkipCluster = {"label", "color", "style"};
                attrs_to_map(rc.extra, sg->node_defaults, kSkipCluster);
            }

            geometry.bind_cluster(rc.x, rc.y, rc.width, rc.height, 0, 0);
            eval_numeric_attrs(rc.extra, geometry);

            snapshot.clusters.push_back(rc);
        return ClusterExtent{rc.x, rc.y, rc.x + rc.width, rc.y + rc.height, true};
    }

    void build_clusters(const DotGraphDiagram* diagram, LayoutSnapshot& snapshot,
                        const std::unordered_map<std::string, size_t>& index_of,
                        const std::vector<std::pair<double,double>>& coords,
                        const std::vector<double>& widths, const std::vector<double>& heights,
                        GeometryEval& geometry) {
        for (auto* sg = diagram->subgraphs; sg; sg = sg->next) {
            build_cluster_recursive(sg, snapshot, index_of, coords, widths, heights, geometry);
        }
    }
};

DotGraphRenderer::DotGraphRenderer() : pimpl(std::make_unique<Impl>()) {}
DotGraphRenderer::~DotGraphRenderer() = default;

LayoutSnapshot DotGraphRenderer::layout(const DotGraphDiagram* diagram) {
    return pimpl->layout(diagram);
}

// --- Mustache SVG Rendering ---

namespace {

enum class ProxyType { Root, NodesList, EdgesList, ClustersList, Node, Edge, Cluster, Value };

struct MustacheProxy {
    ProxyType type;
    const LayoutSnapshot* snapshot;
    size_t index = 0;
    double ox = 0, oy = 0;
    mutable std::string val_str;
};

struct ProviderPool {
    std::vector<std::unique_ptr<MustacheProxy>> storage;
    GeometryEval geo; // shared evaluator instance

    MustacheProxy* create(ProxyType t, const LayoutSnapshot* s, size_t i = 0, double x = 0, double y = 0) {
        storage.push_back(std::make_unique<MustacheProxy>(MustacheProxy{t, s, i, x, y}));
        return storage.back().get();
    }
};

static std::string fmt(double v) { return GeometryEval::fmt_d(v); }

static void* get_root(void* data) {
    return static_cast<ProviderPool*>(data)->storage[0].get();
}

static void* get_child_by_name(void* node, const char* name, size_t size, void* data) {
    if (!node) return nullptr;
    auto* proxy = static_cast<const MustacheProxy*>(node);
    auto* pool = static_cast<ProviderPool*>(data);
    auto& geo = pool->geo;
    std::string key(name, size);

    auto val = [&](const std::string& v) -> void* {
        auto* p = pool->create(ProxyType::Value, proxy->snapshot);
        p->val_str = v;
        return p;
    };

    if (proxy->type == ProxyType::Root) {
        if (key == "width") return val(fmt(proxy->snapshot->width));
        if (key == "height") return val(fmt(proxy->snapshot->height));
        if (key == "nodes") return pool->create(ProxyType::NodesList, proxy->snapshot, 0, proxy->ox, proxy->oy);
        if (key == "edges") return pool->create(ProxyType::EdgesList, proxy->snapshot, 0, proxy->ox, proxy->oy);
        if (key == "clusters") return !proxy->snapshot->clusters.empty()
            ? (void*)pool->create(ProxyType::ClustersList, proxy->snapshot, 0, proxy->ox, proxy->oy)
            : nullptr;
        // Fallthrough: look up graph_extra map
        { auto it = proxy->snapshot->graph_extra.find(key); if (it != proxy->snapshot->graph_extra.end()) return val(it->second); }
    }
    else if (proxy->type == ProxyType::Node) {
        const auto& n = proxy->snapshot->nodes[proxy->index];
        geo.bind_node(n.x, n.y, n.width, n.height, proxy->ox, proxy->oy);

        if (key == "id") return val(n.id);
        if (key == "label") return val(n.label);
        if (key == "cx") return val(geo.eval_fmt("x + ox + w / 2"));
        if (key == "cy") return val(geo.eval_fmt("y + oy + h / 2"));
        if (key == "rx") return val(geo.eval_fmt("w / 2"));
        if (key == "ry") return val(geo.eval_fmt("h / 2"));
        if (key == "r") return val(geo.eval_fmt("min(w, h) / 2"));
        if (key == "r_inner") {
            auto it = n.extra.find("r_inner_offset");
            std::string offset = (it != n.extra.end()) ? it->second : "4";
            return val(geo.eval_fmt("min(w, h) / 2 - " + offset));
        }
        if (key == "rect_x") return val(geo.eval_fmt("x + ox"));
        if (key == "rect_y") return val(geo.eval_fmt("y + oy"));
        if (key == "w") return val(fmt(n.width));
        if (key == "h") return val(fmt(n.height));
        if (key == "color") return val(n.color);
        if (key == "fillcolor") return val(n.fillcolor);
        if (key == "fontcolor") return val(n.fontcolor);
        if (key == "text_y") return val(geo.eval_fmt("y + oy + h / 2 + 1"));

        bool is_box = (n.shape == DG_SHAPE_BOX || n.shape == DG_SHAPE_RECORD);
        bool is_circle = (n.shape == DG_SHAPE_CIRCLE);
        bool is_diamond = (n.shape == DG_SHAPE_DIAMOND);
        bool is_doublecircle = (n.shape == DG_SHAPE_DOUBLECIRCLE);
        bool is_triangle = (n.shape == DG_SHAPE_TRIANGLE);
        bool is_hexagon = (n.shape == DG_SHAPE_HEXAGON);
        bool is_plaintext = (n.shape == DG_SHAPE_PLAINTEXT);
        bool is_ellipse = !is_box && !is_circle && !is_diamond && !is_doublecircle
                          && !is_triangle && !is_hexagon && !is_plaintext;

        if (key == "is_ellipse") return is_ellipse ? (void*)node : nullptr;
        if (key == "is_box") return is_box ? (void*)node : nullptr;
        if (key == "is_circle") return is_circle ? (void*)node : nullptr;
        if (key == "is_diamond") return is_diamond ? (void*)node : nullptr;
        if (key == "is_doublecircle") return is_doublecircle ? (void*)node : nullptr;
        if (key == "is_triangle") return is_triangle ? (void*)node : nullptr;
        if (key == "is_hexagon") return is_hexagon ? (void*)node : nullptr;
        if (key == "is_plaintext") return is_plaintext ? (void*)node : nullptr;

        // Shape polygon points through MIR-backed expressions.
        if (key == "diamond_points") {
            return val(geo.eval_points({
                {"x + ox + w/2", "y + oy"},          // top
                {"x + ox + w",   "y + oy + h/2"},    // right
                {"x + ox + w/2", "y + oy + h"},      // bottom
                {"x + ox",       "y + oy + h/2"}     // left
            }));
        }
        if (key == "triangle_points") {
            return val(geo.eval_points({
                {"x + ox + w/2", "y + oy"},           // top
                {"x + ox + w",   "y + oy + h"},       // bottom-right
                {"x + ox",       "y + oy + h"}        // bottom-left
            }));
        }
        if (key == "hexagon_points") {
            return val(geo.eval_points({
                {"x + ox + w/4",     "y + oy"},           // top-left
                {"x + ox + 3*w/4",   "y + oy"},           // top-right
                {"x + ox + w",       "y + oy + h/2"},     // right
                {"x + ox + 3*w/4",   "y + oy + h"},       // bottom-right
                {"x + ox + w/4",     "y + oy + h"},        // bottom-left
                {"x + ox",           "y + oy + h/2"}      // left
            }));
        }
        // Fallthrough: look up extra map
        { auto it = n.extra.find(key); if (it != n.extra.end()) return val(it->second); }
    }
    else if (proxy->type == ProxyType::Edge) {
        const auto& e = proxy->snapshot->edges[proxy->index];
        if (key == "path_d") {
            std::stringstream ss;
            if (e.points.size() >= 2) {
                ss << "M" << fmt(e.points[0].x + proxy->ox) << " " << fmt(e.points[0].y + proxy->oy);
                for (size_t i = 1; i < e.points.size(); ++i)
                    ss << " L " << fmt(e.points[i].x + proxy->ox) << " " << fmt(e.points[i].y + proxy->oy);
            }
            return val(ss.str());
        }
        if (key == "color") return val(e.color);
        if (key == "directed") return e.directed ? (void*)node : nullptr;
        if (key == "label") return val(e.label);
        if (key == "has_label") return !e.label.empty() ? (void*)node : nullptr;

        // Edge label positioning through MIR-backed expressions.
        if (e.points.size() >= 2) {
            geo.bind_edge(e.points[0].x, e.points[0].y,
                          e.points.back().x, e.points.back().y,
                          proxy->ox, proxy->oy, (double)e.label.length());

            if (key == "label_x") return val(geo.eval_fmt("(x0 + x1) / 2 + ox"));
            if (key == "label_y") return val(geo.eval_fmt("(y0 + y1) / 2 + oy"));
            if (key == "label_rect_w") return val(geo.eval_fmt("len * 7.5 + 12"));
            if (key == "label_rect_h") {
                auto it = e.extra.find("label_rect_h");
                return val(it != e.extra.end() ? it->second : "20");
            }
            if (key == "label_rect_x") return val(geo.eval_fmt("(x0 + x1) / 2 + ox - (len * 7.5 + 12) / 2"));
            if (key == "label_rect_y") return val(geo.eval_fmt("(y0 + y1) / 2 + oy - 10"));
        }
        // Fallthrough: look up extra map
        { auto it = e.extra.find(key); if (it != e.extra.end()) return val(it->second); }
    }
    else if (proxy->type == ProxyType::Cluster) {
        const auto& c = proxy->snapshot->clusters[proxy->index];
        geo.bind_cluster(c.x, c.y, c.width, c.height, proxy->ox, proxy->oy);

        if (key == "x") return val(geo.eval_fmt("x + ox"));
        if (key == "y") return val(geo.eval_fmt("y + oy"));
        if (key == "w") return val(fmt(c.width));
        if (key == "h") return val(fmt(c.height));
        if (key == "color") return val(c.color);
        if (key == "label") return val(c.label);
        if (key == "has_label") return !c.label.empty() ? (void*)node : nullptr;
        if (key == "label_x") return val(geo.eval_fmt("x + ox + w / 2"));
        if (key == "label_y") return val(geo.eval_fmt("y + oy - 5"));
        // Fallthrough: look up extra map
        { auto it = c.extra.find(key); if (it != c.extra.end()) return val(it->second); }
    }

    return nullptr;
}

static void* get_child_by_index(void* node, unsigned index, void* data) {
    if (!node) return nullptr;
    auto* proxy = static_cast<MustacheProxy*>(node);
    auto* pool = static_cast<ProviderPool*>(data);
    if (proxy->type == ProxyType::NodesList && index < proxy->snapshot->nodes.size())
        return pool->create(ProxyType::Node, proxy->snapshot, index, proxy->ox, proxy->oy);
    if (proxy->type == ProxyType::EdgesList && index < proxy->snapshot->edges.size())
        return pool->create(ProxyType::Edge, proxy->snapshot, index, proxy->ox, proxy->oy);
    if (proxy->type == ProxyType::ClustersList && index < proxy->snapshot->clusters.size())
        return pool->create(ProxyType::Cluster, proxy->snapshot, index, proxy->ox, proxy->oy);
    return nullptr;
}

static int dump(void* node, int (*out)(const char*, size_t, void*), void* rdata, void*) {
    if (!node) return 0;
    auto* proxy = static_cast<MustacheProxy*>(node);
    if (proxy->type != ProxyType::Value) return out("1", 1, rdata);
    return out(proxy->val_str.c_str(), proxy->val_str.length(), rdata);
}

static int out_verbatim(const char* output, size_t size, void* data) {
    if (!output || size == 0) return 0;
    static_cast<std::stringstream*>(data)->write(output, size);
    return 0;
}

static int out_escaped(const char* output, size_t size, void* data) {
    if (!output || size == 0) return 0;
    auto* ss = static_cast<std::stringstream*>(data);
    for (size_t i = 0; i < size; i++) {
        switch (output[i]) {
            case '<': *ss << "&lt;"; break;
            case '>': *ss << "&gt;"; break;
            case '&': *ss << "&amp;"; break;
            case '"': *ss << "&quot;"; break;
            case '\'': *ss << "&apos;"; break;
            default: ss->put(output[i]);
        }
    }
    return 0;
}

} // anon namespace

std::string DotGraphRenderer::to_svg(const LayoutSnapshot& snapshot) {
    double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    bool first = true;
    for (const auto& n : snapshot.nodes) {
        if (first) {
            min_x = n.x; min_y = n.y;
            max_x = n.x + n.width; max_y = n.y + n.height;
            first = false;
        } else {
            min_x = std::min(min_x, n.x);
            min_y = std::min(min_y, n.y);
            max_x = std::max(max_x, n.x + n.width);
            max_y = std::max(max_y, n.y + n.height);
        }
    }
    for (const auto& c : snapshot.clusters) {
        min_x = std::min(min_x, c.x);
        min_y = std::min(min_y, c.y);
        max_x = std::max(max_x, c.x + c.width);
        max_y = std::max(max_y, c.y + c.height);
    }

    LayoutSnapshot ms = snapshot;
    bool has = !snapshot.nodes.empty();
    if (ms.width <= 0) ms.width = has ? (max_x - min_x + 100) : 200;
    if (ms.height <= 0) ms.height = has ? (max_y - min_y + 100) : 200;

    double ox = has ? (-min_x + 50) : 50;
    double oy = has ? (-min_y + 50) : 50;

    ProviderPool pool;
    pool.create(ProxyType::Root, &ms, 0, ox, oy);

    MUSTACHE_RENDERER renderer = { out_verbatim, out_escaped };
    MUSTACHE_DATAPROVIDER provider = { dump, get_root, get_child_by_name, get_child_by_index, nullptr };

    std::stringstream ss;
    const char* tpl = get_svg_template();
    MUSTACHE_TEMPLATE* t = mustache_compile(tpl, strlen(tpl), nullptr, nullptr, 0);
    if (t) {
        mustache_process(t, &renderer, &ss, &provider, &pool);
        mustache_release(t);
    }
    return ss.str();
}

} // namespace dotgraph
