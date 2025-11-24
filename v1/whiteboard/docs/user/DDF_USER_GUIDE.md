# DDF User Guide

## Introduction

The Diagram Definition Format (DDF) is a comprehensive format for creating data-driven, interactive diagrams with smart connectors and reusable components. DDF separates concerns into distinct layers, making it easy to create complex diagrams that are maintainable and scalable.

## Table of Contents

1. [Getting Started](#getting-started)
2. [DDF Format Overview](#ddf-format-overview)
3. [Data Layer](#data-layer)
4. [Shape Layer](#shape-layer)
5. [CSS Styling](#css-styling)
6. [Component System](#component-system)
7. [Smart Connectors](#smart-connectors)
8. [Expression Language](#expression-language)
9. [Events and Interactions](#events-and-interactions)
10. [Layout Algorithms](#layout-algorithms)
11. [Examples](#examples)

## Getting Started

### Basic DDF Document Structure

A DDF document is a JSON file with the following structure:

```json
{
  "version": "1.0",
  "metadata": {
    "title": "My Diagram",
    "author": "Your Name"
  },
  "data": { ... },
  "styles": { ... },
  "shapes": [ ... ],
  "components": { ... },
  "connectors": [ ... ],
  "events": [ ... ],
  "layouts": [ ... ]
}
```

### Creating Your First Diagram

Here's a minimal example:

```json
{
  "version": "1.0",
  "shapes": [
    {
      "id": "rect1",
      "type": "rect",
      "geometry": {
        "x": 100,
        "y": 100,
        "width": 120,
        "height": 60
      },
      "style": {
        "fill": "#3498db",
        "stroke": "#2c3e50",
        "stroke-width": "2"
      }
    }
  ]
}
```

## DDF Format Overview

DDF consists of six main layers:

1. **Data Layer**: Structured data (nodes and relationships)
2. **Shape Layer**: Visual primitives organized in a hierarchical tree
3. **Style Layer**: CSS-like stylesheets for reusable styling
4. **Component Layer**: Reusable diagram components
5. **Connector Layer**: Smart routing between shapes
6. **Event Layer**: Interactive behaviors

## Data Layer

The data layer stores structured information that drives diagram generation.

### Defining Data Nodes

```json
{
  "data": {
    "nodes": [
      {
        "id": "node1",
        "type": "person",
        "properties": {
          "name": "John Doe",
          "title": "CEO",
          "department": "Executive"
        }
      }
    ]
  }
}
```

### Defining Relationships

```json
{
  "data": {
    "relationships": [
      {
        "id": "rel1",
        "type": "reports_to",
        "from": "node2",
        "to": "node1",
        "properties": {
          "relationship_type": "direct"
        }
      }
    ]
  }
}
```

### Data Binding

Bind shapes to data nodes to create data-driven diagrams:

```json
{
  "shapes": [
    {
      "id": "shape1",
      "type": "rect",
      "data_binding": {
        "node_id": "node1",
        "property_mappings": {
          "text": "{{name}}",
          "fill": "{{department | color_map}}"
        }
      }
    }
  ]
}
```

## Shape Layer

The shape layer defines visual primitives organized in a hierarchical tree structure (similar to HTML DOM).

### Supported Shape Types

- `rect`: Rectangle
- `circle`: Circle
- `ellipse`: Ellipse
- `path`: Custom path (SVG path syntax)
- `text`: Text element
- `group`: Container for other shapes

### Basic Shape Definition

```json
{
  "id": "rect1",
  "type": "rect",
  "geometry": {
    "x": 100,
    "y": 100,
    "width": 120,
    "height": 60
  },
  "style": {
    "fill": "#3498db",
    "stroke": "#2c3e50",
    "stroke-width": "2",
    "rx": "8",
    "ry": "8"
  }
}
```

### Hierarchical Shapes (Groups)

```json
{
  "id": "group1",
  "type": "group",
  "transform": "translate(100, 100)",
  "children": [
    {
      "id": "rect1",
      "type": "rect",
      "geometry": {"x": 0, "y": 0, "width": 120, "height": 60}
    },
    {
      "id": "text1",
      "type": "text",
      "geometry": {"x": 60, "y": 30},
      "text": "Hello"
    }
  ]
}
```

### Connection Points

Define connection points for smart connectors:

```json
{
  "connection_points": [
    {"id": "top", "x": 0.5, "y": 0},
    {"id": "bottom", "x": 0.5, "y": 1},
    {"id": "left", "x": 0, "y": 0.5},
    {"id": "right", "x": 1, "y": 0.5}
  ]
}
```

## CSS Styling

DDF uses a CSS-like styling system with selectors, cascade, and inheritance.

### Stylesheet Rules

```json
{
  "styles": {
    "rules": [
      {
        "selector": ".card",
        "properties": {
          "fill": "#3498db",
          "stroke": "#2c3e50",
          "stroke-width": "2"
        }
      }
    ]
  }
}
```

### Selector Types

- **Type selector**: `rect`, `circle`, `text`
- **Class selector**: `.card`, `.primary`
- **ID selector**: `#shape1`
- **Pseudo-class selector**: `:hover`, `:selected`, `:active`
- **Combined selectors**: `rect.card`, `.card:hover`

### Style Cascade

Styles are applied in order of specificity:

1. Default styles (lowest priority)
2. Type selectors (`rect`)
3. Class selectors (`.card`)
4. ID selectors (`#shape1`)
5. Inline styles (highest priority)

### Pseudo-States

DDF supports CSS-like pseudo-states:

- `:hover` - Mouse is over the element
- `:selected` - Element is selected
- `:active` - Element is being clicked
- `:focus` - Element has focus

```json
{
  "selector": ".card:hover",
  "properties": {
    "fill": "#2980b9",
    "stroke-width": "3"
  }
}
```

### Style Inheritance

Certain properties inherit from parent to child:

- `color`
- `font-family`
- `font-size`
- `font-weight`
- `text-align`
- `opacity`

## Component System

Components are reusable diagram elements with parameters.

### Defining a Component

```json
{
  "components": {
    "definitions": [
      {
        "id": "person_card",
        "name": "Person Card",
        "parameters": [
          {"name": "name", "type": "string", "default": ""},
          {"name": "title", "type": "string", "default": ""},
          {"name": "color", "type": "color", "default": "#3498db"}
        ],
        "shapes": [
          {
            "type": "rect",
            "geometry": {"x": 0, "y": 0, "width": 120, "height": 60},
            "style": {"fill": "{{color}}"}
          },
          {
            "type": "text",
            "geometry": {"x": 60, "y": 30},
            "text": "{{name}}"
          }
        ],
        "connection_points": [
          {"id": "top", "x": 0.5, "y": 0},
          {"id": "bottom", "x": 0.5, "y": 1}
        ]
      }
    ]
  }
}
```

### Creating Component Instances

```json
{
  "components": {
    "instances": [
      {
        "id": "instance1",
        "component_id": "person_card",
        "parameters": {
          "name": "John Doe",
          "title": "CEO",
          "color": "#e74c3c"
        },
        "position": {"x": 100, "y": 100}
      }
    ]
  }
}
```

### Component Parameters

Supported parameter types:

- `string`: Text value
- `number`: Numeric value
- `color`: Color value (hex, rgb, etc.)
- `boolean`: True/false value

### Component Updates

When a component definition is updated, all instances automatically reflect the changes (unless overridden).

## Smart Connectors

Connectors automatically route between shapes with intelligent pathfinding.

### Basic Connector

```json
{
  "connectors": [
    {
      "id": "conn1",
      "from": {
        "shape_id": "shape1",
        "connection_point": "bottom"
      },
      "to": {
        "shape_id": "shape2",
        "connection_point": "top"
      },
      "routing": {
        "algorithm": "orthogonal",
        "avoid_shapes": true,
        "padding": 10
      },
      "style": {
        "stroke": "#2c3e50",
        "stroke-width": "2"
      },
      "arrow_end": "arrow"
    }
  ]
}
```

### Routing Algorithms

1. **Straight**: Direct line between points
2. **Orthogonal**: Right-angle routing with obstacle avoidance
3. **Bezier**: Smooth curved routing
4. **Curved Orthogonal**: Orthogonal with rounded corners

### Arrow Types

- `none`: No arrow
- `arrow`: Standard arrow
- `diamond`: Diamond shape
- `circle`: Circle shape
- `square`: Square shape

### Connector Labels

```json
{
  "label": {
    "text": "reports to",
    "position": 0.5,
    "offset": {"x": 0, "y": -10}
  }
}
```

### Automatic Re-routing

Connectors automatically re-route when connected shapes move.

## Expression Language

DDF includes a powerful expression language for dynamic values.

### Basic Syntax

Expressions are enclosed in double curly braces: `{{expression}}`

### Variable Access

```
{{data.name}}           // Access data property
{{parent.width}}        // Access parent property
{{index}}               // Current index in iteration
{{canvas.width}}        // Canvas dimensions
{{selected}}            // Selection state
```

### Arithmetic Operations

```
{{width + 10}}
{{height * 2}}
{{x - padding}}
{{count / 2}}
{{index % 2}}
```

### Comparison Operations

```
{{count > 10}}
{{status == "online"}}
{{value != 0}}
{{age >= 18}}
```

### Logical Operations

```
{{enabled && visible}}
{{error || warning}}
{{!hidden}}
```

### Ternary Operator

```
{{status == "online" ? "#27ae60" : "#e74c3c"}}
{{count > 0 ? "visible" : "hidden"}}
```

### Filters

Filters transform values using the pipe operator:

```
{{name | uppercase}}                    // Convert to uppercase
{{value | round}}                       // Round to integer
{{department | color_map}}              // Map to color
{{text | truncate(20)}}                 // Truncate to 20 chars
{{date | format("YYYY-MM-DD")}}         // Format date
```

### Built-in Filters

- `uppercase`: Convert to uppercase
- `lowercase`: Convert to lowercase
- `capitalize`: Capitalize first letter
- `round`: Round to integer
- `floor`: Round down
- `ceil`: Round up
- `abs`: Absolute value
- `truncate(n)`: Truncate to n characters
- `format(pattern)`: Format value
- `color_map`: Map value to color
- `default(value)`: Default value if empty

## Events and Interactions

Define interactive behaviors declaratively.

### Event Definition

```json
{
  "events": [
    {
      "id": "event1",
      "target": "shape1",
      "trigger": "click",
      "actions": [
        {
          "type": "update_data",
          "node_id": "node1",
          "property": "selected",
          "value": "true"
        }
      ]
    }
  ]
}
```

### Event Triggers

- `click`: Mouse click
- `double_click`: Double click
- `right_click`: Right mouse button
- `hover`: Mouse enters element
- `hover_end`: Mouse leaves element
- `drag_start`: Drag begins
- `drag`: During drag
- `drag_end`: Drag ends
- `select`: Element selected
- `deselect`: Element deselected

### Action Types

- `update_data`: Update data node property
- `update_style`: Update shape style
- `update_geometry`: Update shape geometry
- `show_tooltip`: Show tooltip
- `navigate`: Navigate to URL
- `execute_script`: Execute custom script
- `emit_custom_event`: Emit custom event

### Conditional Events

```json
{
  "trigger": "click",
  "condition": "{{data.enabled == true}}",
  "actions": [ ... ]
}
```

### Event Targets

- Specific shape: `"shape1"`
- All shapes: `"*"`
- Class selector: `".card"`
- Type selector: `"rect"`

## Layout Algorithms

Generate diagrams automatically from data.

### Tree Layout

```json
{
  "layouts": [
    {
      "id": "tree_layout",
      "algorithm": "tree",
      "parameters": {
        "direction": "top-down",
        "horizontal_spacing": 50,
        "vertical_spacing": 100,
        "root_node": "node1"
      }
    }
  ]
}
```

**Directions**: `top-down`, `bottom-up`, `left-right`, `right-left`

### Force-Directed Layout

```json
{
  "algorithm": "force-directed",
  "parameters": {
    "iterations": 100,
    "repulsion": 200,
    "attraction": 0.1,
    "damping": 0.9
  }
}
```

### Grid Layout

```json
{
  "algorithm": "grid",
  "parameters": {
    "columns": 4,
    "horizontal_spacing": 50,
    "vertical_spacing": 50
  }
}
```

## Examples

### Example 1: Simple Organization Chart

See `examples/ddf/org_chart.json` for a complete organization chart example.

### Example 2: Network Diagram

See `examples/ddf/network_diagram.json` for a network topology diagram.

### Example 3: Flowchart

See `examples/ddf/flowchart.json` for a user authentication flowchart.

## Best Practices

1. **Use Components**: Create reusable components for repeated elements
2. **Leverage CSS Classes**: Use classes for consistent styling
3. **Data-Driven**: Bind shapes to data for dynamic diagrams
4. **Smart Connectors**: Use orthogonal routing for professional diagrams
5. **Expression Language**: Use expressions for dynamic values
6. **Hierarchical Shapes**: Group related shapes for easier management
7. **Connection Points**: Define clear connection points on components
8. **Validation**: Validate your DDF documents before use

## Troubleshooting

### Common Issues

**Issue**: Shapes not appearing
- Check geometry values are valid
- Verify shapes are within viewport
- Check fill/stroke properties

**Issue**: Connectors not routing correctly
- Verify connection points exist
- Check shape IDs are correct
- Ensure routing algorithm is appropriate

**Issue**: Data binding not working
- Verify data node IDs match
- Check expression syntax
- Ensure property mappings are correct

**Issue**: Styles not applying
- Check selector syntax
- Verify class names match
- Check specificity order

## Next Steps

- Explore the [API Documentation](API_REFERENCE.md)
- Review [Example Diagrams](../../examples/ddf/)
- Learn about [CSS Styling](CSS_STYLING.md)
- Understand [Expression Language](EXPRESSIONS.md)
- Study [Component System](COMPONENTS.md)
