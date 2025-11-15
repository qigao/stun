# Using SVG Shapes in DDF

## Overview
The whiteboard has a built-in SVG shape library with 111 shapes across 11 categories. You can reference these shapes in your DDF files.

## SVG Shape in DDF

### Method 1: Reference by ID
```json
{
  "shapes": [
    {
      "id": "my_server",
      "type": "svg",
      "svg_shape_id": "network.server",
      "geometry": {
        "x": 100,
        "y": 100,
        "width": 80,
        "height": 80
      },
      "svg_parameters": {
        "label": "Web Server"
      }
    }
  ]
}
```

### Method 2: Inline SVG Data
```json
{
  "shapes": [
    {
      "id": "custom_icon",
      "type": "svg",
      "geometry": {
        "x": 200,
        "y": 200,
        "width": 50,
        "height": 50
      },
      "svg_data": "<svg>...</svg>"
    }
  ]
}
```

## Available Shape Categories

### Network (10 shapes)
- `network.server`
- `network.router`
- `network.switch`
- `network.firewall`
- `network.cloud`
- `network.database`
- `network.laptop`
- `network.desktop`
- `network.mobile`
- `network.wifi`

### Flowchart (12 shapes)
- `flowchart.process`
- `flowchart.decision`
- `flowchart.terminator`
- `flowchart.data`
- `flowchart.document`
- `flowchart.manual_input`
- `flowchart.preparation`
- `flowchart.connector`
- `flowchart.or`
- `flowchart.summing_junction`
- `flowchart.delay`
- `flowchart.stored_data`

### UML (12 shapes)
- `uml.class`
- `uml.interface`
- `uml.actor`
- `uml.usecase`
- `uml.component`
- `uml.node`
- `uml.package`
- `uml.note`
- `uml.boundary`
- `uml.control`
- `uml.entity`
- `uml.lifeline`

### Basic (13 shapes)
- `basic.rectangle`
- `basic.circle`
- `basic.triangle`
- `basic.star`
- `basic.pentagon`
- `basic.hexagon`
- `basic.diamond`
- `basic.ellipse`
- `basic.parallelogram`
- `basic.trapezoid`
- `basic.cross`
- `basic.octagon`
- `basic.rounded_rect`

### Arrows (7 shapes)
- `arrows.right`
- `arrows.left`
- `arrows.up`
- `arrows.down`
- `arrows.bidirectional`
- `arrows.curved`
- `arrows.double`

### Business (8 shapes)
- `business.person`
- `business.team`
- `business.building`
- `business.chart`
- `business.money`
- `business.calendar`
- `business.document`
- `business.presentation`

### UI (33 shapes)
- `ui.button`
- `ui.checkbox`
- `ui.radio`
- `ui.textbox`
- `ui.dropdown`
- `ui.slider`
- `ui.toggle`
- `ui.window`
- `ui.dialog`
- `ui.menu`
- ... and 23 more

### Electrical (4 shapes)
- `electrical.resistor`
- `electrical.capacitor`
- `electrical.inductor`
- `electrical.battery`

### Math (4 shapes)
- `math.plus`
- `math.minus`
- `math.multiply`
- `math.divide`

### Symbols (6 shapes)
- `symbols.check`
- `symbols.cross`
- `symbols.warning`
- `symbols.info`
- `symbols.question`
- `symbols.star`

### Table (2 shapes)
- `table.grid`
- `table.cell`

## Parametric Shapes

Many shapes support parameters that can be customized:

```json
{
  "svg_shape_id": "network.server",
  "svg_parameters": {
    "label": "Database Server",
    "color": "#3498db",
    "ip": "192.168.1.100"
  }
}
```

## Example: Network Diagram with SVG Shapes

```json
{
  "version": "1.0",
  "metadata": {
    "title": "Network Topology"
  },
  "shapes": [
    {
      "id": "router1",
      "type": "svg",
      "svg_shape_id": "network.router",
      "geometry": { "x": 200, "y": 100, "width": 80, "height": 80 },
      "svg_parameters": { "label": "Core Router" }
    },
    {
      "id": "server1",
      "type": "svg",
      "svg_shape_id": "network.server",
      "geometry": { "x": 100, "y": 250, "width": 80, "height": 80 },
      "svg_parameters": { "label": "Web Server" }
    },
    {
      "id": "server2",
      "type": "svg",
      "svg_shape_id": "network.database",
      "geometry": { "x": 300, "y": 250, "width": 80, "height": 80 },
      "svg_parameters": { "label": "DB Server" }
    }
  ],
  "connectors": [
    {
      "id": "conn1",
      "from": { "shape_id": "router1", "connection_point": "bottom" },
      "to": { "shape_id": "server1", "connection_point": "top" },
      "arrow_end": "none"
    },
    {
      "id": "conn2",
      "from": { "shape_id": "router1", "connection_point": "bottom" },
      "to": { "shape_id": "server2", "connection_point": "top" },
      "arrow_end": "none"
    }
  ]
}
```

## Implementation Status

### ✅ Currently Working
- SVG shape library loaded (111 shapes)
- Shape panel UI for browsing
- Drag & drop placement
- Parametric shape generation

### 🚧 To Implement for DDF
- Parse `svg_shape_id` in DDF shapes
- Load SVG from library during DDF import
- Render SVG shapes in DDF rendering pipeline
- Convert SVG shapes to editable strokes

## Next Steps

To fully integrate SVG shapes with DDF, we need to:
1. Update DDF shape parsing to recognize `svg_shape_id`
2. Load SVG data from library when rendering
3. Add SVG rendering to the rendering pipeline
4. Support SVG shape conversion to editable strokes
