# Format Comparison: DDF vs .svgshape

## Quick Reference

| Format | Purpose | Import Where | Contains |
|--------|---------|--------------|----------|
| **DDF (.json)** | Complete diagrams | File → Import DDF | Shapes, connectors, data, events, styles |
| **.svgshape** | Shape templates | Shape library only | Single shape or shape group |

## DDF Format (.json)

**Purpose:** Complete, editable diagrams

**Location:** `whiteboard/examples/ddf/*.json`

**Import:** File → Import DDF Document

**Structure:**
```json
{
  "version": "1.0",
  "metadata": {...},
  "data": [...],           // Data nodes
  "shapes": [...],         // Visual shapes
  "connectors": [...],     // Lines between shapes
  "components": [...],     // Reusable components
  "events": [...],         // Interactive events
  "styles": {...}          // CSS-like styles
}
```

**Example:**
```json
{
  "version": "1.0",
  "shapes": [
    {
      "id": "box1",
      "type": "rect",
      "geometry": {"x": 100, "y": 100, "width": 120, "height": 60},
      "text": "Process"
    }
  ],
  "connectors": [
    {
      "from": {"shape_id": "box1"},
      "to": {"shape_id": "box2"}
    }
  ]
}
```

## .svgshape Format

**Purpose:** Reusable shape templates for the shape library

**Location:** `whiteboard/shapes/**/*.svgshape`

**Usage:** Loaded by shape library, placed via shape tool

**Structure:**
```json
{
  "version": "1.0",
  "metadata": {...},
  "type": "rect",          // Single shape
  "geometry": {...},
  "style": {...}
}
```

**Or multiple shapes:**
```json
{
  "version": "1.0",
  "shapes": [              // Multiple shapes as template
    {"type": "rect", ...},
    {"type": "text", ...}
  ]
}
```

**Example:**
```json
{
  "version": "1.0",
  "type": "rect",
  "geometry": {"x": 0, "y": 0, "width": 100, "height": 50},
  "style": {"fill": "#3498db", "stroke": "#2c3e50"}
}
```

## Key Differences

### DDF Document
- ✅ Complete diagram with multiple shapes
- ✅ Connectors between shapes
- ✅ Data layer for data-driven diagrams
- ✅ Events and interactivity
- ✅ Global styles
- ✅ Import via "Import DDF"
- ❌ Not for shape library

### .svgshape File
- ✅ Single shape or shape group template
- ✅ Reusable across diagrams
- ✅ Parametric ({{placeholders}})
- ✅ Used by shape library
- ✅ Placed via shape tool
- ❌ No connectors
- ❌ No data layer
- ❌ Not imported as diagram

## Usage Examples

### Creating a Diagram (Use DDF)

1. Create `my_diagram.json`:
```json
{
  "version": "1.0",
  "shapes": [
    {"id": "s1", "type": "rect", "geometry": {...}},
    {"id": "s2", "type": "circle", "geometry": {...}}
  ],
  "connectors": [
    {"from": {"shape_id": "s1"}, "to": {"shape_id": "s2"}}
  ]
}
```

2. Import: File → Import DDF Document → Select `my_diagram.json`

### Creating a Reusable Shape (Use .svgshape)

1. Create `shapes/custom/my_button.svgshape`:
```json
{
  "version": "1.0",
  "shapes": [
    {"type": "rect", "geometry": {...}, "style": {...}},
    {"type": "text", "text": "{{label}}", ...}
  ]
}
```

2. Add to `shapes/library.json`:
```json
{
  "shapes": [
    {
      "id": "custom.my_button",
      "name": "My Button",
      "category": "Custom",
      "svg_file": "custom/my_button.svgshape"
    }
  ]
}
```

3. Use: Select shape tool → Choose "My Button" → Click to place

## Converting Between Formats

### From .svgshape to DDF

Take a shape template and use it in a diagram:

**.svgshape:**
```json
{
  "type": "rect",
  "geometry": {"x": 0, "y": 0, "width": 100, "height": 50},
  "style": {"fill": "#3498db"}
}
```

**DDF:**
```json
{
  "shapes": [
    {
      "id": "my_shape",
      "type": "rect",
      "geometry": {"x": 100, "y": 100, "width": 100, "height": 50},
      "inline_style": {"fill": "#3498db"}
    }
  ]
}
```

### From DDF to .svgshape

Extract a shape from a diagram to make it reusable:

**DDF:**
```json
{
  "shapes": [
    {
      "id": "nice_button",
      "type": "rect",
      "geometry": {"x": 100, "y": 100, "width": 120, "height": 40},
      "inline_style": {"fill": "#9b59b6", "stroke": "#8e44ad"}
    }
  ]
}
```

**.svgshape:**
```json
{
  "version": "1.0",
  "type": "rect",
  "geometry": {"x": 0, "y": 0, "width": 120, "height": 40},
  "style": {"fill": "#9b59b6", "stroke": "#8e44ad"}
}
```

## Summary

- **Want to create a diagram?** → Use DDF format (.json)
- **Want to create a reusable shape?** → Use .svgshape format
- **Both use similar JSON structure** → Easy to convert between them
- **DDF = Complete diagrams** → Import as document
- **.svgshape = Shape templates** → Add to shape library

## Error: "Trying to import .svgshape as DDF"

If you see this error, you're trying to import a shape template as a diagram.

**Wrong:** File → Import DDF → Select `.svgshape` file ❌

**Right:** 
1. Add `.svgshape` to shape library
2. Use shape tool to place it
3. Or create a DDF document that references it
