#include "stun/graphlayout/layered.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <utility>

namespace stun::graphlayout {
namespace {

constexpr double kMaxDimension = 1.0e7;

struct DisjointSet {
    std::vector<std::size_t> parent;
    explicit DisjointSet(std::size_t n) : parent(n) { std::iota(parent.begin(), parent.end(), 0); }
    std::size_t root(std::size_t u) {
        while (parent[u] != u) {
            parent[u] = parent[parent[u]];
            u = parent[u];
        }
        return u;
    }
    void join(std::size_t a, std::size_t b) {
        a = root(a);
        b = root(b);
        if (a != b) parent[std::max(a, b)] = std::min(a, b);
    }
};

bool dimension_ok(double n) { return std::isfinite(n) && n > 0.0 && n <= kMaxDimension; }
bool gap_ok(double n) { return std::isfinite(n) && n >= 0.0 && n <= kMaxDimension; }

Status failure(Error error, const std::string& message) { return {error, message}; }

} // namespace

Status layout_layered(const Graph& graph, Layout& out, const Options& opt) {
    out = {};
    const std::size_t n = graph.nodes.size();
    if (n > opt.max_nodes || graph.edges.size() > opt.max_edges)
        return failure(Error::CapacityExceeded, "graph exceeds configured node/edge capacity");
    if (!gap_ok(opt.node_gap) || !gap_ok(opt.layer_gap) || !gap_ok(opt.component_gap) ||
        opt.crossing_sweeps > 16 || opt.max_nodes == 0 || opt.max_edges == 0)
        return failure(Error::InvalidOptions, "invalid gap, sweep or capacity options");
    switch (opt.direction) {
    case Direction::TopToBottom: case Direction::BottomToTop:
    case Direction::LeftToRight: case Direction::RightToLeft: break;
    default: return failure(Error::InvalidOptions, "unknown graph direction");
    }

    std::unordered_map<std::string, std::size_t> seen;
    seen.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const auto& node = graph.nodes[i];
        if (node.id.empty() || !dimension_ok(node.width) || !dimension_ok(node.height))
            return failure(Error::InvalidNode, "node identity or dimensions are invalid");
        if (!seen.emplace(node.id, i).second)
            return failure(Error::DuplicateNodeId, "node identifiers must be unique");
    }
    if (n == 0) {
        if (!graph.edges.empty())
            return failure(Error::InvalidEdge, "empty graph cannot contain edges");
        return {};
    }

    // A lexical ID ordering makes the result independent of input insertion order.
    std::vector<std::size_t> canonical(n);
    std::iota(canonical.begin(), canonical.end(), 0);
    std::sort(canonical.begin(), canonical.end(), [&](std::size_t a, std::size_t b) {
        return graph.nodes[a].id < graph.nodes[b].id;
    });
    std::vector<std::size_t> id_order(n);
    for (std::size_t i = 0; i < n; ++i) id_order[canonical[i]] = i;
    const auto id_less = [&](std::size_t a, std::size_t b) { return id_order[a] < id_order[b]; };

    std::vector<std::vector<std::size_t>> outgoing(n), incoming(n);
    DisjointSet weak(n);
    for (const auto& edge : graph.edges) {
        if (edge.source >= n || edge.target >= n)
            return failure(Error::InvalidEdge, "edge references a missing node");
        weak.join(edge.source, edge.target);
        if (edge.source == edge.target) continue; // Self loops do not affect SCC ranks.
        outgoing[edge.source].push_back(edge.target);
        incoming[edge.target].push_back(edge.source);
    }
    for (std::size_t u = 0; u < n; ++u) {
        for (auto* neighbours : {&outgoing[u], &incoming[u]}) {
            std::sort(neighbours->begin(), neighbours->end(), id_less);
            neighbours->erase(std::unique(neighbours->begin(), neighbours->end()), neighbours->end());
        }
    }

    // Iterative Kosaraju: bounded stack consumption, no recursion on large graphs.
    std::vector<unsigned char> visited(n, 0);
    std::vector<std::size_t> finished;
    finished.reserve(n);
    for (std::size_t start : canonical) {
        if (visited[start]) continue;
        std::vector<std::pair<std::size_t, std::size_t>> stack{{start, 0}};
        visited[start] = 1;
        while (!stack.empty()) {
            auto& frame = stack.back();
            const auto u = frame.first;
            if (frame.second < outgoing[u].size()) {
                auto v = outgoing[u][frame.second++];
                if (!visited[v]) { visited[v] = 1; stack.emplace_back(v, 0); }
            } else {
                finished.push_back(u);
                stack.pop_back();
            }
        }
    }
    std::vector<std::size_t> scc_of(n, n);
    std::vector<std::vector<std::size_t>> sccs;
    for (auto it = finished.rbegin(); it != finished.rend(); ++it) {
        if (scc_of[*it] != n) continue;
        const std::size_t scc = sccs.size();
        sccs.emplace_back();
        std::vector<std::size_t> stack{*it};
        scc_of[*it] = scc;
        while (!stack.empty()) {
            auto u = stack.back(); stack.pop_back();
            sccs.back().push_back(u);
            for (std::size_t v : incoming[u]) {
                if (scc_of[v] == n) { scc_of[v] = scc; stack.push_back(v); }
            }
        }
    }
    // Renumber SCCs by smallest stable ID (not DFS discovery order).
    std::vector<std::size_t> scc_key(sccs.size(), n);
    for (std::size_t s = 0; s < sccs.size(); ++s)
        for (std::size_t u : sccs[s]) scc_key[s] = std::min(scc_key[s], id_order[u]);
    std::vector<std::size_t> old_sccs(sccs.size());
    std::iota(old_sccs.begin(), old_sccs.end(), 0);
    std::sort(old_sccs.begin(), old_sccs.end(), [&](std::size_t a, std::size_t b) { return scc_key[a] < scc_key[b]; });
    std::vector<std::size_t> new_id(sccs.size());
    for (std::size_t i = 0; i < old_sccs.size(); ++i) new_id[old_sccs[i]] = i;
    for (auto& s : scc_of) s = new_id[s];

    // Condensation DAG: every inter-SCC edge must advance by at least one rank.
    const std::size_t scc_count = sccs.size();
    std::vector<std::vector<std::size_t>> condensed(scc_count);
    for (std::size_t u = 0; u < n; ++u)
        for (std::size_t v : outgoing[u])
            if (scc_of[u] != scc_of[v]) condensed[scc_of[u]].push_back(scc_of[v]);
    std::vector<std::size_t> indegree(scc_count, 0), scc_rank(scc_count, 0);
    for (auto& adjacent : condensed) {
        std::sort(adjacent.begin(), adjacent.end());
        adjacent.erase(std::unique(adjacent.begin(), adjacent.end()), adjacent.end());
        for (std::size_t v : adjacent) ++indegree[v];
    }
    std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<std::size_t>> ready;
    for (std::size_t s = 0; s < scc_count; ++s) if (indegree[s] == 0) ready.push(s);
    std::size_t processed = 0;
    while (!ready.empty()) {
        const std::size_t u = ready.top(); ready.pop(); ++processed;
        for (std::size_t v : condensed[u]) {
            scc_rank[v] = std::max(scc_rank[v], scc_rank[u] + 1);
            if (--indegree[v] == 0) ready.push(v);
        }
    }
    if (processed != scc_count)
        return failure(Error::InternalInvariant, "SCC condensation must be acyclic");
    std::vector<std::size_t> node_rank(n);
    for (std::size_t i = 0; i < n; ++i) node_rank[i] = scc_rank[scc_of[i]];

    // Weak components are laid out independently, with a gap between them.
    std::vector<std::vector<std::size_t>> weak_groups(n);
    for (std::size_t u : canonical) weak_groups[weak.root(u)].push_back(u);
    std::vector<std::size_t> group_roots;
    for (std::size_t i = 0; i < n; ++i) if (!weak_groups[i].empty()) group_roots.push_back(i);
    std::sort(group_roots.begin(), group_roots.end(), [&](std::size_t a, std::size_t b) {
        return id_order[weak_groups[a].front()] < id_order[weak_groups[b].front()];
    });

    const bool horizontal = opt.direction == Direction::LeftToRight || opt.direction == Direction::RightToLeft;
    const bool reverse = opt.direction == Direction::BottomToTop || opt.direction == Direction::RightToLeft;
    const auto cross_size = [&](std::size_t u) { return horizontal ? graph.nodes[u].height : graph.nodes[u].width; };
    const auto main_size = [&](std::size_t u) { return horizontal ? graph.nodes[u].width : graph.nodes[u].height; };
    std::vector<double> cross_coord(n, 0.0), main_coord(n, 0.0);
    double next_component = 0.0, max_main_extent = 0.0;

    for (std::size_t group_root : group_roots) {
        const auto& nodes = weak_groups[group_root];
        std::size_t max_rank = 0;
        for (std::size_t u : nodes) max_rank = std::max(max_rank, node_rank[u]);
        std::vector<std::vector<std::size_t>> layers(max_rank + 1);
        for (std::size_t u : nodes) layers[node_rank[u]].push_back(u);
        std::vector<std::ptrdiff_t> positions(n, -1);

        auto sweep_layer = [&](std::size_t layer, bool from_above) {
            auto& current = layers[layer];
            const auto& adjacent = layers[from_above ? layer - 1 : layer + 1];
            for (std::size_t i = 0; i < adjacent.size(); ++i) positions[adjacent[i]] = static_cast<std::ptrdiff_t>(i);
            const std::size_t adjacent_rank = from_above ? layer - 1 : layer + 1;
            struct OrderEntry { std::size_t node, previous; double bary; bool has_neighbour; };
            std::vector<OrderEntry> order;
            for (std::size_t k = 0; k < current.size(); ++k) {
                std::size_t u = current[k];
                double sum = 0.0; std::size_t count = 0;
                const auto& neighbours = from_above ? incoming[u] : outgoing[u];
                for (std::size_t v : neighbours) if (node_rank[v] == adjacent_rank && positions[v] >= 0) {
                    sum += static_cast<double>(positions[v]); ++count;
                }
                order.push_back({u, k, count ? sum / static_cast<double>(count) : 0.0, count > 0});
            }
            std::stable_sort(order.begin(), order.end(), [](const OrderEntry& a, const OrderEntry& b) {
                if (a.has_neighbour != b.has_neighbour) return a.has_neighbour;
                if (a.has_neighbour && a.bary != b.bary) return a.bary < b.bary;
                return a.previous < b.previous;
            });
            for (std::size_t k = 0; k < order.size(); ++k) current[k] = order[k].node;
        };
        for (std::size_t pass = 0; pass < opt.crossing_sweeps; ++pass) {
            for (std::size_t rank = 1; rank <= max_rank; ++rank) sweep_layer(rank, true);
            for (std::size_t rank = max_rank; rank > 0; --rank) sweep_layer(rank - 1, false);
        }

        std::vector<double> layer_main(max_rank + 1, 0.0), layer_cross(max_rank + 1, 0.0);
        double component_cross = 0.0;
        for (std::size_t r = 0; r <= max_rank; ++r) {
            for (std::size_t u : layers[r]) {
                layer_main[r] = std::max(layer_main[r], main_size(u));
                layer_cross[r] += cross_size(u);
            }
            if (!layers[r].empty()) layer_cross[r] += opt.node_gap * (layers[r].size() - 1);
            component_cross = std::max(component_cross, layer_cross[r]);
        }
        double main_cursor = 0.0;
        for (std::size_t r = 0; r <= max_rank; ++r) {
            double cross_cursor = next_component + (component_cross - layer_cross[r]) / 2.0;
            for (std::size_t u : layers[r]) {
                cross_coord[u] = cross_cursor;
                main_coord[u] = main_cursor + (layer_main[r] - main_size(u)) / 2.0;
                cross_cursor += cross_size(u) + opt.node_gap;
            }
            main_cursor += layer_main[r];
            if (r < max_rank) main_cursor += opt.layer_gap;
        }
        max_main_extent = std::max(max_main_extent, main_cursor);
        next_component += component_cross + opt.component_gap;
    }

    Layout proposed;
    proposed.nodes.resize(n);
    for (std::size_t u = 0; u < n; ++u) {
        auto& p = proposed.nodes[u];
        p.width = graph.nodes[u].width;
        p.height = graph.nodes[u].height;
        p.rank = node_rank[u];
        p.scc = scc_of[u];
        const double main = reverse ? max_main_extent - main_coord[u] - main_size(u) : main_coord[u];
        if (horizontal) { p.x = main; p.y = cross_coord[u]; }
        else { p.x = cross_coord[u]; p.y = main; }
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || p.x < 0.0 || p.y < 0.0)
            return failure(Error::InvalidGeometry, "non-finite or negative placement");
        proposed.width = std::max(proposed.width, p.x + p.width);
        proposed.height = std::max(proposed.height, p.y + p.height);
    }
    if (!std::isfinite(proposed.width) || !std::isfinite(proposed.height))
        return failure(Error::InvalidGeometry, "layout extent exceeds finite coordinates");
    // Executable contract for the acyclic condensation, shared by all consumers.
    for (std::size_t u = 0; u < n; ++u)
        for (std::size_t v : outgoing[u])
            if (scc_of[u] != scc_of[v] && node_rank[u] >= node_rank[v])
                return failure(Error::InternalInvariant, "inter-SCC edge violates rank ordering");
    out = std::move(proposed);
    return {};
}

} // namespace stun::graphlayout
