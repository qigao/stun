# Rendering Architecture Overview

## Summary

The whiteboard application has **two main rendering systems** that work together:

1. **Canvas Rendering** - User-drawn shapes and SVG shapes
2. **DDF Rendering** - Diagram Definition Format documents

Both systems ultimately use the same rendering stack: **LunaSVG → NanoVG → Screen**

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    WHITEBOARD RENDERING STACK                    │
└─────────────────────────────────────────────────────────────────┘

USER INPUT
    ↓
┌───────────────────────────────┬─────────────────────────────────┐
│   CANVAS SYSTEM               │   DDF SYSTEM                    │
│   (User-drawn shapes)         │   (Imported diagrams)           │
└───────────────────────────────┴─────────────────────────────────┘
    ↓                                   ↓
┌───────────────────────────────┬─────────────────────────────────┐
│ CanvasView::draw_strokes()    │ RenderingPipeline::render()     │
│   - Pen, Rectangle, Circle    │   - Build render tree           │
│   - Line, Arrow                │   - Compute styles (CSS)        │
│   - SVGShape (Tool::SVGShape) │   - Compute layout              │
└───────────────────────────────┴─────────────────────────────────┘
    ↓                                   ↓
    │                                   │
    └───────────────┬───────────────────┘
                    ↓
        ┌───────────────────────────┐
        │  svg/SVGRenderer          │
        │  (whiteboard namespace)   │
        │                           │
        │  - load_svg()             │
        │  - render()               │
        └───────────────────────────┘
                    ↓
        ┌───────────────────────────┐
        │  LunaSVG                  │
        │  - Parse SVG XML          │
        │  - Rasterize to bitmap    │
        └───────────────────────────┘
                    ↓
        ┌───────────────────────────┐
        │  NanoVG                   │
        │  - Create image           │
        │  - Apply transforms       │
        │  - Draw to screen         │
        └───────────────────────────┘
                    ↓
              SCREEN PIXELS
```

---

## Component Breakdown

### 1. Canvas System (User Drawing)

**Purpose**: Render user-drawn shapes on the canvas

**Components**:
- `CanvasView` - Main canvas widget
- `CanvasController` - Handles user input
- `WhiteboardDocument` - Stores strokes
- `svg/SVGRenderer` - Renders SVG shapes

**Flow**:
```cpp
// User draws a shape
CanvasView::draw_strokes(ctx)
    ↓
For each stroke:
    ↓
If stroke.tool == Tool::SVGShape:
    ↓
CanvasView::draw_svg_stroke(ctx, stroke)
    ↓
svg_renderer_->load_svg(stroke.svg_data)  // LunaSVG
    ↓
svg_renderer_->render(ctx, document, pos, scale, rotation)  // NanoVG
```

**Files**:
- `whiteboard/src/canvas/canvas_view.cpp`
- `whiteboard/src/svg/svg_renderer.cpp`
- `whiteboard/include/whiteboard/svg/svg_renderer.h`

---

### 2. DDF System (Diagram Import)

**Purpose**: Render imported DDF diagram documents

**Components**:
- `DDFDocument` - DDF document model
- `RenderingPipeline` - DDF rendering engine
- `ShapeLayer`, `ConnectorLayer`, etc. - DDF layers
- `svg/SVGRenderer` - Renders SVG shapes (same as canvas!)

**Flow**:
```cpp
// User imports DDF document
DDFDocument::load_from_json(json_string)
    ↓
RenderingPipeline::render(doc)
    ↓
build_render_tree(doc)
    ↓
For each shape:
    ↓
If shape.type == "svg":
    ↓
render_svg(shape)
    ↓
svg_renderer_->load_svg(shape.svg_data)  // LunaSVG
    ↓
svg_renderer_->render(ctx, document, pos, scale, rotation)  // NanoVG
```

**Files**:
- `whiteboard/src/ddf/ddf_document.cpp`
- `whiteboard/src/ddf/rendering_pipeline.cpp`
- `whiteboard/src/svg/svg_renderer.cpp` (shared!)

---

### 3. SVG Shape Library

**Purpose**: Manage reusable SVG shape templates

**Components**:
- `SVGShapeLibrary` - Shape library manager
- `SVGGenerator` - Generates SVG from `.svgshape` files
- `svg/SVGRenderer` - Renders generated SVG

**Flow**:
```cpp
// User places shape from library
SVGShapeLibrary::create_shape(shape_id, position)
    ↓
load_svgshape_file(filepath)  // Load .svgshape JSON
    ↓
SVGGenerator::generate(shape_desc)  // Generate SVG XML
    ↓
Returns Stroke with svg_data
    ↓
CanvasView::draw_svg_stroke()  // Render to canvas
    ↓
svg_renderer_->load_svg() + render()  // LunaSVG + NanoVG
```

**Files**:
- `whiteboard/src/svg/svg_shape_library.cpp`
- `whiteboard/src/svg/svg_generator.cpp`
- `whiteboard/src/svg/svg_renderer.cpp` (shared!)

---

## Key Classes

### svg/SVGRenderer (whiteboard namespace)
**Purpose**: Runtime SVG rendering to screen

**Location**: `whiteboard/include/whiteboard/svg/svg_renderer.h`

**Methods**:
- `load_svg(svg_data)` → Returns `lunasvg::Document`
- `render(ctx, document, pos, scale, rotation, tint)` → Draws to NanoVG

**Used By**:
- CanvasView (for Tool::SVGShape strokes)
- RenderingPipeline (for DDF SVG shapes)
- Both systems share this renderer!

**Libraries**:
- LunaSVG - SVG parsing and rasterization
- NanoVG - GPU-accelerated 2D rendering

---

### ddf/SVGRenderer (whiteboard::ddf namespace)
**Purpose**: Export DDF documents to SVG files

**Location**: `whiteboard/include/whiteboard/ddf/svg_renderer.h`

**Methods**:
- `render(DDFDocument)` → Returns SVG XML string
- `render_shape(Shape)` → Returns SVG element string
- `render_connector(Connector)` → Returns SVG path string

**Used By**:
- DDFDocument::export_to_svg()
- Export feature

**Libraries**:
- None - pure C++ string generation

**Note**: This is NOT used for rendering to screen!

---

## Rendering Paths

### Path 1: User Draws SVG Shape
```
User clicks shape tool
    ↓
Selects shape from library
    ↓
SVGShapeLibrary::create_shape()
    ↓
Generates SVG data
    ↓
Creates Stroke with Tool::SVGShape
    ↓
CanvasView::draw_svg_stroke()
    ↓
svg/SVGRenderer::load_svg() [LunaSVG]
    ↓
svg/SVGRenderer::render() [NanoVG]
    ↓
Screen
```

### Path 2: User Imports DDF Document
```
User clicks "Import DDF"
    ↓
DDFDocument::load_from_json()
    ↓
RenderingPipeline::render()
    ↓
For SVG shapes:
    ↓
render_svg(shape)
    ↓
svg/SVGRenderer::load_svg() [LunaSVG]
    ↓
svg/SVGRenderer::render() [NanoVG]
    ↓
Screen
```

### Path 3: User Exports DDF to SVG File
```
User clicks "Export to SVG"
    ↓
DDFDocument::export_to_svg()
    ↓
ddf/SVGRenderer::render()
    ↓
Generates SVG XML string
    ↓
Saves to file
    ↓
No screen rendering!
```

---

## Common Misconceptions

### ❌ "DDF doesn't use LunaSVG"
**Wrong!** DDF rendering DOES use LunaSVG via `svg/SVGRenderer` for SVG shapes.

### ❌ "There are two separate SVG rendering systems"
**Partially wrong!** There are two `SVGRenderer` classes, but:
- `svg/SVGRenderer` - Runtime rendering (LunaSVG + NanoVG) - **shared by both systems**
- `ddf/SVGRenderer` - Export to file (string generation) - **not for rendering**

### ❌ "Canvas and DDF render differently"
**Wrong!** Both use the same `svg/SVGRenderer` → LunaSVG → NanoVG pipeline for SVG shapes.

### ✅ "Both systems share the same rendering backend"
**Correct!** Both Canvas and DDF use `svg/SVGRenderer` for SVG shapes.

---

## File Organization

```
whiteboard/
├── include/whiteboard/
│   ├── svg/
│   │   └── svg_renderer.h          # Runtime SVG rendering
│   └── ddf/
│       └── svg_renderer.h          # DDF export to SVG
│
├── src/
│   ├── svg/
│   │   ├── svg_renderer.cpp        # LunaSVG + NanoVG
│   │   ├── svg_generator.cpp       # .svgshape → SVG
│   │   └── svg_shape_library.cpp   # Shape library
│   ├── ddf/
│   │   ├── svg_renderer.cpp        # DDF → SVG XML
│   │   ├── rendering_pipeline.cpp  # DDF rendering
│   │   └── ddf_document.cpp        # DDF model
│   └── canvas/
│       └── canvas_view.cpp         # Canvas rendering
│
├── shapes/                          # .svgshape templates
└── examples/ddf/                    # DDF documents
```

---

## Performance Considerations

### Shared Renderer Benefits
- **Code reuse**: One SVG renderer for all systems
- **Consistent rendering**: Same visual output everywhere
- **Optimized**: LunaSVG is fast and memory-safe
- **GPU-accelerated**: NanoVG uses OpenGL

### Caching
- Canvas caches SVG documents per stroke
- DDF rendering pipeline can cache render trees
- LunaSVG documents are lightweight

---

## Future Improvements

### Potential Optimizations
1. **Render tree caching** - Cache DDF render trees between frames
2. **Viewport culling** - Only render visible shapes
3. **Level of detail** - Simplify shapes when zoomed out
4. **Batch rendering** - Group similar shapes for GPU efficiency

### Potential Features
1. **WebGL backend** - Alternative to NanoVG
2. **SVG filters** - Blur, drop-shadow, etc.
3. **SVG animations** - SMIL animation support
4. **Hardware acceleration** - GPU compute for complex shapes

---

## Debugging Tips

### Check which renderer is being used
```cpp
// In canvas_view.cpp
logd("Rendering SVG via svg/SVGRenderer");

// In rendering_pipeline.cpp
logd("DDF: Rendered SVG via SVGRenderer");
```

### Verify LunaSVG is working
```cpp
auto doc = svg_renderer_->load_svg(svg_data);
if (!doc) {
    loge("LunaSVG failed to parse SVG");
}
```

### Check NanoVG context
```cpp
if (!nvg_context_) {
    loge("NanoVG context is null!");
}
```

---

## Related Documentation

- [SVGShape vs DDF Comparison](SVGSHAPE_VS_DDF_COMPARISON.md)
- [SVG Shape Format](api/SVG_SHAPE_FORMAT.md)
- [DDF API Reference](api/DDF_API_REFERENCE.md)
- [SVG Generator Guide](dev/SVG_GENERATOR_GUIDE.md)

---

**Last Updated**: October 2025
