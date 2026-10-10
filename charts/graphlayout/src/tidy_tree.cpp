#include "stun/graphlayout/tidy_tree.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace stun::graphlayout {
namespace {

TidyTreeStatus error(TidyTreeError kind, const char* reason) {
    return {kind, reason};
}

struct Profile {
    std::vector<double> left;
    std::vector<double> right;
};

// Combine two already-laid-out forests at the same vertical origin. Existing
// sibling placement is kept fixed; the next subtree shifts only to the right.
// Relative profiles are measured from their root centers.
bool place_next(Profile& together, const Profile& next,
                double gap, double& shift) {
    shift = 0.0;
    if (!together.left.empty()) {
        const std::size_t depth = std::min(together.right.size(), next.left.size());
        for (std::size_t d = 0; d < depth; ++d)
            shift = std::max(shift, together.right[d] + gap - next.left[d]);
    }
    if (!std::isfinite(shift)) return false;
    if (together.left.size() < next.left.size()) {
        together.left.resize(next.left.size(), std::numeric_limits<double>::infinity());
        together.right.resize(next.right.size(), -std::numeric_limits<double>::infinity());
    }
    for (std::size_t d = 0; d < next.left.size(); ++d) {
        const double a = next.left[d] + shift;
        const double b = next.right[d] + shift;
        if (!std::isfinite(a) || !std::isfinite(b)) return false;
        together.left[d] = std::min(together.left[d], a);
        together.right[d] = std::max(together.right[d], b);
    }
    return true;
}

} // namespace

TidyTreeStatus layout_tidy_tree(const Tree& tree, TidyTreeLayout& out,
                                const TidyTreeOptions& opt) {
    out = {};
    if (!std::isfinite(opt.sibling_gap) || opt.sibling_gap < 0.0 ||
        !std::isfinite(opt.layer_gap) || opt.layer_gap < 0.0 ||
        !std::isfinite(opt.forest_gap) || opt.forest_gap < 0.0 ||
        opt.max_nodes == 0 || opt.max_contour_cells == 0 || opt.max_pair_checks == 0)
        return error(TidyTreeError::InvalidOptions, "invalid tree layout gaps or capacities");
    const std::size_t n = tree.nodes.size();
    if (n > opt.max_nodes)
        return error(TidyTreeError::CapacityExceeded, "tree node capacity exceeded");
    if (n == 0) return {};

    std::unordered_set<std::string> ids;
    ids.reserve(n);
    std::vector<std::vector<std::size_t>> children(n);
    std::vector<std::size_t> roots;
    roots.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const TreeNode& node = tree.nodes[i];
        if (node.id.empty() || !std::isfinite(node.width) || !std::isfinite(node.height) ||
            node.width <= 0.0 || node.height <= 0.0)
            return error(TidyTreeError::InvalidNode, "invalid tree node identity or dimensions");
        if (!ids.insert(node.id).second)
            return error(TidyTreeError::DuplicateId, "duplicate tree node ID");
        if (node.parent == TreeNoParent) roots.push_back(i);
        else {
            if (node.parent >= n || node.parent == i)
                return error(TidyTreeError::InvalidParent, "invalid tree parent index");
            children[node.parent].push_back(i);
        }
    }
    if (roots.empty())
        return error(TidyTreeError::InvalidForest, "forest has no roots");

    // Iterative DFS avoids stack exhaustion in deep paths. Parent links form a
    // forest iff all nodes are reachable from the declared roots.
    std::vector<std::size_t> traversal;
    traversal.reserve(n);
    std::vector<std::size_t> depth(n, n);
    std::vector<std::size_t> stack;
    stack.reserve(n);
    for (auto it = roots.rbegin(); it != roots.rend(); ++it) {
        depth[*it] = 0;
        stack.push_back(*it);
    }
    std::vector<double> max_height;
    while (!stack.empty()) {
        const std::size_t u = stack.back();
        stack.pop_back();
        traversal.push_back(u);
        const std::size_t d = depth[u];
        if (d >= n) return error(TidyTreeError::InternalInvariant, "invalid depth");
        if (max_height.size() <= d) max_height.resize(d+1, 0.0);
        max_height[d] = std::max(max_height[d], tree.nodes[u].height);
        for (auto it = children[u].rbegin(); it != children[u].rend(); ++it) {
            if (depth[*it] != n)
                return error(TidyTreeError::InvalidForest, "forest contains repeated visits");
            depth[*it] = d + 1;
            stack.push_back(*it);
        }
    }
    if (traversal.size() != n)
        return error(TidyTreeError::InvalidForest, "forest contains a cycle or unreachable nodes");

    std::vector<Profile> profiles(n);
    std::vector<double> relative(n, 0.0);
    std::size_t cells = 0;
    // Build subtree contours bottom-up. The bound explicitly accounts for
    // retained profiles on *all* ancestors, including long chains.
    for (auto it = traversal.rbegin(); it != traversal.rend(); ++it) {
        const std::size_t u = *it;
        Profile merged;
        for (const std::size_t v : children[u]) {
            double dx = 0.0;
            if (!place_next(merged, profiles[v], opt.sibling_gap, dx))
                return error(TidyTreeError::InvalidGeometry, "tree contour overflow");
            relative[v] = dx;
        }
        double center = 0.0;
        if (!children[u].empty()) {
            center = 0.5 * (relative[children[u].front()] + relative[children[u].back()]);
            if (!std::isfinite(center))
                return error(TidyTreeError::InvalidGeometry, "tree subtree center overflow");
        }
        Profile& p = profiles[u];
        p.left.reserve(merged.left.size() + 1);
        p.right.reserve(merged.right.size() + 1);
        p.left.push_back(-tree.nodes[u].width * 0.5);
        p.right.push_back(tree.nodes[u].width * 0.5);
        for (std::size_t d = 0; d < merged.left.size(); ++d) {
            p.left.push_back(merged.left[d] - center);
            p.right.push_back(merged.right[d] - center);
        }
        for (const std::size_t v : children[u]) relative[v] -= center;
        for (std::size_t d = 0; d < p.left.size(); ++d)
            if (!std::isfinite(p.left[d]) || !std::isfinite(p.right[d]))
                return error(TidyTreeError::InvalidGeometry, "nonfinite tree contour");
        if (p.left.size() > (opt.max_contour_cells - cells) / 2)
            return error(TidyTreeError::CapacityExceeded, "tree contour memory budget exceeded");
        cells += p.left.size() * 2;
    }

    Profile forest;
    for (const std::size_t r : roots) {
        double dx = 0.0;
        if (!place_next(forest, profiles[r], opt.forest_gap, dx))
            return error(TidyTreeError::InvalidGeometry, "forest contour overflow");
        relative[r] = dx;
    }
    if (forest.left.empty())
        return error(TidyTreeError::InternalInvariant, "empty forest contour");
    double min_x = forest.left[0];
    for (const double x : forest.left) min_x = std::min(min_x, x);
    if (!std::isfinite(min_x))
        return error(TidyTreeError::InvalidGeometry, "nonfinite forest bound");

    std::vector<double> y_of_depth(max_height.size(), 0.0);
    for (std::size_t d = 1; d < y_of_depth.size(); ++d) {
        y_of_depth[d] = y_of_depth[d-1] + max_height[d-1] + opt.layer_gap;
        if (!std::isfinite(y_of_depth[d]))
            return error(TidyTreeError::InvalidGeometry, "tree vertical coordinate overflow");
    }

    TidyTreeLayout candidate;
    candidate.nodes.resize(n);
    std::vector<double> centers(n);
    for (const std::size_t u : traversal) {
        const TreeNode& source = tree.nodes[u];
        centers[u] = relative[u] + (source.parent == TreeNoParent
                                  ? -min_x : centers[source.parent]);
        auto& p = candidate.nodes[u];
        p.x = centers[u] - source.width * 0.5;
        p.y = y_of_depth[depth[u]];
        p.width = source.width;
        p.height = source.height;
        p.parent = source.parent;
        p.depth = depth[u];
        if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
            !std::isfinite(p.x + p.width) || !std::isfinite(p.y + p.height))
            return error(TidyTreeError::InvalidGeometry, "tree placement overflow");
        candidate.width = std::max(candidate.width, p.x + p.width);
        candidate.height = std::max(candidate.height, p.y + p.height);
    }
    if (!std::isfinite(candidate.width) || !std::isfinite(candidate.height))
        return error(TidyTreeError::InvalidGeometry, "tree bounds overflow");

    // Check postconditions independently of contour construction. This is a
    // geometry gate, not a formal proof of optimal or minimum-width layout.
    std::size_t checks = 0;
    for (std::size_t a = 0; a < n; ++a) {
        const auto& u = candidate.nodes[a];
        if (u.x < -1e-8 || u.y < 0.0 || u.x + u.width > candidate.width + 1e-8 ||
            u.y + u.height > candidate.height + 1e-8)
            return error(TidyTreeError::InternalInvariant, "node exceeds tree layout bounds");
        for (std::size_t b = a+1; b < n; ++b) {
            if (++checks > opt.max_pair_checks)
                return error(TidyTreeError::CapacityExceeded, "tree pair validation budget exceeded");
            const auto& v = candidate.nodes[b];
            if (!(u.x + u.width <= v.x + 1e-8 || v.x + v.width <= u.x + 1e-8 ||
                  u.y + u.height <= v.y + 1e-8 || v.y + v.height <= u.y + 1e-8))
                return error(TidyTreeError::InternalInvariant, "tree rectangle overlap");
        }
    }
    out = std::move(candidate);
    return {};
}

} // namespace stun::graphlayout
