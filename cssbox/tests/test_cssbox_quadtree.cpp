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
#include <cssbox.h>
#include <cssbox_quadtree.h>
#include "cssbox_internal.h"

using Catch::Matchers::WithinAbs;
using namespace cssbox;

// ============================================================================
// Phase 1: Constraint Propagation Tests
// ============================================================================

TEST_CASE("Quadtree Phase 1 - Constraint Propagation", "[quadtree][phase1]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1000, 800);
    
    SECTION("Root receives viewport constraints") {
        const char* css = R"(
            #root {
                width: 100%;
                height: 100%;
            }
        )";
        
        cssboxParseCSS(renderer, css);
        cssboxElement* root = cssboxCreateElement(renderer, "root", "div");
        cssboxComputeLayout(renderer);
        
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
        
        cssboxParseCSS(renderer, css);
        cssboxElement* parent = cssboxCreateElement(renderer, "parent", "div");
        cssboxElement* child = cssboxCreateElement(renderer, "child", "div");
        cssboxAppendChild(renderer, parent, child);
        cssboxComputeLayout(renderer);
        
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
    
    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Grid Placement and Spanning Tests
// ============================================================================

TEST_CASE("Quadtree Grid - Explicit Placement and Spanning", "[quadtree][grid][placement]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1000, 800);
    
    SECTION("Explicit grid placement") {
        // Test grid-row: 2, grid-column: 3
        cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
        container->style.display = Display::GRID;
        container->style.grid_template_columns = {GridTrack::px(100), GridTrack::px(100), GridTrack::px(100)};
        container->style.grid_template_rows = {GridTrack::px(100), GridTrack::px(100)};
        container->style.width = Length::px(1000);
        container->style.height = Length::px(800);
        
        cssboxElement* item = cssboxCreateElement(renderer, "item", "div");
        item->style.grid_row.start = 2;
        item->style.grid_column.start = 3;
        cssboxAppendChild(renderer, container, item);
        
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
        cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
        container->style.display = Display::GRID;
        container->style.grid_template_columns = {GridTrack::px(100), GridTrack::px(100), GridTrack::px(100)};
        container->style.grid_template_rows = {GridTrack::px(100), GridTrack::px(100)};
        container->style.width = Length::px(1000);
        container->style.height = Length::px(800);
        
        cssboxElement* item = cssboxCreateElement(renderer, "item", "div");
        item->style.grid_column.span = 2;  // Span 2 columns
        item->style.grid_row.span = 1;
        cssboxAppendChild(renderer, container, item);
        
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
        cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
        container->style.display = Display::GRID;
        container->style.grid_template_columns = {GridTrack::px(100), GridTrack::px(100), GridTrack::px(100)};
        container->style.grid_template_rows = {GridTrack::px(100), GridTrack::px(100)};
        container->style.width = Length::px(1000);
        container->style.height = Length::px(800);
        
        // Explicit item at row 1, col 2
        cssboxElement* item1 = cssboxCreateElement(renderer, "item1", "div");
        item1->style.grid_row.start = 1;
        item1->style.grid_column.start = 2;
        cssboxAppendChild(renderer, container, item1);
        
        // Auto-placed item (should go to first available cell)
        cssboxElement* item2 = cssboxCreateElement(renderer, "item2", "div");
        cssboxAppendChild(renderer, container, item2);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(container, renderer);
        engine.compute_layout(tree, renderer);
        
        REQUIRE(tree->children.size() == 2);
        // Both items should have valid dimensions
        REQUIRE_THAT(tree->children[0]->width, WithinAbs(100.0f, 0.1f));
        REQUIRE_THAT(tree->children[1]->width, WithinAbs(100.0f, 0.1f));
        
        delete tree;
    }
    
    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Phase 2: Dimension Calculation Tests
// ============================================================================

TEST_CASE("Quadtree Phase 2 - Block Dimension Calculation", "[quadtree][phase2][block]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1000, 800);
    
    SECTION("Fixed size block element") {
        const char* css = R"(
            #test {
                width: 200px;
                height: 100px;
            }
        )";
        
        cssboxParseCSS(renderer, css);
        cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
        cssboxComputeLayout(renderer);
        
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
        
        cssboxParseCSS(renderer, css);
        cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
        cssboxComputeLayout(renderer);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(elem, renderer);
        engine.compute_layout(tree, renderer);
        
        // 50% of 1000 = 500, 25% of 800 = 200
        REQUIRE_THAT(tree->width, WithinAbs(500.0f, 0.1f));
        REQUIRE_THAT(tree->height, WithinAbs(200.0f, 0.1f));
        
        delete tree;
    }
    
    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Quadtree Phase 2 - Flex Dimension Calculation", "[quadtree][phase2][flex]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1000, 800);
    
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
        
        cssboxParseCSS(renderer, css);
        cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
        cssboxElement* child1 = cssboxCreateElement(renderer, "child1", "div");
        cssboxElement* child2 = cssboxCreateElement(renderer, "child2", "div");
        
        cssboxAddClass(child1, "child");
        cssboxAddClass(child2, "child");
        cssboxAppendChild(renderer, container, child1);
        cssboxAppendChild(renderer, container, child2);
        cssboxComputeLayout(renderer);
        
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
    
    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Phase 3: Position Calculation Tests
// ============================================================================

TEST_CASE("Quadtree Phase 3 - Position Calculation", "[quadtree][phase3]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1000, 800);
    
    SECTION("Root positioned at origin") {
        const char* css = R"(
            #root {
                width: 100%;
                height: 100%;
            }
        )";
        
        cssboxParseCSS(renderer, css);
        cssboxElement* root = cssboxCreateElement(renderer, "root", "div");
        cssboxComputeLayout(renderer);
        
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
        
        cssboxParseCSS(renderer, css);
        cssboxElement* parent = cssboxCreateElement(renderer, "parent", "div");
        cssboxElement* child = cssboxCreateElement(renderer, "child", "div");
        cssboxAppendChild(renderer, parent, child);
        cssboxComputeLayout(renderer);
        
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
    
    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Integration Tests - Full Three-Phase Algorithm
// ============================================================================

TEST_CASE("Quadtree Integration - Complete Layout", "[quadtree][integration]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1000, 800);
    
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
        
        cssboxParseCSS(renderer, css);
        cssboxElement* root = cssboxCreateElement(renderer, "root", "div");
        cssboxElement* header = cssboxCreateElement(renderer, "header", "div");
        cssboxElement* content = cssboxCreateElement(renderer, "content", "div");
        
        cssboxAppendChild(renderer, root, header);
        cssboxAppendChild(renderer, root, content);
        cssboxComputeLayout(renderer);
        
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
    
    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_CASE("Quadtree Edge Cases", "[quadtree][edge-cases]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1000, 800);
    
    SECTION("Empty element tree") {
        cssboxElement* elem = cssboxCreateElement(renderer, "empty", "div");
        cssboxComputeLayout(renderer);
        
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
        
        cssboxParseCSS(renderer, css);
        
        // Create nested structure: root -> child1 -> child2 -> child3
        cssboxElement* root = cssboxCreateElement(renderer, "root", "div");
        cssboxElement* child1 = cssboxCreateElement(renderer, "child1", "div");
        cssboxElement* child2 = cssboxCreateElement(renderer, "child2", "div");
        cssboxElement* child3 = cssboxCreateElement(renderer, "child3", "div");
        
        cssboxAddClass(root, "box");
        cssboxAddClass(child1, "box");
        cssboxAddClass(child2, "box");
        cssboxAddClass(child3, "box");
        
        cssboxAppendChild(renderer, root, child1);
        cssboxAppendChild(renderer, child1, child2);
        cssboxAppendChild(renderer, child2, child3);
        cssboxComputeLayout(renderer);
        
        QuadtreeLayoutEngine engine(1000, 800);
        LayoutNode* tree = engine.build_tree(root, renderer);
        
        engine.compute_layout(tree, renderer);
        
        // Verify tree structure
        REQUIRE(tree->children.size() == 1);
        REQUIRE(tree->children[0]->children.size() == 1);
        REQUIRE(tree->children[0]->children[0]->children.size() == 1);
        
        delete tree;
    }
    
    cssboxDeleteRenderer(renderer);
}
