#include "flowchart_renderer.h"
#include "flowchart/flowchart_ast.h"
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <deque>
#include <unordered_map>
#include "libavoid/libavoid.h"
#include "mustache/mustache.h"
#include "flowchart_template.h"
namespace mermaid {
namespace flowchart {

class FlowchartRenderer::Impl {
public:
    LayoutSnapshot layout(const FlowchartDiagram* diagram) {
        if (!diagram || !diagram->nodes) return {};
        if (diagram->layout_mode == FC_LAYOUT_LEGACY) {
            return layout_legacy(diagram);
        }
        return layout_professional(diagram);
    }

private:
    LayoutSnapshot layout_legacy(const FlowchartDiagram* diagram) {
        LayoutSnapshot snapshot;
        std::map<std::string, FlowchartNode*> node_registry;
        std::vector<FlowchartNode*> all_nodes;

        for (auto* n = diagram->nodes; n; n = n->next) {
            node_registry[n->id] = n;
            all_nodes.push_back(n);
        }

        std::map<std::string, int> levels;
        std::queue<std::string> q;
        std::map<std::string, std::vector<std::string>> adj;
        std::map<std::string, int> in_degree;
        
        for (auto* n : all_nodes) in_degree[n->id] = 0;
        for (auto* e = diagram->edges; e; e = e->next) {
            adj[e->from].push_back(e->to);
            in_degree[e->to]++;
        }

        for (auto* n : all_nodes) {
            if (in_degree[n->id] == 0) {
                q.push(n->id);
                levels[n->id] = 0;
            }
        }

        if (q.empty() && !all_nodes.empty()) {
            q.push(all_nodes[0]->id);
            levels[all_nodes[0]->id] = 0;
        }

        int max_level = 0;
        while (!q.empty()) {
            std::string u = q.front();
            q.pop();

            for (const auto& v : adj[u]) {
                if (levels.find(v) == levels.end()) {
                    levels[v] = levels[u] + 1;
                    max_level = std::max(max_level, levels[v]);
                    q.push(v);
                }
            }
        }

        for (auto* n : all_nodes) {
            if (levels.find(n->id) == levels.end()) levels[n->id] = 0;
        }

        std::vector<std::vector<std::string>> layer_groups(max_level + 1);
        for (auto const& [id, level] : levels) {
            layer_groups[level].push_back(id);
        }

        const float NODE_WIDTH = 130.0f;
        const float NODE_HEIGHT = 60.0f;
        const float HORIZ_GAP = 70.0f;
        const float VERT_GAP = 110.0f;

        std::map<std::string, std::pair<float, float>> coords;

        for (int l = 0; l <= max_level; ++l) {
            float total_w = (float)layer_groups[l].size() * NODE_WIDTH + (float)(layer_groups[l].size() - 1) * HORIZ_GAP;
            float start_x = -total_w / 2.0f;

            for (size_t i = 0; i < layer_groups[l].size(); ++i) {
                std::string id = layer_groups[l][i];
                float x = start_x + (float)i * (NODE_WIDTH + HORIZ_GAP);
                float y = (float)l * (NODE_HEIGHT + VERT_GAP);
                coords[id] = {x, y};

                RenderedNode rn;
                rn.id = id;
                rn.text = node_registry[id]->label ? node_registry[id]->label : id;
                rn.x = (double)x;
                rn.y = (double)y;
                rn.width = (double)NODE_WIDTH;
                rn.height = (double)NODE_HEIGHT;
                rn.shape = node_registry[id]->shape;
                snapshot.nodes.push_back(rn);
            }
        }

        for (auto* e = diagram->edges; e; e = e->next) {
            if (coords.count(e->from) && coords.count(e->to)) {
                RenderedEdge re;
                re.source_id = e->from;
                re.target_id = e->to;
                re.label = e->label ? e->label : "";
                auto start = coords[e->from];
                auto end = coords[e->to];
                re.points.push_back({(double)start.first + NODE_WIDTH/2, (double)start.second + NODE_HEIGHT});
                re.points.push_back({(double)end.first + NODE_WIDTH/2, (double)end.second});
                snapshot.edges.push_back(re);
            }
        }

        return snapshot;
    }

    static bool is_horizontal_direction(const FlowchartDiagram* diagram) {
        if (!diagram || !diagram->direction || !diagram->direction[0]) return false;
        return diagram->direction[0] == 'L' || diagram->direction[0] == 'R';
    }

    LayoutSnapshot layout_professional(const FlowchartDiagram* diagram) {
        LayoutSnapshot snapshot;
        std::vector<FlowchartNode*> nodes;
        std::unordered_map<std::string, size_t> index_of;

        for (auto* n = diagram->nodes; n; n = n->next) {
            index_of[n->id] = nodes.size();
            nodes.push_back(n);
        }

        const double DEFAULT_W = 130.0;
        const double DEFAULT_H = 60.0;
        const double H_GAP = 70.0;
        const double V_GAP = 110.0;

        const size_t ncount = nodes.size();
        std::vector<double> widths(ncount, DEFAULT_W);
        std::vector<double> heights(ncount, DEFAULT_H);

        for (size_t i = 0; i < ncount; ++i) {
            if (nodes[i]->layout_width > 0) widths[i] = nodes[i]->layout_width;
            if (nodes[i]->layout_height > 0) heights[i] = nodes[i]->layout_height;
        }

        std::vector<std::vector<size_t>> out(ncount);
        std::vector<std::vector<size_t>> in(ncount);
        std::vector<int> indeg(ncount, 0);

        for (auto* e = diagram->edges; e; e = e->next) {
            auto it_from = index_of.find(e->from ? e->from : "");
            auto it_to = index_of.find(e->to ? e->to : "");
            if (it_from == index_of.end() || it_to == index_of.end()) continue;
            size_t u = it_from->second;
            size_t v = it_to->second;
            out[u].push_back(v);
            in[v].push_back(u);
            indeg[v]++;
        }

        std::vector<size_t> topo;
        topo.reserve(ncount);
        std::deque<size_t> q;
        std::vector<int> indeg_work = indeg;
        std::vector<bool> processed(ncount, false);

        for (size_t i = 0; i < ncount; ++i) {
            if (indeg_work[i] == 0) q.push_back(i);
        }

        while (topo.size() < ncount) {
            if (q.empty()) {
                for (size_t i = 0; i < ncount; ++i) {
                    if (!processed[i]) {
                        indeg_work[i] = 0;
                        q.push_back(i);
                        break;
                    }
                }
            }
            size_t u = q.front();
            q.pop_front();
            if (processed[u]) continue;
            processed[u] = true;
            topo.push_back(u);
            for (size_t v : out[u]) {
                indeg_work[v]--;
                if (indeg_work[v] == 0) q.push_back(v);
            }
        }

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
        for (size_t i = 0; i < ncount; ++i) {
            layers[level[i]].push_back(i);
        }

        auto order_by_barycenter = [&](int layer_index, bool use_in_neighbors) {
            if (layer_index < 0 || layer_index >= (int)layers.size()) return;
            const std::vector<size_t>& prev = use_in_neighbors ? layers[layer_index - 1] : layers[layer_index + 1];
            std::vector<int> pos(ncount, -1);
            for (size_t i = 0; i < prev.size(); ++i) pos[prev[i]] = (int)i;

            struct Item {
                size_t node;
                double bary;
                bool has;
                size_t orig;
            };
            std::vector<Item> items;
            items.reserve(layers[layer_index].size());
            for (size_t i = 0; i < layers[layer_index].size(); ++i) {
                size_t u = layers[layer_index][i];
                const std::vector<size_t>& neigh = use_in_neighbors ? in[u] : out[u];
                double sum = 0.0;
                int count = 0;
                for (size_t v : neigh) {
                    if (pos[v] >= 0) {
                        sum += pos[v];
                        count++;
                    }
                }
                Item it;
                it.node = u;
                it.has = count > 0;
                it.bary = it.has ? (sum / count) : 0.0;
                it.orig = i;
                items.push_back(it);
            }

            std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
                if (a.has != b.has) return a.has > b.has;
                if (!a.has) return a.orig < b.orig;
                if (a.bary == b.bary) return a.orig < b.orig;
                return a.bary < b.bary;
            });

            for (size_t i = 0; i < items.size(); ++i) layers[layer_index][i] = items[i].node;
        };

        for (int iter = 0; iter < 2; ++iter) {
            for (int l = 1; l <= max_level; ++l) order_by_barycenter(l, true);
            for (int l = max_level - 1; l >= 0; --l) order_by_barycenter(l, false);
        }

        std::vector<double> layer_heights(max_level + 1, 0.0);
        for (int l = 0; l <= max_level; ++l) {
            double h = 0.0;
            for (size_t idx : layers[l]) h = std::max(h, heights[idx]);
            layer_heights[l] = h;
        }

        std::vector<std::pair<double, double>> coords(ncount, {0.0, 0.0});
        double y_cursor = 0.0;
        for (int l = 0; l <= max_level; ++l) {
            double total_w = 0.0;
            for (size_t i = 0; i < layers[l].size(); ++i) {
                total_w += widths[layers[l][i]];
                if (i + 1 < layers[l].size()) total_w += H_GAP;
            }
            double x_cursor = -total_w / 2.0;
            for (size_t i = 0; i < layers[l].size(); ++i) {
                size_t idx = layers[l][i];
                coords[idx] = {x_cursor, y_cursor};
                x_cursor += widths[idx] + H_GAP;
            }
            y_cursor += layer_heights[l] + V_GAP;
        }

        for (size_t i = 0; i < ncount; ++i) {
            RenderedNode rn;
            rn.id = nodes[i]->id;
            rn.text = nodes[i]->label ? nodes[i]->label : nodes[i]->id;
            rn.x = coords[i].first;
            rn.y = coords[i].second;
            rn.width = widths[i];
            rn.height = heights[i];
            rn.shape = nodes[i]->shape;
            snapshot.nodes.push_back(rn);
        }

        const bool horizontal = is_horizontal_direction(diagram);
        const double endpoint_pad = 1.0;

        auto add_straight_edge = [&](FlowchartEdge* e, size_t u, size_t v) {
            RenderedEdge re;
            re.source_id = e->from;
            re.target_id = e->to;
            re.label = e->label ? e->label : "";
            if (horizontal) {
                double sx = coords[u].first + widths[u] + endpoint_pad;
                double sy = coords[u].second + heights[u] / 2.0;
                double tx = coords[v].first - endpoint_pad;
                double ty = coords[v].second + heights[v] / 2.0;
                re.points.push_back({sx, sy});
                re.points.push_back({tx, ty});
            } else {
                double sx = coords[u].first + widths[u] / 2.0;
                double sy = coords[u].second + heights[u] + endpoint_pad;
                double tx = coords[v].first + widths[v] / 2.0;
                double ty = coords[v].second - endpoint_pad;
                re.points.push_back({sx, sy});
                re.points.push_back({tx, ty});
            }
            snapshot.edges.push_back(re);
        };

        if (diagram->edges) {
            const bool use_orthogonal = diagram->routing_mode == FC_ROUTE_ORTHOGONAL;
            Avoid::Router router(use_orthogonal ? Avoid::OrthogonalRouting : Avoid::PolyLineRouting);
            if (use_orthogonal) {
                router.setRoutingParameter(Avoid::segmentPenalty, Avoid::chooseSensibleParamValue);
            } else {
                router.setRoutingParameter(Avoid::anglePenalty, Avoid::chooseSensibleParamValue);
            }
            if (diagram->routing_shape_buffer >= 0.0) {
                router.setRoutingParameter(Avoid::shapeBufferDistance, diagram->routing_shape_buffer);
            }
            if (diagram->routing_nudging_distance >= 0.0) {
                router.setRoutingParameter(Avoid::idealNudgingDistance, diagram->routing_nudging_distance);
            }
            if (diagram->routing_segment_penalty >= 0.0) {
                router.setRoutingParameter(Avoid::segmentPenalty, diagram->routing_segment_penalty);
            }
            if (diagram->routing_angle_penalty >= 0.0) {
                router.setRoutingParameter(Avoid::anglePenalty, diagram->routing_angle_penalty);
            }
            if (diagram->routing_crossing_penalty >= 0.0) {
                router.setRoutingParameter(Avoid::crossingPenalty, diagram->routing_crossing_penalty);
            }
            if (diagram->routing_nudge_orthogonal_ends >= 0) {
                router.setRoutingOption(Avoid::nudgeOrthogonalSegmentsConnectedToShapes,
                                        diagram->routing_nudge_orthogonal_ends != 0);
            }
            if (diagram->routing_nudge_shared_paths >= 0) {
                router.setRoutingOption(Avoid::nudgeSharedPathsWithCommonEndPoint,
                                        diagram->routing_nudge_shared_paths != 0);
            }

            std::vector<Avoid::ShapeRef*> shapes;
            shapes.reserve(ncount);
            for (size_t i = 0; i < ncount; ++i) {
                Avoid::Point tl(coords[i].first, coords[i].second);
                Avoid::Point br(coords[i].first + widths[i], coords[i].second + heights[i]);
                Avoid::Rectangle rect(tl, br);
                shapes.push_back(new Avoid::ShapeRef(&router, rect));
            }

            std::vector<Avoid::ConnRef*> conns;
            std::vector<FlowchartEdge*> edges;
            for (auto* e = diagram->edges; e; e = e->next) {
                auto it_from = index_of.find(e->from ? e->from : "");
                auto it_to = index_of.find(e->to ? e->to : "");
                if (it_from == index_of.end() || it_to == index_of.end()) continue;
                size_t u = it_from->second;
                size_t v = it_to->second;

                Avoid::ConnRef* cr = new Avoid::ConnRef(&router);
                if (horizontal) {
                    Avoid::Point src(coords[u].first + widths[u] + endpoint_pad, coords[u].second + heights[u] / 2.0);
                    Avoid::Point dst(coords[v].first - endpoint_pad, coords[v].second + heights[v] / 2.0);
                    cr->setEndpoints(Avoid::ConnEnd(src, Avoid::ConnDirAll), Avoid::ConnEnd(dst, Avoid::ConnDirAll));
                } else {
                    Avoid::Point src(coords[u].first + widths[u] / 2.0, coords[u].second + heights[u] + endpoint_pad);
                    Avoid::Point dst(coords[v].first + widths[v] / 2.0, coords[v].second - endpoint_pad);
                    cr->setEndpoints(Avoid::ConnEnd(src, Avoid::ConnDirAll), Avoid::ConnEnd(dst, Avoid::ConnDirAll));
                }
                conns.push_back(cr);
                edges.push_back(e);
            }

            router.processTransaction();

            for (size_t i = 0; i < edges.size(); ++i) {
                FlowchartEdge* e = edges[i];
                auto it_from = index_of.find(e->from ? e->from : "");
                auto it_to = index_of.find(e->to ? e->to : "");
                if (it_from == index_of.end() || it_to == index_of.end()) continue;
                size_t u = it_from->second;
                size_t v = it_to->second;

                Avoid::PolyLine& route = conns[i]->displayRoute();
                if (route.ps.size() < 2) {
                    add_straight_edge(e, u, v);
                    continue;
                }

                RenderedEdge re;
                re.source_id = e->from;
                re.target_id = e->to;
                re.label = e->label ? e->label : "";
                for (const auto& p : route.ps) {
                    re.points.push_back({p.x, p.y});
                }
                snapshot.edges.push_back(re);
            }
        }

        return snapshot;
    }
};

FlowchartRenderer::FlowchartRenderer() : pimpl(std::make_unique<Impl>()) {}
FlowchartRenderer::~FlowchartRenderer() = default;

LayoutSnapshot FlowchartRenderer::layout(const FlowchartDiagram* diagram) {
    return pimpl->layout(diagram);
}

// --- Mustache Rendering Logic ---

namespace {



enum class ProxyType { Root, NodesList, EdgesList, Node, Edge, Value };

struct MustacheProxy {
    ProxyType type;
    const LayoutSnapshot* snapshot;
    size_t index = 0;
    double ox = 0;
    double oy = 0;
    mutable std::string val_str; 
};

struct ProviderPool {
    std::vector<std::unique_ptr<MustacheProxy>> storage;
    MustacheProxy* create(ProxyType t, const LayoutSnapshot* s, size_t i = 0, double x = 0, double y = 0) {
        storage.push_back(std::make_unique<MustacheProxy>(MustacheProxy{t, s, i, x, y}));
        return storage.back().get();
    }
};

static std::string format_double(double v) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << v;
    std::string s = ss.str();
    s.erase(s.find_last_not_of('0') + 1, std::string::npos);
    if (s.back() == '.') s.pop_back();
    return s;
}

static void* get_root(void* data) {
    return static_cast<ProviderPool*>(data)->storage[0].get();
}

static void* get_child_by_name(void* node, const char* name, size_t size, void* data) {
    if (!node) return nullptr;
    auto* proxy = static_cast<const MustacheProxy*>(node);
    auto* pool = static_cast<ProviderPool*>(data);
    const auto& opt = proxy->snapshot->options;
    std::string key(name, size);

    auto create_val = [&](const std::string& v) {
        auto* p = pool->create(ProxyType::Value, proxy->snapshot);
        p->val_str = v;
        return (void*)p;
    };

    if (proxy->type == ProxyType::Root) {
        if (key == "width") return create_val(format_double(proxy->snapshot->total_width));
        if (key == "height") return create_val(format_double(proxy->snapshot->total_height));
        if (key == "background_color") return create_val("#ffffff");
        if (key == "line_color") return create_val("#4b5563");
        if (key == "line_width") return create_val("1.5");
        if (key == "primary_color") return create_val("#6366f1"); // Indigo
        if (key == "primary_color_alt") return create_val("#a5b4fc"); // Lighter Indigo
        if (key == "text_color") return create_val("#1f2937");
        if (key == "nodes") return pool->create(ProxyType::NodesList, proxy->snapshot, 0, proxy->ox, proxy->oy);
        if (key == "edges") return pool->create(ProxyType::EdgesList, proxy->snapshot, 0, proxy->ox, proxy->oy);
    } else if (proxy->type == ProxyType::Node) {
        const auto& n = proxy->snapshot->nodes[proxy->index];
        if (key == "id") return create_val(n.id);
        if (key == "label") return create_val(n.text);
        if (key == "rect_x") return create_val(format_double(n.x + proxy->ox));
        if (key == "rect_y") return create_val(format_double(n.y + proxy->oy));
        if (key == "x") return create_val(format_double(n.x + proxy->ox + n.width / 2));
        if (key == "y") return create_val(format_double(n.y + proxy->oy + n.height / 2));
        if (key == "width") return create_val(format_double(n.width));
        if (key == "height") return create_val(format_double(n.height));
        
        bool is_diamond = (n.shape == FC_SHAPE_RHOMBUS || n.shape == FC_SHAPE_DIAMOND || n.shape == FC_SHAPE_HEXAGON);
        bool is_circle = (n.shape == FC_SHAPE_CIRCLE || n.shape == FC_SHAPE_DOUBLECIRCLE || n.shape == FC_SHAPE_ROUND);

        double text_offset = is_diamond ? 4.0 : 1.0;
        if (key == "text_y_node") return create_val(format_double(n.y + proxy->oy + n.height / 2 + text_offset));
        
        if (key == "shape_diamond") return is_diamond ? (void*)node : nullptr;
        if (key == "shape_circle") return is_circle ? (void*)node : nullptr;
        if (key == "shape_rect") return (!is_diamond && !is_circle) ? (void*)node : nullptr;
        
        if (key == "points_str" && is_diamond) {
            double cx = n.x + proxy->ox + n.width / 2;
            double cy = n.y + proxy->oy + n.height / 2;
            std::stringstream ss;
            ss << format_double(cx) << "," << format_double(n.y + proxy->oy) << " "
               << format_double(n.x + proxy->ox + n.width) << "," << format_double(cy) << " "
               << format_double(cx) << "," << format_double(n.y + proxy->oy + n.height) << " "
               << format_double(n.x + proxy->ox) << "," << format_double(cy);
            return create_val(ss.str());
        }
        if (key == "radius") return create_val(format_double(std::min(n.width, n.height) / 2));
    } else if (proxy->type == ProxyType::Edge) {
        const auto& e = proxy->snapshot->edges[proxy->index];
        if (key == "path_d") {
            std::stringstream ss;
            if (e.points.size() >= 2) {
                ss << "M" << format_double(e.points[0].x + proxy->ox) << " " << format_double(e.points[0].y + proxy->oy);
                for (size_t i = 1; i < e.points.size(); ++i) {
                    ss << " L " << format_double(e.points[i].x + proxy->ox) << " " << format_double(e.points[i].y + proxy->oy);
                }
            }
            return create_val(ss.str());
        }
        if (key == "label") return create_val(e.label);
        if (key == "has_label") return !e.label.empty() ? (void*)node : nullptr;
        if (key == "label_x") return create_val(format_double((e.points[0].x + e.points[1].x) / 2 + proxy->ox));
        if (key == "label_y") return create_val(format_double((e.points[0].y + e.points[1].y) / 2 + proxy->oy));
        if (key == "label_rect_w") return create_val(format_double(e.label.length() * 8 + 12));
        if (key == "label_rect_h") return create_val("20");
        if (key == "label_rect_x") return create_val(format_double((e.points[0].x + e.points[1].x) / 2 + proxy->ox - (e.label.length() * 8 + 12) / 2));
        if (key == "label_rect_y") return create_val(format_double((e.points[0].y + e.points[1].y) / 2 + proxy->oy - 10));
    }

    return nullptr;
}

static void* get_child_by_index(void* node, unsigned index, void* data) {
    if (!node) return nullptr;
    auto* proxy = static_cast<MustacheProxy*>(node);
    auto* pool = static_cast<ProviderPool*>(data);

    if (proxy->type == ProxyType::NodesList && index < proxy->snapshot->nodes.size()) {
        return pool->create(ProxyType::Node, proxy->snapshot, index, proxy->ox, proxy->oy);
    }
    if (proxy->type == ProxyType::EdgesList && index < proxy->snapshot->edges.size()) {
        return pool->create(ProxyType::Edge, proxy->snapshot, index, proxy->ox, proxy->oy);
    }
    return nullptr;
}

static int dump(void* node, int (*out)(const char*, size_t, void*), void* rdata, void*) {
    if (!node) return 0;
    auto* proxy = static_cast<MustacheProxy*>(node);
    if (proxy->type != ProxyType::Value) return out("1", 1, rdata);
    return out(proxy->val_str.c_str(), proxy->val_str.length(), rdata);
}

static int out_verbatim(const char *output, size_t size, void *data) {
    if (!output || size == 0) return 0;
    static_cast<std::stringstream*>(data)->write(output, size);
    return 0;
}

static int out_escaped(const char *output, size_t size, void *data) {
    if (!output || size == 0) return 0;
    auto* ss = static_cast<std::stringstream*>(data);
    for (size_t i = 0; i < size; i++) {
        switch (output[i]) {
            case '<': *ss << "&lt;"; break;
            case '>': *ss << "&gt;"; break;
            case '&': *ss << "&amp;"; break;
            case '\"': *ss << "&quot;"; break;
            case '\'': *ss << "&apos;"; break;
            default: ss->put(output[i]);
        }
    }
    return 0;
}

} // anon namespace

std::string FlowchartRenderer::to_svg(const LayoutSnapshot& snapshot) {
    double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    bool has_nodes = !snapshot.nodes.empty();
    bool first = true;
    for (const auto& node : snapshot.nodes) {
        if (first) {
            min_x = node.x; min_y = node.y;
            max_x = node.x + node.width; max_y = node.y + node.height;
            first = false;
        } else {
            min_x = std::min(min_x, node.x);
            min_y = std::min(min_y, node.y);
            max_x = std::max(max_x, node.x + node.width);
            max_y = std::max(max_y, node.y + node.height);
        }
    }

    LayoutSnapshot mutable_snapshot = snapshot;
    if (mutable_snapshot.total_width <= 0) {
        mutable_snapshot.total_width = has_nodes ? (max_x - min_x + 100) : 200;
    }
    if (mutable_snapshot.total_height <= 0) {
        mutable_snapshot.total_height = has_nodes ? (max_y - min_y + 100) : 200;
    }
    
    double offset_x = has_nodes ? (-min_x + 50) : 50;
    double offset_y = has_nodes ? (-min_y + 50) : 50;

    ProviderPool pool;
    pool.create(ProxyType::Root, &mutable_snapshot, 0, offset_x, offset_y);

    MUSTACHE_RENDERER renderer = { out_verbatim, out_escaped };
    MUSTACHE_DATAPROVIDER provider = { dump, get_root, get_child_by_name, get_child_by_index, nullptr };
    
    std::stringstream ss;
    const char* tpl_str = get_svg_template();
    MUSTACHE_TEMPLATE* t = mustache_compile(tpl_str, strlen(tpl_str), nullptr, nullptr, 0);
    if (t) {
        mustache_process(t, &renderer, &ss, &provider, &pool);
        mustache_release(t);
    }

    return ss.str();
}

} // namespace flowchart
} // namespace mermaid
