# Quadtree-Based Layout Engine Design

## Problem Statement

Current layout engine has issues with nested flex/grid containers:
- Parent dimensions not properly propagated to children
- Multiple layout passes cause performance issues
- Recursive layout computation is complex and error-prone

## Proposed Solution: Spatial Quadtree Layout

### Core Concept

Use a quadtree to represent the spatial hierarchy of UI elements, enabling:
1. **Single-pass layout** - compute all dimensions in one traversal
2. **Spatial queries** - efficiently find elements in regions
3. **Constraint propagation** - parent constraints flow down naturally
4. **Incremental updates** - only recompute affected subtrees

### Quadtree Structure

```cpp
struct LayoutNode {
    // Spatial bounds (computed)
    float x, y, width, height;
    
    // Layout constraints (input from CSS)
    LayoutConstraints constraints;
    
    // Element reference
    NVGCSSElement* element;
    
    // Quadtree structure
    LayoutNode* parent;
    std::vector<LayoutNode*> children;
    
    // Layout type
    enum Type { BLOCK, FLEX, GRID } type;
    
    // Dirty flag for incremental updates
    bool needs_layout;
};

struct LayoutConstraints {
    // Available space from parent
    float available_width;
    float available_height;
    
    // CSS properties
    Length width, height;
    Length min_width, max_width;
    Length min_height, max_height;
    
    // Flex/Grid specific
    FlexProperties flex;
    GridProperties grid;
};
```

### Layout Algorithm

#### Phase 1: Constraint Collection (Top-Down)
```
function collect_constraints(node, parent_constraints):
    // Inherit available space from parent
    node.constraints.available_width = parent_constraints.available_width
    node.constraints.available_height = parent_constraints.available_height
    
    // Apply CSS constraints
    if node.width is percentage:
        node.constraints.width = parent_constraints.available_width * percentage
    
    // Recursively collect for children
    for child in node.children:
        collect_constraints(child, node.constraints)
```

#### Phase 2: Dimension Calculation (Bottom-Up)
```
function calculate_dimensions(node):
    // Calculate children first (bottom-up)
    for child in node.children:
        calculate_dimensions(child)
    
    // Calculate this node's dimensions based on type
    if node.type == FLEX:
        calculate_flex_dimensions(node)
    else if node.type == GRID:
        calculate_grid_dimensions(node)
    else:
        calculate_block_dimensions(node)
    
    // Apply min/max constraints
    node.width = clamp(node.width, node.min_width, node.max_width)
    node.height = clamp(node.height, node.min_height, node.max_height)
```

#### Phase 3: Position Calculation (Top-Down)
```
function calculate_positions(node, parent_x, parent_y):
    // Position this node relative to parent
    node.x = parent_x + offset_x
    node.y = parent_y + offset_y
    
    // Position children based on layout type
    if node.type == FLEX:
        position_flex_children(node)
    else if node.type == GRID:
        position_grid_children(node)
    else:
        position_block_children(node)
    
    // Recursively position children
    for child in node.children:
        calculate_positions(child, node.x, node.y)
```

### Key Improvements

1. **Clear Separation of Concerns**
   - Constraint collection (what space is available)
   - Dimension calculation (how big should elements be)
   - Position calculation (where should elements go)

2. **Proper Parent-Child Communication**
   - Parents provide available space to children
   - Children report their computed size back to parents
   - No circular dependencies

3. **Incremental Updates**
   - Only recompute nodes marked as dirty
   - Dirty flag propagates up to ancestors
   - Spatial queries enable efficient hit testing

4. **Flex/Grid Integration**
   ```cpp
   function calculate_flex_dimensions(node):
       // Collect flex items
       items = node.children
       
       // Calculate main axis size
       available_main = node.constraints.available_width (or height)
       
       // Distribute space using flex-grow/shrink
       for item in items:
           if item.flex_grow > 0:
               item.main_size = base_size + (free_space * flex_grow / total_grow)
       
       // Calculate cross axis size
       for item in items:
           item.cross_size = max(item.min_cross, item.content_cross)
       
       // Set node dimensions
       node.width = sum(item.main_size) + gaps
       node.height = max(item.cross_size)
   ```

### Implementation Plan

#### Step 1: Create Quadtree Structure
- Define `LayoutNode` and `LayoutConstraints` structs
- Build tree from element hierarchy
- Add dirty flag tracking

#### Step 2: Implement Three-Phase Algorithm
- Phase 1: Constraint collection (top-down)
- Phase 2: Dimension calculation (bottom-up)
- Phase 3: Position calculation (top-down)

#### Step 3: Integrate Flex/Grid
- Adapt existing flex algorithm to work with constraints
- Adapt existing grid algorithm to work with constraints
- Ensure proper space distribution

#### Step 4: Incremental Updates
- Track which nodes need relayout
- Propagate dirty flags up tree
- Only recompute affected subtrees

### Benefits

1. **Correctness**: Parent dimensions always available to children
2. **Performance**: Single-pass layout, incremental updates
3. **Maintainability**: Clear algorithm phases, easier to debug
4. **Extensibility**: Easy to add new layout types

### Migration Strategy

1. Keep existing layout code as fallback
2. Implement quadtree layout in parallel
3. Add feature flag to switch between implementations
4. Test thoroughly with existing examples
5. Deprecate old implementation once stable

## Example: Nested Flex Layout

```
Root (flex column, height: 100%)
├─ Header (flex row, height: 80px)
│  ├─ Title (flex-grow: 1)
│  └─ Subtitle (flex-grow: 0)
└─ Main (flex row, flex-grow: 1)
   ├─ Sidebar (flex column, flex-basis: 250px)
   ├─ Center (flex column, flex-grow: 1)
   └─ Right (flex column, flex-grow: 1)
```

### Phase 1: Constraints (Top-Down)
```
Root: available = viewport (1400x900)
  Header: available = 1400x80
    Title: available = ~1200x80
    Subtitle: available = ~200x80
  Main: available = 1400x820 (900 - 80)
    Sidebar: available = 250x820
    Center: available = ~575x820
    Right: available = ~575x820
```

### Phase 2: Dimensions (Bottom-Up)
```
Title: width = 1200, height = 80
Subtitle: width = 200, height = 80
Header: width = 1400, height = 80

Sidebar: width = 250, height = 820
Center: width = 575, height = 820
Right: width = 575, height = 820
Main: width = 1400, height = 820

Root: width = 1400, height = 900
```

### Phase 3: Positions (Top-Down)
```
Root: (0, 0)
  Header: (0, 0)
    Title: (0, 0)
    Subtitle: (1200, 0)
  Main: (0, 80)
    Sidebar: (0, 80)
    Center: (250, 80)
    Right: (825, 80)
```

## Conclusion

The quadtree-based layout engine provides a clean, efficient solution to the nested layout problem. By separating constraint collection, dimension calculation, and position calculation into distinct phases, we ensure parent dimensions are always available to children while maintaining good performance through incremental updates.
