#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace stun::graphlayout {

inline constexpr std::size_t TreeNoParent = std::numeric_limits<std::size_t>::max();

// Ordered rooted forest, not an arbitrary DAG. Child/sibling order is determined
// by input node order; a caller must not rely on hash-map iteration ordering.
struct TreeNode {
    std::string id;
    double width = 0.0;
    double height = 0.0;
    std::size_t parent = TreeNoParent;
};

struct Tree {
    std::vector<TreeNode> nodes;
};

struct TidyTreeOptions {
    double sibling_gap = 24.0;
    double layer_gap = 48.0;
    double forest_gap = 96.0;
    std::size_t max_nodes = 2048;
    // Contour merging is bounded but can take O(n^2) space/time for a chain.
    std::size_t max_contour_cells = 5000000;
    std::size_t max_pair_checks = 2500000;
};

struct TidyTreeNode {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    std::size_t depth = 0;
    std::size_t parent = TreeNoParent;
};

struct TidyTreeLayout {
    // Index and parent index retain the original input order.
    std::vector<TidyTreeNode> nodes;
    double width = 0.0;
    double height = 0.0;
};

enum class TidyTreeError {
    None,
    InvalidNode,
    DuplicateId,
    InvalidParent,
    InvalidForest,
    InvalidOptions,
    CapacityExceeded,
    InvalidGeometry,
    InternalInvariant,
};

struct TidyTreeStatus {
    TidyTreeError error = TidyTreeError::None;
    std::string message;
    explicit operator bool() const { return error == TidyTreeError::None; }
};

// Variable-size, sibling-order-preserving tree placement. Subtree contours
// enforce separation at *every shared depth*, while each parent's center
// lies midway between its first and last child centers. Supports forests,
// unbalanced branching, and independent widths/heights at every node.
//
// The algorithm is deterministic for identical ordered input. Its bounded
// contour merge is O(n^2) worst case; it does NOT claim linear time or
// globally minimal horizontal extent. Empty graph succeeds with zero bounds.
// Errors clear the output; no projection or other layout fallback is invoked.
TidyTreeStatus layout_tidy_tree(const Tree& tree, TidyTreeLayout& out,
                                const TidyTreeOptions& options = {});

} // namespace stun::graphlayout
