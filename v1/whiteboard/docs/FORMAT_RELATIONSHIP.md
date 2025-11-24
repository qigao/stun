# DDF and SVGShape Relationship

## Visual Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         THE BIG PICTURE                          │
└─────────────────────────────────────────────────────────────────┘

                    ┌─────────────────────┐
                    │   DDF DOCUMENT      │
                    │  (The Container)    │
                    │                     │
                    │  In-memory: ✅      │
                    │  File format: ✅    │
                    └──────────┬──────────┘
                               │
                               │ Contains
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
        ▼                      ▼                      ▼
┌───────────────┐    ┌──────────────────┐    ┌──────────────┐
│    Shapes     │    │   Connectors     │    │     Data     │
│               │    │                  │    │              │
│ - Native DDF  │    │ - Lines          │    │ - Nodes      │
│ - SVG shapes  │    │ - Arrows         │    │ - Relations  │
│   (references)│    │ - Smart routing  │    │ - Properties │
└───────┬───────┘    └──────────────────┘    └──────────────┘
        │
        │ References
        │
        ▼
┌─────────────────────┐
│  SVGSHAPE TEMPLATE  │
│  (Building Block)   │
│                     │
│  In-memory: ❌      │
│  File format: ✅    │
│  (Loaded on-demand) │
└─────────────────────┘
```

## Concrete Example

### Scenario: Creating an Organization Chart

```
Step 1: User places "Person Card" from shape library
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Shape Library:
  shapes/org/person_card.svgshape
  ┌─────────────────────────────┐
  │ {                           │
  │   "shapes": [               │
  │     {                       │
  │       "type": "rect",       │
  │       "geometry": {...}     │
  │     },                      │
  │     {                       │
  │       "type": "text",       │
  │       "text": "{{name}}"    │
  │     }                       │
  │   ]                         │
  │ }                           │
  └─────────────────────────────┘
           │
           │ Loaded by SVGShapeLibrary
           │
           ▼
  Generated SVG data
           │
           │ Creates DDF shape
           ▼
  
DDF Document (in memory):
  ┌─────────────────────────────┐
  │ {                           │
  │   "shapes": [               │
  │     {                       │
  │       "id": "person1",      │
  │       "type": "svg",        │
  │       "svg_shape_id":       │
  │         "org.person_card",  │ ← References template
  │       "svg_data": "<svg>...",│
  │       "svg_parameters": {   │
  │         "name": "Sarah"     │
  │       },                    │
  │       "geometry": {         │
  │         "x": 100,           │
  │         "y": 100            │
  │       }                     │
  │     }                       │
  │   ]                         │
  │ }                           │
  └─────────────────────────────┘


Step 2: User saves the diagram
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DDF Document → org_chart.json
  ┌─────────────────────────────┐
  │ {                           │
  │   "version": "1.0",         │
  │   "shapes": [               │
  │     {                       │
  │       "id": "person1",      │
  │       "type": "svg",        │
  │       "svg_shape_id":       │
  │         "org.person_card",  │ ← Reference saved
  │       "svg_parameters": {   │
  │         "name": "Sarah"     │
  │       },                    │
  │       "geometry": {...}     │
  │     }                       │
  │   ]                         │
  │ }                           │
  └─────────────────────────────┘

Note: SVGShape template NOT embedded in file!


Step 3: User loads the diagram later
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

org_chart.json → DDF Document
           │
           │ For each shape with svg_shape_id
           │
           ▼
  Load SVGShape template from library
  shapes/org/person_card.svgshape
           │
           │ Generate SVG with parameters
           │
           ▼
  Render to screen using svg/SVGRenderer
```

## Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        DATA FLOW                                 │
└─────────────────────────────────────────────────────────────────┘

USER ACTION: Place shape from library
    │
    ▼
┌─────────────────────────────────┐
│  SVGShapeLibrary                │
│  - Loads .svgshape file         │
│  - Generates SVG XML            │
│  - Returns Stroke/Shape         │
└────────────┬────────────────────┘
             │
             ▼
┌─────────────────────────────────┐
│  DDF Document (in memory)       │
│  - Stores shape with reference  │
│  - svg_shape_id = "org.person"  │
│  - svg_data = "<svg>..."        │
└────────────┬────────────────────┘
             │
             │ User saves
             ▼
┌─────────────────────────────────┐
│  File: org_chart.json           │
│  - Contains shape reference     │
│  - Does NOT embed template      │
└────────────┬────────────────────┘
             │
             │ User loads
             ▼
┌─────────────────────────────────┐
│  DDF Document (in memory)       │
│  - Reads shape reference        │
│  - Loads template from library  │
│  - Generates SVG data           │
└────────────┬────────────────────┘
             │
             │ Render
             ▼
┌─────────────────────────────────┐
│  svg/SVGRenderer                │
│  - LunaSVG parses SVG           │
│  - NanoVG draws to screen       │
└─────────────────────────────────┘
```

## Memory Layout

```
┌─────────────────────────────────────────────────────────────────┐
│                    RUNTIME MEMORY                                │
└─────────────────────────────────────────────────────────────────┘

Application Memory:
├── WhiteboardDocument (Canvas strokes)
│   ├── Stroke 1: Pen drawing
│   ├── Stroke 2: Rectangle
│   └── Stroke 3: SVGShape (Tool::SVGShape)
│       └── svg_data: "<svg>..."
│
├── DDFDocument (Imported diagram)
│   ├── ShapeLayer
│   │   ├── Shape 1: rect (native DDF)
│   │   ├── Shape 2: svg (references template)
│   │   │   ├── svg_shape_id: "flowchart.process"
│   │   │   └── svg_data: "<svg>..."  ← Generated from template
│   │   └── Shape 3: circle (native DDF)
│   ├── ConnectorLayer
│   │   └── Connector 1: Shape1 → Shape2
│   ├── DataLayer
│   │   └── Node 1: {name: "Process A"}
│   └── EventLayer
│       └── Event 1: click → action
│
└── SVGShapeLibrary (Shape templates)
    ├── flowchart/process.svgshape  ← Loaded on-demand
    ├── flowchart/decision.svgshape
    └── org/person_card.svgshape

Note: SVGShape templates are NOT kept in memory!
      They're loaded when needed, SVG is generated, then discarded.
```

## File System Layout

```
whiteboard/
├── shapes/                          ← SVGShape templates
│   ├── library.json                 (Index of all shapes)
│   ├── flowchart/
│   │   ├── process.svgshape        ← Template files
│   │   └── decision.svgshape
│   ├── org/
│   │   └── person_card.svgshape
│   └── network/
│       └── router.svgshape
│
└── examples/
    └── ddf/                         ← DDF documents
        ├── org_chart.json          ← Complete diagrams
        ├── network_diagram.json
        └── flowchart.json

Key Points:
- SVGShape files stay in shapes/ folder
- DDF files reference shapes by ID
- Templates are NOT embedded in DDF files
```

## Comparison Table

| Aspect | DDF | SVGShape |
|--------|-----|----------|
| **What** | Document format | Visual template |
| **Analogy** | Word document | Clip art |
| **In Memory** | ✅ Full document structure | ❌ Loaded on-demand |
| **On Disk** | ✅ .json files | ✅ .svgshape files |
| **Contains** | Everything | Just shape definition |
| **References** | Can reference SVGShapes | Standalone |
| **Scope** | Complete diagram | Single shape/group |
| **Reusable** | No (instance) | Yes (template) |
| **Location** | `examples/ddf/` | `shapes/` |
| **Loaded By** | DDFDocument | SVGShapeLibrary |
| **Used For** | Import/export diagrams | Shape library |

## Key Takeaways

### ✅ DDF is the Universal Format
- In-memory representation of diagrams
- File format for saving/loading
- Contains shapes, connectors, data, events, styles
- Can reference SVGShape templates

### ✅ SVGShape is a Building Block
- Reusable visual element template
- Stored in shape library
- Loaded on-demand when needed
- Referenced by DDF shapes (not embedded)

### ✅ They Work Together
- DDF shapes can reference SVGShape templates
- SVGShapeLibrary loads templates and generates SVG
- Generated SVG is stored in DDF shape
- Both render using the same svg/SVGRenderer

### ✅ Separation of Concerns
- **DDF**: Document structure, logic, data
- **SVGShape**: Visual appearance, reusability

---

**Last Updated**: October 2025
