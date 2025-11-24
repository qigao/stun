# SVGShape vs DDF: Complete Comparison

## Quick Summary

| Aspect | .svgshape | DDF (.json) |
|--------|-----------|-------------|
| **Purpose** | Shape template | Complete diagram |
| **Scope** | Single shape/group | Full document |
| **Location** | `shapes/` folder | `examples/ddf/` |
| **Usage** | Shape library | Import as diagram |
| **Contains** | Shapes only | Shapes + connectors + data + events |
| **Parametric** | Yes (`{{param}}`) | Yes (expressions) |
| **Reusable** | Yes (template) | No (instance) |
| **Data-driven** | No | Yes |
| **Interactive** | No | Yes (events) |

---

## Detailed Comparison

### 1. File Structure

#### SVGShape (.svgshape)
```json
{
  "version": "1.0",
  "metadata": {...},
  "shapes": [              // Just shapes
    {
      "type": "rect",
      "geometry": {...},
      "style": {...}
    }
  ]
}
```

#### DDF (.json)
```json
{
  "version": "1.0",
  "metadata": {...},
  "data": {...},           // Data layer
  "shapes": [...],         // Visual shapes
  "connectors": [...],     // Connections
  "components": {...},     // Reusable components
  "events": [...],         // Interactivity
  "styles": {...},         // Global styles
  "layouts": [...]         // Layout algorithms
}
```

---

### 2. Purpose & Use Cases

#### SVGShape
**Purpose**: Reusable shape templates for the shape library

**Use Cases**:
- Creating custom shapes for the shape palette
- Building a library of standard shapes (flowchart, UML, network)
- Parametric shapes with placeholders
- Consistent branding (company logos, icons)

**Example**: Process box for flowcharts
```json
{
  "shapes": [
    {
      "type": "rect",
      "geometry": {"x": 0, "y": 0, "width": 120, "height": 60},
      "style": {"fill": "#3498db"}
    },
    {
      "type": "text",
      "text": "{{label}}",
      "geometry": {"x": 60, "y": 30}
    }
  ]
}
```

#### DDF
**Purpose**: Complete, editable diagrams with data and logic

**Use Cases**:
- Organization charts
- Network diagrams
- Flowcharts with logic
- Data-driven visualizations
- Interactive dashboards
- Process documentation

**Example**: Org chart with data
```json
{
  "data": {
    "nodes": [
      {"id": "ceo", "properties": {"name": "Sarah", "title": "CEO"}}
    ]
  },
  "shapes": [
    {
      "id": "ceo_card",
      "data_binding": {"node_id": "ceo"},
      "type": "rect"
    }
  ]
}
```

---

### 3. Feature Comparison

| Feature | SVGShape | DDF |
|---------|----------|-----|
| **Shapes** | ✅ Yes | ✅ Yes |
| **Connectors** | ❌ No | ✅ Yes |
| **Data Layer** | ❌ No | ✅ Yes |
| **Data Binding** | ❌ No | ✅ Yes |
| **Events** | ❌ No | ✅ Yes |
| **Global Styles** | ❌ No | ✅ Yes (CSS-like) |
| **Components** | ❌ No | ✅ Yes |
| **Layouts** | ❌ No | ✅ Yes (auto-layout) |
| **Hierarchical** | ✅ Groups | ✅ Tree structure |
| **Parametric** | ✅ `{{param}}` | ✅ Expressions |
| **Reusable** | ✅ Template | ❌ Instance |

---

### 4. Shape Definition

#### SVGShape - Simple & Focused
```json
{
  "type": "rect",
  "geometry": {
    "x": 0,
    "y": 0,
    "width": 100,
    "height": 50
  },
  "style": {
    "fill": "#3498db",
    "stroke": "#2c3e50"
  }
}
```

**Characteristics**:
- Coordinates relative to (0,0)
- No ID required
- Style inline
- No connections

#### DDF - Rich & Connected
```json
{
  "id": "shape1",
  "type": "rect",
  "classes": ["process", "important"],
  "geometry": {
    "x": 100,
    "y": 100,
    "width": 100,
    "height": 50
  },
  "inline_style": {
    "fill": "#3498db"
  },
  "data_binding": {
    "node_id": "data1",
    "property_mappings": {
      "text": "data.name"
    }
  },
  "connection_points": [
    {"id": "top", "x_ratio": 0.5, "y_ratio": 0}
  ]
}
```

**Characteristics**:
- Absolute coordinates
- ID required for connections
- CSS classes + inline styles
- Data binding
- Connection points

---

### 5. Parametric Features

#### SVGShape - Template Parameters
```json
{
  "shapes": [
    {
      "type": "text",
      "text": "{{label}}",
      "style": {
        "fill": "{{color}}"
      }
    }
  ]
}
```

**Usage**:
```cpp
shape_library->create_shape("my_shape", position, {
  {"label", "Hello"},
  {"color", "#ff0000"}
});
```

#### DDF - Expression Language
```json
{
  "shapes": [
    {
      "type": "text",
      "text": "{{data.name}}",
      "geometry": {
        "x": "{{data.x * 100}}",
        "y": "{{data.y * 100}}"
      }
    }
  ]
}
```

**Features**:
- Data binding
- Arithmetic expressions
- Conditional logic
- Property mappings

---

### 6. Real-World Examples

#### SVGShape: Network Device Template
```json
{
  "version": "1.0",
  "metadata": {
    "title": "Network Device",
    "category": "Network"
  },
  "shapes": [
    {
      "type": "rect",
      "geometry": {"x": 0, "y": 0, "width": 100, "height": 80, "rx": 5},
      "style": {"fill": "#34495e", "stroke": "#2c3e50"}
    },
    {
      "type": "circle",
      "geometry": {"cx": 85, "cy": 15, "r": 5},
      "style": {"fill": "{{status_color}}"}
    },
    {
      "type": "text",
      "geometry": {"x": 50, "y": 65},
      "text": "{{label}}",
      "style": {"fill": "#ecf0f1", "font-size": "12px"}
    }
  ]
}
```

**Use**: Place in shape library, drag onto canvas with different labels/colors

#### DDF: Organization Chart
```json
{
  "version": "1.0",
  "data": {
    "nodes": [
      {"id": "ceo", "properties": {"name": "Sarah", "title": "CEO"}},
      {"id": "cto", "properties": {"name": "Michael", "title": "CTO"}}
    ],
    "relationships": [
      {"from": "cto", "to": "ceo", "type": "reports_to"}
    ]
  },
  "components": {
    "definitions": [
      {
        "id": "person_card",
        "shapes": [
          {"type": "rect", "geometry": {...}},
          {"type": "text", "text": "{{name}}"}
        ]
      }
    ]
  },
  "layouts": [
    {
      "algorithm": "tree",
      "parameters": {"direction": "top-down"}
    }
  ],
  "events": [
    {
      "trigger": "hover",
      "actions": [{"type": "show_tooltip"}]
    }
  ]
}
```

**Use**: Import as complete diagram, auto-layout, interactive

---

### 7. Rendering Pipeline

#### SVGShape Rendering
```
.svgshape file
    ↓
SVGShapeLibrary::load_svgshape_file()
    ↓
SVGGenerator::generate()
    ↓
SVG XML string
    ↓
svg/SVGRenderer::load_svg() [LunaSVG]
    ↓
svg/SVGRenderer::render() [NanoVG]
    ↓
Canvas pixels
```

#### DDF Rendering
```
DDF .json file
    ↓
DDFDocument::load_from_json()
    ↓
RenderingPipeline::render()
    ↓
For SVG shapes:
    ↓
svg/SVGRenderer::load_svg() [LunaSVG]
    ↓
svg/SVGRenderer::render() [NanoVG]
    ↓
Canvas pixels
```

**Note**: Both use the same `svg/SVGRenderer` for actual rendering!

---

### 8. When to Use Which

#### Use SVGShape When:
- ✅ Creating reusable shape templates
- ✅ Building a shape library
- ✅ Need parametric shapes
- ✅ Want to drag-and-drop shapes
- ✅ Creating icons or symbols
- ✅ Standardizing visual elements

#### Use DDF When:
- ✅ Creating complete diagrams
- ✅ Need connectors between shapes
- ✅ Have data to visualize
- ✅ Want interactivity (hover, click)
- ✅ Need auto-layout
- ✅ Building data-driven visualizations
- ✅ Creating editable documents

---

### 9. Conversion Between Formats

#### SVGShape → DDF
Extract a shape template and use it in a diagram:

```python
# .svgshape
{
  "type": "rect",
  "geometry": {"x": 0, "y": 0, "width": 100, "height": 50}
}

# Convert to DDF
{
  "shapes": [
    {
      "id": "shape1",
      "type": "rect",
      "geometry": {"x": 100, "y": 100, "width": 100, "height": 50}
    }
  ]
}
```

**Changes**:
- Add absolute coordinates
- Add ID
- Change `style` → `inline_style`

#### DDF → SVGShape
Extract a shape from a diagram to make it reusable:

```python
# DDF
{
  "shapes": [
    {
      "id": "nice_button",
      "type": "rect",
      "geometry": {"x": 100, "y": 100, "width": 120, "height": 40}
    }
  ]
}

# Convert to .svgshape
{
  "type": "rect",
  "geometry": {"x": 0, "y": 0, "width": 120, "height": 40}
}
```

**Changes**:
- Remove ID
- Reset coordinates to (0,0)
- Change `inline_style` → `style`
- Remove data bindings, connections

---

### 10. Integration

#### SVGShape in DDF
Reference a shape library shape in a DDF document:

```json
{
  "shapes": [
    {
      "id": "device1",
      "type": "svg",
      "svg_shape_id": "network.router",
      "svg_parameters": {
        "label": "Main Router",
        "status_color": "#2ecc71"
      },
      "geometry": {
        "x": 100,
        "y": 100,
        "width": 100,
        "height": 80
      }
    }
  ]
}
```

---

### 11. File Organization

```
whiteboard/
├── shapes/                    # SVGShape templates
│   ├── library.json          # Shape library index
│   ├── flowchart/
│   │   ├── process.svgshape
│   │   └── decision.svgshape
│   ├── network/
│   │   └── router.svgshape
│   └── ui/
│       └── button.svgshape
│
└── examples/
    └── ddf/                   # DDF documents
        ├── org_chart.json
        ├── network_diagram.json
        └── flowchart.json
```

---

### 12. Summary Table

| Aspect | SVGShape | DDF |
|--------|----------|-----|
| **File Extension** | `.svgshape` | `.json` |
| **Namespace** | `whiteboard` | `whiteboard::ddf` |
| **Loader** | `SVGShapeLibrary` | `DDFDocument` |
| **Generator** | `SVGGenerator` | N/A |
| **Renderer** | `svg/SVGRenderer` | `RenderingPipeline` + `svg/SVGRenderer` |
| **Export** | N/A | `ddf/SVGRenderer` |
| **Coordinates** | Relative (0,0) | Absolute |
| **IDs** | Optional | Required |
| **Styling** | `style` | `inline_style` + `classes` |
| **Connections** | No | Yes |
| **Data** | No | Yes |
| **Events** | No | Yes |
| **Reusable** | Yes | No |

---

## Conclusion

- **SVGShape** = Shape template for library (like a cookie cutter)
- **DDF** = Complete diagram document (like a finished cake)
- Both use similar JSON structure for shapes
- Both render using the same `svg/SVGRenderer` + LunaSVG + NanoVG
- Easy to convert between them
- Use together: SVGShape templates → DDF documents

---

**Last Updated**: October 2025
