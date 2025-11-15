# Architecture Summary: DDF and SVGShape

## Core Concept

```
┌─────────────────────────────────────────────────────────────┐
│                    WHITEBOARD ARCHITECTURE                   │
└─────────────────────────────────────────────────────────────┘

DDF = Document Format (in-memory + file)
    ↓
    Contains shapes, connectors, data, events, styles
    ↓
    Shapes can reference SVGShape templates
    ↓
SVGShape = Visual Element Template
    ↓
    Basic building blocks for shapes
    ↓
    Rendered as part of DDF or Canvas strokes
```

## The Relationship

### DDF (Diagram Definition Format)
**Role**: The document format for everything

**Used For**:
- ✅ In-memory representation of diagrams
- ✅ File format for saving/loading diagrams
- ✅ Complete document structure
- ✅ Shapes, connectors, data, events, styles

**Analogy**: DDF is like a **Word document** - it's the complete file format that contains everything.

```json
{
  "version": "1.0",
  "shapes": [
    {
      "id": "shape1",
      "type": "svg",
      "svg_shape_id": "flowchart.process",  // References SVGShape
      "geometry": {"x": 100, "y": 100}
    }
  ],
  "connectors": [...],
  "data": {...},
  "events": [...]
}
```

### SVGShape
**Role**: Basic visual element template

**Used For**:
- ✅ Reusable shape templates
- ✅ Building blocks for DDF shapes
- ✅ Shape library entries
- ✅ Visual primitives

**Analogy**: SVGShape is like a **clip art** or **icon** - a reusable visual element.

```json
{
  "type": "rect",
  "geometry": {"x": 0, "y": 0, "width": 100, "height": 50},
  "style": {"fill": "#3498db"}
}
```

## How They Work Together

### Composition Model

```
DDF Document (The Container)
├── Shape 1: Rectangle (native DDF shape)
├── Shape 2: SVG Shape (references "flowchart.process")
│   └── SVGShape Template: flowchart.process.svgshape
│       ├── Rectangle
│       └── Text
├── Shape 3: Circle (native DDF shape)
└── Connector: Shape1 → Shape2
```

### Example Flow

1. **User creates a flowchart**:
   ```
   User action: Place "Process" shape from library
       ↓
   SVGShapeLibrary loads "flowchart/process.svgshape"
       ↓
   Generates SVG data from template
       ↓
   Creates DDF shape with svg_shape_id reference
       ↓
   DDF document now contains the shape
   ```

2. **User saves the diagram**:
   ```
   DDF document → JSON file
   (SVGShape templates are referenced, not embedded)
   ```

3. **User loads the diagram**:
   ```
   JSON file → DDF document
       ↓
   For each shape with svg_shape_id:
       ↓
   Load SVGShape template from library
       ↓
   Render using svg/SVGRenderer
   ```

## In-Memory Representation

### DDF Document Structure
```cpp
class DDFDocument {
    DataLayer data_layer_;           // Data nodes
    ShapeLayer shape_layer_;         // Visual shapes
    ConnectorLayer connector_layer_; // Connections
    EventLayer event_layer_;         // Interactivity
    StyleLayer style_layer_;         // CSS-like styles
    ComponentLayer component_layer_; // Reusable components
};
```

### Shape in Memory
```cpp
struct Shape {
    std::string id;
    std::string type;  // "rect", "circle", "svg", etc.
    
    // For SVG shapes from library:
    std::string svg_shape_id;  // e.g., "flowchart.process"
    std::string svg_data;      // Generated SVG XML
    std::map<std::string, std::string> svg_parameters;
    
    // Geometry and style
    std::map<std::string, float> geometry;
    std::map<std::string, std::string> inline_style;
    
    // Tree structure
    std::vector<std::string> child_shape_ids;
    std::string parent_shape_id;
};
```

## File Format Usage

### DDF Files (.json)
**Purpose**: Save complete diagrams

**Contains**:
- All shapes (including references to SVGShapes)
- All connectors
- Data layer
- Events
- Styles

**Example**: `examples/ddf/org_chart.json`

### SVGShape Files (.svgshape)
**Purpose**: Define reusable shape templates

**Contains**:
- Shape geometry
- Style definitions
- Parameters ({{placeholders}})

**Example**: `shapes/flowchart/process.svgshape`

**Location**: Shape library folder, not in DDF documents

## The Key Insight

### DDF is the Universal Format

```
┌─────────────────────────────────────────────────────────┐
│                    DDF DOCUMENT                          │
│  (In-memory representation AND file format)              │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  Shapes:                                                 │
│  ┌────────────────────────────────────────────────┐    │
│  │ Native DDF Shapes (rect, circle, path, text)   │    │
│  └────────────────────────────────────────────────┘    │
│  ┌────────────────────────────────────────────────┐    │
│  │ SVG Shapes (references to .svgshape templates) │    │
│  └────────────────────────────────────────────────┘    │
│                                                          │
│  Connectors: Lines between shapes                        │
│  Data: Data nodes and relationships                      │
│  Events: Interactive behaviors                           │
│  Styles: CSS-like styling rules                          │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

### SVGShape is a Building Block

```
┌─────────────────────────────────────────────────────────┐
│                  SVGSHAPE TEMPLATE                       │
│  (Reusable visual element)                               │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  Basic shape definition:                                 │
│  - Geometry (x, y, width, height, etc.)                  │
│  - Style (fill, stroke, etc.)                            │
│  - Parameters ({{label}}, {{color}}, etc.)               │
│                                                          │
│  Used by:                                                │
│  - Shape library                                         │
│  - DDF shapes (via svg_shape_id reference)               │
│  - Canvas strokes (Tool::SVGShape)                       │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

## Practical Examples

### Example 1: Creating a Flowchart

```
1. User places "Process" shape from library
   → SVGShape template loaded: flowchart/process.svgshape
   → DDF shape created with svg_shape_id = "flowchart.process"
   → Shape added to DDF document in memory

2. User places "Decision" shape
   → SVGShape template loaded: flowchart/decision.svgshape
   → DDF shape created with svg_shape_id = "flowchart.decision"
   → Shape added to DDF document

3. User connects shapes
   → DDF connector created
   → Connector added to DDF document

4. User saves diagram
   → DDF document serialized to JSON file
   → File contains shape references, not SVGShape templates

5. User loads diagram later
   → DDF document loaded from JSON
   → For each shape with svg_shape_id:
     → Load SVGShape template from library
     → Generate SVG data
     → Render to screen
```

### Example 2: Canvas Drawing

```
1. User draws with pen tool
   → Creates Stroke with Tool::Pen
   → Stored in WhiteboardDocument (not DDF)

2. User places SVG shape from library
   → Creates Stroke with Tool::SVGShape
   → svg_data contains generated SVG
   → Stored in WhiteboardDocument

3. User imports DDF document
   → DDF document loaded
   → Rendered alongside canvas strokes
   → Both visible on canvas
```

## Summary Table

| Aspect | DDF | SVGShape |
|--------|-----|----------|
| **What is it?** | Document format | Visual element template |
| **Scope** | Complete diagram | Single shape/group |
| **In-memory** | ✅ Yes (DDFDocument) | ❌ No (loaded on-demand) |
| **File format** | ✅ Yes (.json) | ✅ Yes (.svgshape) |
| **Contains** | Everything | Just shape definition |
| **Reusable** | No (instance) | Yes (template) |
| **References** | Can reference SVGShapes | Standalone |
| **Used by** | Diagram import/export | Shape library |

## Analogy

Think of it like web development:

- **DDF** = HTML document (contains structure, content, references to images)
- **SVGShape** = Image file (reusable asset referenced by HTML)

Or like a word processor:

- **DDF** = Word document (.docx)
- **SVGShape** = Clip art library

Or like programming:

- **DDF** = Source code file (contains logic, references to libraries)
- **SVGShape** = Library function (reusable component)

## Conclusion

✅ **DDF is the universal format** for both in-memory representation and file storage

✅ **SVGShape is a building block** - a reusable visual element template

✅ **They work together**: DDF documents reference SVGShape templates

✅ **Separation of concerns**:
- DDF = Document structure, data, logic
- SVGShape = Visual appearance, reusable templates

---

**Last Updated**: October 2025
