/*
 * Unit tests for Quadtree Layout Engine
 * 
 * Tests the three-phase algorithm with simple cases to identify issues.
 */

#include <cssbox.h>
#include <cssbox_internal.h>
#include <cssbox_quadtree.h>
#include <iostream>
#include <cassert>
#include <cmath>

using namespace cssbox;

// Mock renderer for testing (no actual rendering)
class MockRenderer {
public:
    cssboxRenderer* renderer = nullptr;
    
    MockRenderer() {
        // Create minimal renderer without OpenGL context
        renderer = new cssboxRenderer();
        renderer->vg = nullptr; // No actual NanoVG context needed for layout tests
    }
    
    ~MockRenderer() {
        delete renderer;
    }
};

// Helper to check if values are approximately equal
bool approx_equal(float a, float b, float epsilon = 0.1f) {
    return std::abs(a - b) < epsilon;
}

// Test 1: Simple single element
void test_single_element() {
    std::cout << "\n=== Test 1: Single Element ===" << std::endl;
    
    MockRenderer mock;
    QuadtreeLayoutEngine engine(1000, 800);
    
    // Create root element
    cssboxElement* root = new cssboxElement();
    root->id = "root";
    root->computed_style.display = DisplayType::BLOCK;
    root->computed_style.width = CSSLength{100.0f, CSSUnit::PERCENT};
    root->computed_style.height = CSSLength{100.0f, CSSUnit::PERCENT};
    
    // Build and compute layout
    LayoutNode* tree = engine.build_tree(root, mock.renderer);
    engine.compute_layout(tree, mock.renderer);
    
    std::cout << "Root constraints: " << tree->constraints.available_width 
              << "x" << tree->constraints.available_height << std::endl;
    std::cout << "Root dimensions: " << tree->width << "x" << tree->height << std::endl;
    
    // Verify
    assert(approx_equal(tree->width, 1000.0f));
    assert(approx_equal(tree->height, 800.0f));
    
    delete tree;
    delete root;
    std::cout << "✓ Test 1 passed" << std::endl;
}

// Test 2: Parent with fixed-size child
void test_parent_child_fixed() {
    std::cout << "\n=== Test 2: Parent with Fixed Child ===" << std::endl;
    
    MockRenderer mock;
    QuadtreeLayoutEngine engine(1000, 800);
    
    // Create root
    cssboxElement* root = new cssboxElement();
    root->id = "root";
    root->computed_style.display = DisplayType::BLOCK;
    root->computed_style.width = CSSLength{100.0f, CSSUnit::PERCENT};
    root->computed_style.height = CSSLength{100.0f, CSSUnit::PERCENT};
    
    // Create child with fixed size
    cssboxElement* child = new cssboxElement();
    child->id = "child";
    child->computed_style.display = DisplayType::BLOCK;
    child->computed_style.width = CSSLength{200.0f, CSSUnit::PX};
    child->computed_style.height = CSSLength{100.0f, CSSUnit::PX};
    child->parent = root;
    root->children.push_back(child);
    
    // Build and compute layout
    LayoutNode* tree = engine.build_tree(root, mock.renderer);
    engine.compute_layout(tree, mock.renderer);
    
    std::cout << "Root: " << tree->width << "x" << tree->height << std::endl;
    std::cout << "Child constraints: " << tree->children[0]->constraints.available_width 
              << "x" << tree->children[0]->constraints.available_height << std::endl;
    std::cout << "Child: " << tree->children[0]->width << "x" << tree->children[0]->height << std::endl;
    
    // Verify child received parent constraints
    assert(approx_equal(tree->children[0]->constraints.available_width, 1000.0f));
    assert(approx_equal(tree->children[0]->constraints.available_height, 800.0f));
    
    // Verify child dimensions
    assert(approx_equal(tree->children[0]->width, 200.0f));
    assert(approx_equal(tree->children[0]->height, 100.0f));
    
    delete tree;
    delete child;
    delete root;
    std::cout << "✓ Test 2 passed" << std::endl;
}

// Test 3: Flex row with two children
void test_flex_row_simple() {
    std::cout << "\n=== Test 3: Flex Row (Simple) ===" << std::endl;
    
    MockRenderer mock;
    QuadtreeLayoutEngine engine(1000, 800);
    
    // Create flex container
    cssboxElement* container = new cssboxElement();
    container->id = "container";
    container->computed_style.display = DisplayType::FLEX;
    container->computed_style.flex_direction = FlexDirection::ROW;
    container->computed_style.width = CSSLength{100.0f, CSSUnit::PERCENT};
    container->computed_style.height = CSSLength{100.0f, CSSUnit::PERCENT};
    
    // Create two children with flex-grow
    cssboxElement* child1 = new cssboxElement();
    child1->id = "child1";
    child1->computed_style.display = DisplayType::BLOCK;
    child1->computed_style.flex_grow = 1.0f;
    child1->parent = container;
    
    cssboxElement* child2 = new cssboxElement();
    child2->id = "child2";
    child2->computed_style.display = DisplayType::BLOCK;
    child2->computed_style.flex_grow = 1.0f;
    child2->parent = container;
    
    container->children.push_back(child1);
    container->children.push_back(child2);
    
    // Build and compute layout
    LayoutNode* tree = engine.build_tree(container, mock.renderer);
    engine.compute_layout(tree, mock.renderer);
    
    std::cout << "Container: " << tree->width << "x" << tree->height << std::endl;
    std::cout << "Child1 constraints: " << tree->children[0]->constraints.available_width 
              << "x" << tree->children[0]->constraints.available_height << std::endl;
    std::cout << "Child1: " << tree->children[0]->width << "x" << tree->children[0]->height << std::endl;
    std::cout << "Child2: " << tree->children[1]->width << "x" << tree->children[1]->height << std::endl;
    
    // Verify children received container constraints
    assert(approx_equal(tree->children[0]->constraints.available_width, 1000.0f));
    assert(approx_equal(tree->children[0]->constraints.available_height, 800.0f));
    
    // Expected: Each child should get 500px width (50% of 1000)
    std::cout << "\nExpected child widths: ~500px each" << std::endl;
    std::cout << "Actual child1 width: " << tree->children[0]->width << std::endl;
    std::cout << "Actual child2 width: " << tree->children[1]->width << std::endl;
    
    // This will likely fail, showing the issue
    if (!approx_equal(tree->children[0]->width, 500.0f)) {
        std::cout << "⚠ Child1 width incorrect (expected ~500, got " 
                  << tree->children[0]->width << ")" << std::endl;
    }
    if (!approx_equal(tree->children[1]->width, 500.0f)) {
        std::cout << "⚠ Child2 width incorrect (expected ~500, got " 
                  << tree->children[1]->width << ")" << std::endl;
    }
    
    delete tree;
    delete child2;
    delete child1;
    delete container;
    std::cout << "✓ Test 3 completed (issues identified)" << std::endl;
}

// Test 4: Nested flex containers
void test_nested_flex() {
    std::cout << "\n=== Test 4: Nested Flex Containers ===" << std::endl;
    
    MockRenderer mock;
    QuadtreeLayoutEngine engine(1000, 800);
    
    // Outer container (column)
    cssboxElement* outer = new cssboxElement();
    outer->id = "outer";
    outer->computed_style.display = DisplayType::FLEX;
    outer->computed_style.flex_direction = FlexDirection::COLUMN;
    outer->computed_style.width = CSSLength{100.0f, CSSUnit::PERCENT};
    outer->computed_style.height = CSSLength{100.0f, CSSUnit::PERCENT};
    
    // Inner container (row)
    cssboxElement* inner = new cssboxElement();
    inner->id = "inner";
    inner->computed_style.display = DisplayType::FLEX;
    inner->computed_style.flex_direction = FlexDirection::ROW;
    inner->computed_style.flex_grow = 1.0f;
    inner->parent = outer;
    
    // Two children in inner
    cssboxElement* child1 = new cssboxElement();
    child1->id = "child1";
    child1->computed_style.display = DisplayType::BLOCK;
    child1->computed_style.flex_grow = 1.0f;
    child1->parent = inner;
    
    cssboxElement* child2 = new cssboxElement();
    child2->id = "child2";
    child2->computed_style.display = DisplayType::BLOCK;
    child2->computed_style.flex_grow = 1.0f;
    child2->parent = inner;
    
    inner->children.push_back(child1);
    inner->children.push_back(child2);
    outer->children.push_back(inner);
    
    // Build and compute layout
    LayoutNode* tree = engine.build_tree(outer, mock.renderer);
    engine.compute_layout(tree, mock.renderer);
    
    std::cout << "Outer: " << tree->width << "x" << tree->height << std::endl;
    std::cout << "Inner constraints: " << tree->children[0]->constraints.available_width 
              << "x" << tree->children[0]->constraints.available_height << std::endl;
    std::cout << "Inner: " << tree->children[0]->width << "x" << tree->children[0]->height << std::endl;
    std::cout << "Child1: " << tree->children[0]->children[0]->width 
              << "x" << tree->children[0]->children[0]->height << std::endl;
    std::cout << "Child2: " << tree->children[0]->children[1]->width 
              << "x" << tree->children[0]->children[1]->height << std::endl;
    
    std::cout << "\nExpected:" << std::endl;
    std::cout << "  Inner: 1000x800" << std::endl;
    std::cout << "  Child1: 500x800" << std::endl;
    std::cout << "  Child2: 500x800" << std::endl;
    
    // Check if inner received correct constraints
    if (!approx_equal(tree->children[0]->constraints.available_width, 1000.0f)) {
        std::cout << "⚠ Inner didn't receive correct width constraint" << std::endl;
    }
    if (!approx_equal(tree->children[0]->constraints.available_height, 800.0f)) {
        std::cout << "⚠ Inner didn't receive correct height constraint" << std::endl;
    }
    
    delete tree;
    delete child2;
    delete child1;
    delete inner;
    delete outer;
    std::cout << "✓ Test 4 completed (issues identified)" << std::endl;
}

int main() {
    std::cout << "Quadtree Layout Engine - Unit Tests" << std::endl;
    std::cout << "====================================" << std::endl;
    
    try {
        test_single_element();
        test_parent_child_fixed();
        test_flex_row_simple();
        test_nested_flex();
        
        std::cout << "\n=== Summary ===" << std::endl;
        std::cout << "Tests completed. Check output for identified issues." << std::endl;
        std::cout << "\nKey findings:" << std::endl;
        std::cout << "1. Constraint propagation (Phase 1) - Check if working" << std::endl;
        std::cout << "2. Flex dimension calculation (Phase 2) - Likely incorrect" << std::endl;
        std::cout << "3. Nested flex - Shows if parent constraints reach grandchildren" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
