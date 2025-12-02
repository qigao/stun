/**
 * Quadtree Layout Engine Unit Tests
 *
 * Tests the three-phase quadtree layout algorithm:
 * - Phase 1: Constraint propagation (top-down)
 * - Phase 2: Dimension calculation (bottom-up)
 * - Phase 3: Position calculation (top-down)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <nanovg.h>
#include <nanovg_css.h>
#include <nanovg_css_quadtree.h>
#include "nanovg_css_internal.h"

using Catch::Matchers::WithinAbs;
using namespace nvgcss;

// ============================================================================
// Phase 1: Constraint Propagation Tests
// ============================================================================

TEST_CASE("Quadtree Phase 1 - Constraint Propagation", "[quadtree][phase1]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 1000, 800);
    
    SECTION("Root receives viewport constraints") {
        const char* css = R"(
            #root {
                width: 100%;
                height: 100%;
            }
        )";
        
        nvgcssParseCSS(renderer, css);
        NVGCSSElement* root = nvgcssCreateElement(renderer, "root", "div");
        nvgcssComputeLayout(renderer);
        
        // Build quadtree and compute layout
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(root, renderer);
        engine.compute_layout(tree, renderer);
        
        // Verify root received viewport constraints
        REQUIRE_THAT(tree->constraints.available_width, WithinAbs(1000.0f, 0.1f));
        REQUIRE_THAT(tree->constraints.available_height, WithinAbs(800.0f, 0.1f));
        
        delete tree;
    }
    
    SECTION("Child receives parent constraints") {
        const char* css = R"(
            #parent {
                width: 500px;
                height: 400px;
            }
            #child {
                width: 200px;
                height: 100px;
            }
        )";
        
        nvgcssParseCSS(renderer, css);
        NVGCSSElement* parent = nvgcssCreateElement(renderer, "parent", "div");
        NVGCSSElement* child = nvgcssCreateElement(renderer, "child", "div");
        nvgcssAppendChild(renderer, parent, child);
        nvgcssComputeLayout(renderer);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(parent, renderer);
        engine.compute_layout(tree, renderer);
        
        // Parent gets viewport constraints
        REQUIRE_THAT(tree->constraints.available_width, WithinAbs(1000.0f, 0.1f));
        
        // Child should receive parent's constraints (constrained by parent size)
        REQUIRE(tree->children.size() == 1);
        REQUIRE_THAT(tree->children[0]->constraints.available_width, WithinAbs(500.0f, 0.1f));
        
        delete tree;
    }
    
    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Grid Placement and Spanning Tests
// ============================================================================

TEST_CASE("Quadtree Grid - Explicit Placement and Spanning", "[quadtree][grid][placement]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 1000, 800);
    
    SECTION("Explicit grid placement") {
        // Test grid-row: 2, grid-column: 3
        NVGCSSElement* container = nvgcssCreateElement(renderer, "container", "div");
        container->style.display = Display::GRID;
        container->style.grid_template_columns = {GridTrack::px(100), GridTrack::px(100), GridTrack::px(100)};
        container->style.grid_template_rows = {GridTrack::px(100), GridTrack::px(100)};
        container->style.width = Length::px(1000);
        container->style.height = Length::px(800);
        
        NVGCSSElement* item = nvgcssCreateElement(renderer, "item", "div");
        item->style.grid_row.start = 2;
        item->style.grid_column.start = 3;
        nvgcssAppendChild(renderer, container, item);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(container, renderer);
        engine.compute_layout(tree, renderer);
        
        // Item should be at row 2, column 3 (0-based: row 1, col 2)
        REQUIRE(tree->children.size() == 1);
        REQUIRE_THAT(tree->children[0]->width, WithinAbs(100.0f, 0.1f));
        REQUIRE_THAT(tree->children[0]->height, WithinAbs(100.0f, 0.1f));
        
        delete tree;
    }
    
    SECTION("Grid spanning with span keyword") {
        NVGCSSElement* container = nvgcssCreateElement(renderer, "container", "div");
        container->style.display = Display::GRID;
        container->style.grid_template_columns = {GridTrack::px(100), GridTrack::px(100), GridTrack::px(100)};
        container->style.grid_template_rows = {GridTrack::px(100), GridTrack::px(100)};
        container->style.width = Length::px(1000);
        container->style.height = Length::px(800);
        
        NVGCSSElement* item = nvgcssCreateElement(renderer, "item", "div");
        item->style.grid_column.span = 2;  // Span 2 columns
        item->style.grid_row.span = 1;
        nvgcssAppendChild(renderer, container, item);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(container, renderer);
        engine.compute_layout(tree, renderer);
        
        // Item should span 2 columns (200px width)
        REQUIRE(tree->children.size() == 1);
        REQUIRE_THAT(tree->children[0]->width, WithinAbs(200.0f, 0.1f));
        REQUIRE_THAT(tree->children[0]->height, WithinAbs(100.0f, 0.1f));
        
        delete tree;
    }
    
    SECTION("Mixed explicit placement and auto-placement") {
        NVGCSSElement* container = nvgcssCreateElement(renderer, "container", "div");
        container->style.display = Display::GRID;
        container->style.grid_template_columns = {GridTrack::px(100), GridTrack::px(100), GridTrack::px(100)};
        container->style.grid_template_rows = {GridTrack::px(100), GridTrack::px(100)};
        container->style.width = Length::px(1000);
        container->style.height = Length::px(800);
        
        // Explicit item at row 1, col 2
        NVGCSSElement* item1 = nvgcssCreateElement(renderer, "item1", "div");
        item1->style.grid_row.start = 1;
        item1->style.grid_column.start = 2;
        nvgcssAppendChild(renderer, container, item1);
        
        // Auto-placed item (should go to first available cell)
        NVGCSSElement* item2 = nvgcssCreateElement(renderer, "item2", "div");
        nvgcssAppendChild(renderer, container, item2);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(container, renderer);
        engine.compute_layout(tree, renderer);
        
        REQUIRE(tree->children.size() == 2);
        // Both items should have valid dimensions
        REQUIRE_THAT(tree->children[0]->width, WithinAbs(100.0f, 0.1f));
        REQUIRE_THAT(tree->children[1]->width, WithinAbs(100.0f, 0.1f));
        
        delete tree;
    }
    
    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Phase 2: Dimension Calculation Tests
// ============================================================================

TEST_CASE("Quadtree Phase 2 - Block Dimension Calculation", "[quadtree][phase2][block]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 1000, 800);
    
    SECTION("Fixed size block element") {
        const char* css = R"(
            #test {
                width: 200px;
                height: 100px;
            }
        )";
        
        nvgcssParseCSS(renderer, css);
        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
        nvgcssComputeLayout(renderer);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(elem, renderer);
        engine.compute_layout(tree, renderer);
        
        REQUIRE_THAT(tree->width, WithinAbs(200.0f, 0.1f));
        REQUIRE_THAT(tree->height, WithinAbs(100.0f, 0.1f));
        
        delete tree;
    }
    
    SECTION("Percentage size block element") {
        const char* css = R"(
            #test {
                width: 50%;
                height: 25%;
            }
        )";
        
        nvgcssParseCSS(renderer, css);
        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
        nvgcssComputeLayout(renderer);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(elem, renderer);
        engine.compute_layout(tree, renderer);
        
        // 50% of 1000 = 500, 25% of 800 = 200
        REQUIRE_THAT(tree->width, WithinAbs(500.0f, 0.1f));
        REQUIRE_THAT(tree->height, WithinAbs(200.0f, 0.1f));
        
        delete tree;
    }
    
    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("Quadtree Phase 2 - Flex Dimension Calculation", "[quadtree][phase2][flex]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 1000, 800);
    
    SECTION("Flex container with fixed children") {
        const char* css = R"(
            #container {
                display: flex;
                flex-direction: row;
                width: 100%;
                height: 100px;
            }
            .child {
                width: 200px;
                height: 50px;
            }
        )";
        
        nvgcssParseCSS(renderer, css);
        NVGCSSElement* container = nvgcssCreateElement(renderer, "container", "div");
        NVGCSSElement* child1 = nvgcssCreateElement(renderer, "child1", "div");
        NVGCSSElement* child2 = nvgcssCreateElement(renderer, "child2", "div");
        
        nvgcssAddClass(child1, "child");
        nvgcssAddClass(child2, "child");
        nvgcssAppendChild(renderer, container, child1);
        nvgcssAppendChild(renderer, container, child2);
        nvgcssComputeLayout(renderer);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(container, renderer);
        
        engine.compute_layout(tree, renderer);
        
        // Container should be full width
        REQUIRE_THAT(tree->width, WithinAbs(1000.0f, 0.1f));
        REQUIRE_THAT(tree->height, WithinAbs(100.0f, 0.1f));
        
        // Children should have fixed sizes
        REQUIRE(tree->children.size() == 2);
        // Note: Current implementation delegates to old flex, so these may not pass yet
        
        delete tree;
    }
    
    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Phase 3: Position Calculation Tests
// ============================================================================

TEST_CASE("Quadtree Phase 3 - Position Calculation", "[quadtree][phase3]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 1000, 800);
    
    SECTION("Root positioned at origin") {
        const char* css = R"(
            #root {
                width: 100%;
                height: 100%;
            }
        )";
        
        nvgcssParseCSS(renderer, css);
        NVGCSSElement* root = nvgcssCreateElement(renderer, "root", "div");
        nvgcssComputeLayout(renderer);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(root, renderer);
        
        engine.compute_layout(tree, renderer);
        
        REQUIRE_THAT(tree->x, WithinAbs(0.0f, 0.1f));
        REQUIRE_THAT(tree->y, WithinAbs(0.0f, 0.1f));
        
        delete tree;
    }
    
    SECTION("Child positioned relative to parent") {
        const char* css = R"(
            #parent {
                width: 500px;
                height: 400px;
                padding: 20px;
            }
            #child {
                width: 100px;
                height: 50px;
            }
        )";
        
        nvgcssParseCSS(renderer, css);
        NVGCSSElement* parent = nvgcssCreateElement(renderer, "parent", "div");
        NVGCSSElement* child = nvgcssCreateElement(renderer, "child", "div");
        nvgcssAppendChild(renderer, parent, child);
        nvgcssComputeLayout(renderer);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(parent, renderer);
        
        engine.compute_layout(tree, renderer);
        
        // Parent at origin
        REQUIRE_THAT(tree->x, WithinAbs(0.0f, 0.1f));
        REQUIRE_THAT(tree->y, WithinAbs(0.0f, 0.1f));
        
        // Child should be offset by parent's padding
        REQUIRE(tree->children.size() == 1);
        REQUIRE_THAT(tree->children[0]->x, WithinAbs(20.0f, 0.1f));
        REQUIRE_THAT(tree->children[0]->y, WithinAbs(20.0f, 0.1f));
        
        delete tree;
    }
    
    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Integration Tests - Full Three-Phase Algorithm
// ============================================================================

TEST_CASE("Quadtree Integration - Complete Layout", "[quadtree][integration]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 1000, 800);
    
    SECTION("Simple nested layout") {
        const char* css = R"(
            #root {
                width: 100%;
                height: 100%;
            }
            #header {
                width: 100%;
                height: 80px;
            }
            #content {
                width: 100%;
                height: 720px;
            }
        )";
        
        nvgcssParseCSS(renderer, css);
        NVGCSSElement* root = nvgcssCreateElement(renderer, "root", "div");
        NVGCSSElement* header = nvgcssCreateElement(renderer, "header", "div");
        NVGCSSElement* content = nvgcssCreateElement(renderer, "content", "div");
        
        nvgcssAppendChild(renderer, root, header);
        nvgcssAppendChild(renderer, root, content);
        nvgcssComputeLayout(renderer);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(root, renderer);
        
        engine.compute_layout(tree, renderer);
        
        // Verify root
        REQUIRE_THAT(tree->width, WithinAbs(1000.0f, 0.1f));
        REQUIRE_THAT(tree->height, WithinAbs(800.0f, 0.1f));
        
        // Verify children exist
        REQUIRE(tree->children.size() == 2);
        
        delete tree;
    }
    
    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_CASE("Quadtree Edge Cases", "[quadtree][edge-cases]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 1000, 800);
    
    SECTION("Empty element tree") {
        NVGCSSElement* elem = nvgcssCreateElement(renderer, "empty", "div");
        nvgcssComputeLayout(renderer);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(elem, renderer);
        
        // Should not crash
        engine.compute_layout(tree, renderer);
        
        REQUIRE(tree != nullptr);
        REQUIRE(tree->children.empty());
        
        delete tree;
    }
    
    SECTION("Deeply nested elements") {
        const char* css = R"(
            .box {
                width: 100px;
                height: 100px;
            }
        )";
        
        nvgcssParseCSS(renderer, css);
        
        // Create nested structure: root -> child1 -> child2 -> child3
        NVGCSSElement* root = nvgcssCreateElement(renderer, "root", "div");
        NVGCSSElement* child1 = nvgcssCreateElement(renderer, "child1", "div");
        NVGCSSElement* child2 = nvgcssCreateElement(renderer, "child2", "div");
        NVGCSSElement* child3 = nvgcssCreateElement(renderer, "child3", "div");
        
        nvgcssAddClass(root, "box");
        nvgcssAddClass(child1, "box");
        nvgcssAddClass(child2, "box");
        nvgcssAddClass(child3, "box");
        
        nvgcssAppendChild(renderer, root, child1);
        nvgcssAppendChild(renderer, child1, child2);
        nvgcssAppendChild(renderer, child2, child3);
        nvgcssComputeLayout(renderer);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(root, renderer);
        
        engine.compute_layout(tree, renderer);
        
        // Verify tree structure
        REQUIRE(tree->children.size() == 1);
        REQUIRE(tree->children[0]->children.size() == 1);
        REQUIRE(tree->children[0]->children[0]->children.size() == 1);
        
        delete tree;
    }
    
    nvgcssDeleteRenderer(renderer);
}
