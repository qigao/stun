/*
 * NanoVG CSS - Quadtree Layout Engine
 *
 * Three-phase layout algorithm:
 * 1. Constraint Collection (top-down) - propagate available space
 * 2. Dimension Calculation (bottom-up) - compute sizes
 * 3. Position Calculation (top-down) - compute positions
 */

#pragma once

#include "cssbox_types.h"
#include <vector>

namespace cssbox {

// Forward declarations
struct LayoutNode;
struct LayoutConstraints;
class QuadtreeLayoutEngine;

// ============================================================================
// Layout Constraints
// ============================================================================

struct LayoutConstraints {
    // Available space from parent (resolved)
    float available_width;
    float available_height;
    
    // CSS dimensions (may be auto, percentage, or fixed)
    Length width;
    Length height;
    Length min_width;
    Length max_width;
    Length min_height;
    Length max_height;
    
    // Padding and margin
    float padding[4];  // top, right, bottom, left
    float margin[4];   // top, right, bottom, left
    float border[4];   // top, right, bottom, left
    
    LayoutConstraints() 
        : available_width(0), available_height(0) {
        for (int i = 0; i < 4; i++) {
            padding[i] = 0;
            margin[i] = 0;
            border[i] = 0;
        }
    }
};

// ============================================================================
// Layout Node (Quadtree Node)
// ============================================================================

struct LayoutNode {
    enum Type {
        BLOCK,      // Regular block layout
        FLEX,       // Flexbox container
        GRID        // Grid container
    };
    
    // Computed bounds (output)
    float x, y;
    float width, height;
    float content_width, content_height;
    
    // Layout constraints (input)
    LayoutConstraints constraints;
    
    // Element reference
    cssboxElement* element;
    
    // Tree structure
    LayoutNode* parent;
    std::vector<LayoutNode*> children;
    
    // Layout type
    Type type;
    
    // Dirty flag for incremental updates
    bool needs_layout;
    
    LayoutNode(cssboxElement* elem = nullptr)
        : x(0), y(0), width(0), height(0)
        , content_width(0), content_height(0)
        , element(elem), parent(nullptr)
        , type(BLOCK), needs_layout(true) {}
    
    ~LayoutNode() {
        for (auto* child : children) {
            delete child;
        }
    }
    
    // Mark this node and ancestors as needing layout
    void mark_dirty() {
        needs_layout = true;
        if (parent) {
            parent->mark_dirty();
        }
    }
};

// ============================================================================
// Quadtree Layout Engine
// ============================================================================

class QuadtreeLayoutEngine {
public:
    QuadtreeLayoutEngine(float viewport_width, float viewport_height);
    ~QuadtreeLayoutEngine();
    
    // Build layout tree from element hierarchy
    LayoutNode* build_tree(cssboxElement* root, cssboxRenderer* renderer);
    
    // Compute layout (three-phase algorithm)
    void compute_layout(LayoutNode* root, cssboxRenderer* renderer);
    
    // Update viewport dimensions
    void set_viewport(float width, float height);
    
    // Write computed layout back to elements
    void write_to_elements(LayoutNode* root);
    
private:
    float viewport_width_;
    float viewport_height_;
    
    // Phase 1: Collect constraints (top-down)
    void collect_constraints(LayoutNode* node, const LayoutConstraints& parent_constraints);
    
    // Phase 2: Calculate dimensions (bottom-up)
    void calculate_dimensions(LayoutNode* node, cssboxRenderer* renderer);
    
    // Phase 3: Calculate positions (top-down)
    void calculate_positions(LayoutNode* node, float parent_x, float parent_y);
    
    // Layout type-specific dimension calculation
    void calculate_block_dimensions(LayoutNode* node, cssboxRenderer* renderer);
    void calculate_flex_dimensions(LayoutNode* node, cssboxRenderer* renderer);
    void calculate_flex_single_line(LayoutNode* node, bool is_row, float available_main, float available_cross);
    void calculate_flex_multi_line(LayoutNode* node, bool is_row, float available_main, float available_cross);
    void calculate_grid_dimensions(LayoutNode* node, cssboxRenderer* renderer);
    
    // Layout type-specific positioning
    void position_block_children(LayoutNode* node);
    void position_flex_children(LayoutNode* node);
    void position_grid_children(LayoutNode* node);
    
    // Helper functions
    float resolve_length(const Length& length, float context_size, float font_size);
    float clamp_dimension(float value, float min_val, float max_val);
};

} // namespace cssbox
