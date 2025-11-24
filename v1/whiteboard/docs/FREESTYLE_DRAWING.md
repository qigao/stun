# Freestyle Drawing in Whiteboard

## Overview

The whiteboard supports **freestyle drawing** (also called freehand drawing or pen drawing) through the **Pen Tool**. This allows users to draw smooth, natural strokes with their mouse or stylus.

```
┌─────────────────────────────────────────────────────────────────┐
│                    FREESTYLE DRAWING SYSTEM                      │
└─────────────────────────────────────────────────────────────────┘

User Input (Mouse/Stylus)
    ↓
CanvasController::handle_pen_tool_*()
    ↓
Creates Stroke with Tool::Pen
    ↓
Stores points as user drags
    ↓
WhiteboardDocument::add_stroke()
    ↓
CanvasView::draw_strokes()
    ↓
NanoVG renders smooth path
    ↓
Screen
```

## Architecture

### 1. Tool Enum
```cpp
// whiteboard/include/whiteboard/types.h
enum class Tool { 
    Select, 
    Pan, 
    Pen,        // ← Freestyle drawing tool
    Text, 
    Sticky, 
    Rectangle, 
    Diamond, 
    Circle, 
    Line, 
    Arrow, 
    Image, 
    SVGShape 
};
```

### 2. Stroke Structure
```cpp
struct Stroke {
    std::vector<Point> points;  // ← Stores drawing path
    nanogui::Color color;
    float width;
    Tool tool;                  // ← Set to Tool::Pen
    // ... other properties
};
```

### 3. Point Structure
```cpp
struct Point {
    float x;
    float y;
    Point(float xx = 0.f, float yy = 0.f) : x(xx), y(yy) {}
};
```

## Drawing Flow

### Step 1: Mouse Down (Start Drawing)
```cpp
// whiteboard/src/canvas/canvas_controller.cpp

void CanvasController::handle_pen_tool_down(const nanogui::Vector2f &pos) {
    // Start drawing a new pen stroke
    m_mode = Mode::Drawing;
    
    // Create new stroke with current properties
    m_temp_stroke = Stroke();
    m_temp_stroke.tool = Tool::Pen;
    m_temp_stroke.color = m_document->get_stroke_color();
    m_temp_stroke.width = m_document->get_stroke_width();
    
    // Add first point
    Point snapped = snap_point(Point(pos.x(), pos.y()));
    m_temp_stroke.points.push_back(snapped);
    
    // Show temporary stroke in view
    m_view->set_current_stroke(m_temp_stroke);
}
```

### Step 2: Mouse Drag (Continue Drawing)
```cpp
void CanvasController::handle_pen_tool_drag(const nanogui::Vector2f &pos) {
    // Add point to current stroke (with snapping)
    Point snapped = snap_point(Point(pos.x(), pos.y()));
    m_temp_stroke.points.push_back(snapped);
    
    // Update view with new point
    m_view->set_current_stroke(m_temp_stroke);
}
```

### Step 3: Mouse Up (Finish Drawing)
```cpp
void CanvasController::handle_pen_tool_up(const nanogui::Vector2f &pos) {
    // Finalize the stroke
    if (!m_temp_stroke.points.empty()) {
        m_document->add_stroke(m_temp_stroke);
    }
    
    // Clear temporary stroke from view
    m_view->clear_current_stroke();
    m_mode = Mode::None;
}
```

## Rendering

### Rendering Pen Strokes
```cpp
// whiteboard/src/canvas/canvas_view.cpp

void CanvasView::draw_stroke(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.tool == Tool::Pen) {
        // Draw freehand path
        nvgBeginPath(ctx);
        
        for (size_t i = 0; i < stroke.points.size(); ++i) {
            nanogui::Vector2f screen_pos = canvas_to_global(to_vec(stroke.points[i]));
            
            if (i == 0) {
                nvgMoveTo(ctx, screen_pos.x(), screen_pos.y());
            } else {
                nvgLineTo(ctx, screen_pos.x(), screen_pos.y());
            }
        }
        
        // Apply stroke properties
        nvgStrokeColor(ctx, nvgRGBA(
            stroke.color.r() * 255, 
            stroke.color.g() * 255,
            stroke.color.b() * 255, 
            stroke.color.w() * 255
        ));
        nvgStrokeWidth(ctx, stroke.width * zoom);
        nvgStroke(ctx);
    }
}
```

## Features

### 1. Smooth Path Rendering
- Uses NanoVG for anti-aliased rendering
- Connects points with straight lines
- Appears smooth due to high point density

### 2. Customizable Properties
```cpp
// User can customize:
- Color (RGB + Alpha)
- Width (stroke thickness)
- Opacity (0.0 to 1.0)
- Stroke style (Solid, Dashed, Dotted)
```

### 3. Snapping Support
```cpp
Point CanvasController::snap_point(const Point &p) {
    if (!m_document->get_snap_enabled()) {
        return p;
    }
    
    float grid_size = m_document->get_grid_size();
    return Point(
        std::round(p.x / grid_size) * grid_size,
        std::round(p.y / grid_size) * grid_size
    );
}
```

### 4. Real-time Preview
- Temporary stroke shown while drawing
- Updates on every mouse move
- Finalized when mouse released

## User Workflow

### Basic Drawing
```
1. Select Pen Tool
   - Click pen icon in toolbar
   - Or press 'P' key

2. Choose Color and Width
   - Select color from color picker
   - Adjust width slider

3. Draw
   - Click and drag on canvas
   - Path follows mouse/stylus
   - Release to finish stroke

4. Result
   - Stroke added to document
   - Can be selected, moved, deleted
   - Saved with document
```

### Advanced Features

#### Drawing with Snapping
```
1. Enable grid snapping
2. Draw with pen tool
3. Points snap to grid intersections
4. Creates precise, aligned drawings
```

#### Drawing with Opacity
```
1. Select pen tool
2. Adjust opacity slider (0-100%)
3. Draw semi-transparent strokes
4. Useful for sketching, annotations
```

#### Drawing with Dash Styles
```
1. Select pen tool
2. Choose stroke style:
   - Solid (default)
   - Dashed
   - Dotted
3. Draw with selected style
```

## Data Storage

### In Memory
```cpp
// WhiteboardDocument stores all strokes
class WhiteboardDocument {
    std::vector<Stroke> m_strokes;  // ← Pen strokes stored here
};
```

### In File (Not DDF)
Pen strokes are stored in the **WhiteboardDocument**, not in DDF format.

**Note**: Currently, pen strokes are **NOT saved to DDF files**. They exist only in the canvas session.

To save pen drawings:
1. Export as PNG/JPG (rasterized)
2. Export as SVG (vectorized)
3. Or implement custom save format

## Comparison with Other Tools

| Tool | Points Stored | Rendering | Use Case |
|------|---------------|-----------|----------|
| **Pen** | Many points (path) | Smooth line | Freehand drawing, sketching |
| **Line** | 2 points (start, end) | Straight line | Precise lines |
| **Rectangle** | 2 points (corners) | Rectangle | Boxes, frames |
| **Circle** | 2 points (center, radius) | Circle | Circular shapes |
| **SVGShape** | 1 point (position) | SVG rendering | Complex shapes |

## Performance Considerations

### Point Density
```cpp
// More points = smoother curve but more memory
// Typical pen stroke: 50-500 points

// Optimization: Point simplification
// Could reduce points while maintaining visual quality
// Example: Douglas-Peucker algorithm
```

### Rendering Optimization
```cpp
// Viewport culling
// Only render strokes visible in viewport

void CanvasView::draw_strokes(NVGcontext *ctx) {
    // Calculate visible viewport
    nanogui::Vector2f viewport_min = local_to_canvas(nanogui::Vector2i(0, 0));
    nanogui::Vector2f viewport_max = local_to_canvas(m_size);
    
    for (const auto &stroke : strokes) {
        // Check if stroke bounds intersect viewport
        float min_x, min_y, max_x, max_y;
        stroke.get_bounds(min_x, min_y, max_x, max_y);
        
        bool is_visible = !(
            max_x < viewport_min.x() || 
            min_x > viewport_max.x() ||
            max_y < viewport_min.y() || 
            min_y > viewport_max.y()
        );
        
        if (is_visible) {
            draw_stroke(ctx, stroke);
        }
    }
}
```

## Future Enhancements

### 1. Pressure Sensitivity
```cpp
// Support stylus pressure for variable width
struct Point {
    float x, y;
    float pressure;  // 0.0 to 1.0
};

// Render with varying width based on pressure
for (size_t i = 0; i < stroke.points.size(); ++i) {
    float width = base_width * stroke.points[i].pressure;
    // Draw segment with variable width
}
```

### 2. Stroke Smoothing
```cpp
// Apply Bezier curve smoothing
// Convert point sequence to smooth curves
// Reduces jitter from mouse input
```

### 3. Eraser Tool
```cpp
// Erase parts of pen strokes
// Split strokes at erase points
// Remove erased segments
```

### 4. Brush Styles
```cpp
// Different brush types:
- Pencil (hard edge)
- Marker (soft edge)
- Highlighter (transparent, wide)
- Calligraphy (angle-dependent width)
```

### 5. Save to DDF
```cpp
// Convert pen strokes to DDF paths
// Store in DDF document
// Enable save/load of drawings

{
  "shapes": [
    {
      "type": "path",
      "path_data": "M 10 10 L 20 20 L 30 15 ...",
      "style": {
        "stroke": "#000000",
        "stroke-width": "3"
      }
    }
  ]
}
```

## Code Examples

### Example 1: Simple Pen Drawing
```cpp
// User draws a simple curve
// Points collected: (10,10), (15,12), (20,15), (25,20), (30,25)

Stroke pen_stroke;
pen_stroke.tool = Tool::Pen;
pen_stroke.color = nanogui::Color(0, 0, 0, 1);  // Black
pen_stroke.width = 3.0f;
pen_stroke.points = {
    Point(10, 10),
    Point(15, 12),
    Point(20, 15),
    Point(25, 20),
    Point(30, 25)
};

document->add_stroke(pen_stroke);
```

### Example 2: Drawing with Custom Properties
```cpp
Stroke fancy_stroke;
fancy_stroke.tool = Tool::Pen;
fancy_stroke.color = nanogui::Color(1, 0, 0, 1);  // Red
fancy_stroke.width = 5.0f;
fancy_stroke.opacity = 0.5f;  // 50% transparent
fancy_stroke.stroke_style = StrokeStyle::Dashed;
// ... add points ...
```

## Summary

### ✅ Freestyle Drawing Features

**Supported**:
- ✅ Pen tool for freehand drawing
- ✅ Smooth path rendering with NanoVG
- ✅ Customizable color, width, opacity
- ✅ Stroke styles (solid, dashed, dotted)
- ✅ Real-time preview while drawing
- ✅ Grid snapping support
- ✅ Selection, move, delete operations
- ✅ Viewport culling for performance

**Not Yet Supported**:
- ❌ Pressure sensitivity (stylus)
- ❌ Stroke smoothing algorithms
- ❌ Eraser tool
- ❌ Brush styles
- ❌ Save to DDF format

### Architecture Summary

```
Pen Tool
    ↓
Collects points during drag
    ↓
Creates Stroke with Tool::Pen
    ↓
Stores in WhiteboardDocument
    ↓
Renders with NanoVG
    ↓
Smooth freehand drawing on canvas
```

---

**Last Updated**: October 2025
