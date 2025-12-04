# Quadtree Layout Engine - Test Plan

## Overview

This document outlines the testing strategy for validating the quadtree layout engine implementation.

## Test Approach

Since the quadtree layout engine integrates with the existing cssbox rendering system, testing is done through:

1. **Visual Demo** (`example_quadtree_layout.cpp`) - Already implemented and running
2. **Log Analysis** - Examining render logs to identify dimension issues
3. **Manual Verification** - Comparing expected vs actual dimensions

## Current Test Results (from example_quadtree_layout)

### Test Case: Nested Flex Layout
**Setup:**
- Viewport: 1400x900
- Root (flex column, 100%)
  - Header (flex row, height: 80px, padding: 20px)
    - Title (flex-grow: 1)
    - Subtitle (flex-grow: 0)
  - Main (flex row, flex-grow: 1, padding: 20px, gap: 20px)
    - Sidebar (flex-basis: 250px)
    - Content (flex-grow: 1)
    - Right (flex-grow: 1)

### Expected Dimensions

```
Root: 1400x900 ✓
Header: 1400x80 → content area: 1360x40 (after padding)
  Title: ~1160x40
  Subtitle: ~200x40
Main: 1400x820 ✓ → content area: 1360x780 (after padding)
  Sidebar: 250x780
  Content: ~535x780
  Right: ~535x780
```

### Actual Dimensions (from logs)

```
Root: 1400x900 ✓ CORRECT
Header: 20x40 ✗ WRONG (should be 1360x40)
Main: 1400x820 ✓ CORRECT
Title: 1240x40 ✓ CORRECT
Subtitle: 1400x40 ⚠ CLOSE (should be ~200x40)
Sidebar: 250x50 ✗ WRONG (should be 250x780)
Content: 535x50 ✗ WRONG (should be 535x780)
Right: 535x50 ✗ WRONG (should be 535x780)
```

## Issue Analysis

### ✅ What's Working

1. **Constraint Propagation (Phase 1)**
   - Root receives viewport constraints: 1400x900 ✓
   - Main receives correct dimensions: 1400x820 ✓
   - Children receive parent constraints ✓

2. **Block Layout**
   - Root dimensions calculated correctly ✓
   - Main dimensions calculated correctly ✓

### ❌ What's Broken

1. **Flex Dimension Calculation (Phase 2)**
   - Header: Width collapsed to 20px (padding only)
   - Sidebar/Content/Right: Heights collapsed to 50px
   - Issue: `calculate_flex_dimensions()` delegates to old algorithm
   - Old algorithm doesn't use quadtree's constraint system

2. **Flex-grow Distribution**
   - Not distributing available space correctly
   - Children not using parent's available dimensions

## Root Cause

The `calculate_flex_dimensions()` function in `cssbox_quadtree.cpp` currently does:

```cpp
void QuadtreeLayoutEngine::calculate_flex_dimensions(LayoutNode* node, cssboxRenderer* renderer) {
    // Delegate to existing flexbox layout
    compute_flexbox_layout(node->element, renderer);
    
    // Copy dimensions back
    node->width = node->element->layout.width;
    node->height = node->element->layout.height;
}
```

**Problem:** The old `compute_flexbox_layout()` doesn't receive or use the constraints from `node->constraints.available_width/height`.

## Solution

Replace the delegation with constraint-based flex algorithm:

```cpp
void QuadtreeLayoutEngine::calculate_flex_dimensions(LayoutNode* node, cssboxRenderer* renderer) {
    bool is_row = (node->element->style.flex_direction == FlexDirection::ROW);
    
    // Use constraints from parent
    float available_main = is_row ? node->constraints.available_width 
                                  : node->constraints.available_height;
    float available_cross = is_row ? node->constraints.available_height 
                                   : node->constraints.available_width;
    
    // Account for padding
    float padding_main = is_row ? (node->element->style.padding_left + node->element->style.padding_right)
                                : (node->element->style.padding_top + node->element->style.padding_bottom);
    float padding_cross = is_row ? (node->element->style.padding_top + node->element->style.padding_bottom)
                                 : (node->element->style.padding_left + node->element->style.padding_right);
    
    available_main -= padding_main;
    available_cross -= padding_cross;
    
    // Collect flex items
    float total_flex_grow = 0;
    float total_base_size = 0;
    
    for (auto* child : node->children) {
        total_flex_grow += child->element->style.flex_grow;
        
        // Calculate base size
        float base = 0;
        if (child->element->style.flex_basis.unit != CSSUnit::AUTO) {
            base = resolve_length(child->element->style.flex_basis, available_main);
        }
        total_base_size += base;
    }
    
    // Distribute remaining space
    float remaining = available_main - total_base_size;
    
    for (auto* child : node->children) {
        if (child->element->style.flex_grow > 0 && total_flex_grow > 0) {
            float extra = remaining * (child->element->style.flex_grow / total_flex_grow);
            float base = resolve_length(child->element->style.flex_basis, available_main);
            
            if (is_row) {
                child->width = base + extra;
                child->height = available_cross;
            } else {
                child->width = available_cross;
                child->height = base + extra;
            }
        }
    }
    
    // Set container dimensions
    node->width = node->constraints.available_width;
    node->height = node->constraints.available_height;
}
```

## Next Steps

1. **Implement constraint-based flex algorithm** in `calculate_flex_dimensions()`
2. **Run example_quadtree_layout** and verify dimensions in logs
3. **Iterate** until all dimensions match expected values
4. **Implement constraint-based grid algorithm** similarly
5. **Add feature flag** to switch between old/new layout engines
6. **Performance testing** to ensure no regression

## Success Criteria

All dimensions in the demo should match expected values:

- [x] Root: 1400x900
- [ ] Header: 1360x40 (content area after padding)
- [x] Main: 1400x820
- [x] Title: ~1160x40
- [ ] Subtitle: ~200x40
- [ ] Sidebar: 250x780
- [ ] Content: ~535x780
- [ ] Right: ~535x780

## Manual Testing Procedure

1. Build and run `example_quadtree_layout`
2. Check console output for dimension logs
3. Compare actual vs expected dimensions
4. Identify which elements have incorrect dimensions
5. Trace back to which phase of the algorithm is failing
6. Fix the issue and re-test

## Automated Testing (Future)

Once the implementation is stable, create proper unit tests:

- Test constraint propagation in isolation
- Test flex dimension calculation with known inputs
- Test nested layouts with multiple levels
- Test edge cases (empty containers, single child, etc.)
- Performance benchmarks vs old layout system
