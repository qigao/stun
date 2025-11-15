# DDF Component System Guide

## Overview

The DDF Component System allows you to create reusable diagram elements with parameters. Components encapsulate shapes, styles, and behavior into reusable units that can be instantiated multiple times with different parameter values.

## Table of Contents

1. [Basic Concepts](#basic-concepts)
2. [Defining Components](#defining-components)
3. [Component Parameters](#component-parameters)
4. [Creating Instances](#creating-instances)
5. [Component Updates](#component-updates)
6. [Advanced Features](#advanced-features)
7. [Examples](#examples)

## Basic Concepts

### What is a Component?

A component is a reusable diagram element that:
- Encapsulates one or more shapes
- Accepts parameters for customization
- Defines connection points for connectors
- Can be instantiated multiple times
- Updates all instances when the definition changes

### Component vs Shape

- **Shape**: A single visual primitive (rect, circle, text, etc.)
- **Component**: A collection of shapes with parameters and behavior

### Use Cases

- Organization chart person cards
- Network device icons
- Flowchart symbols
- UI mockup elements
- Reusable diagram patterns

## Defining Components

### Basic Component Definition

```json
{
  "components": {
    "definitions": [
      {
        "id": "person_card",
        "name": "Person Card",
        "parameters": [
          {"name": "name", "type": "string", "default": ""},
          {"name": "title", "type": "string", "default": ""}
        ],
        "shapes": [
          {
            "type": "rect",
            "geometry": {"x": 0, "y": 0, "width": 120, "height": 60},
            "style": {"fill": "#3498db"}
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

### Component Structure

```json
{
  "id": "unique_id",              // Unique component identifier
  "name": "Display Name",         // Human-readable name
  "description": "...",           // Optional description
  "parameters": [...],            // Component parameters
  "shapes": [...],                // Shape definitions
  "connection_points": [...],     // Connection points for connectors
  "metadata": {...}               // Optional metadata
}
```

## Component Parameters

### Parameter Types

DDF supports four parameter types:

1. **string**: Text values
2. **number**: Numeric values
3. **color**: Color values (hex, rgb, etc.)
4. **boolean**: True/false values

### Parameter Definition

```json
{
  "name": "parameter_name",
  "type": "string",
  "default": "default_value",
  "description": "Parameter description",
  "validation": {
    "required": true,
    "min": 0,
    "max": 100,
    "pattern": "^[A-Z].*"
  }
}
```

### String Parameters

```json
{
  "name": "title",
  "type": "string",
  "default": "Untitled",
  "description": "Card title text"
}
```

Usage in shapes:

```json
{
  "text": "{{title}}"
}
```

### Number Parameters

```json
{
  "name": "width",
  "type": "number",
  "default": 120,
  "validation": {
    "min": 50,
    "max": 300
  }
}
```

Usage in shapes:

```json
{
  "geometry": {
    "width": "{{width}}"
  }
}
```

### Color Parameters

```json
{
  "name": "color",
  "type": "color",
  "default": "#3498db"
}
```

Usage in shapes:

```json
{
  "style": {
    "fill": "{{color}}"
  }
}
```

### Boolean Parameters

```json
{
  "name": "show_icon",
  "type": "boolean",
  "default": true
}
```

Usage in shapes:

```json
{
  "style": {
    "opacity": "{{show_icon ? 1 : 0}}"
  }
}
```

### Parameter Expressions

Parameters can be used in expressions:

```json
{
  "geometry": {
    "width": "{{width * 2}}",
    "height": "{{width / 2}}"
  },
  "text": "{{name | uppercase}}",
  "style": {
    "fill": "{{enabled ? color : '#cccccc'}}"
  }
}
```

## Creating Instances

### Basic Instance

```json
{
  "components": {
    "instances": [
      {
        "id": "instance1",
        "component_id": "person_card",
        "parameters": {
          "name": "John Doe",
          "title": "CEO"
        },
        "position": {"x": 100, "y": 100}
      }
    ]
  }
}
```

### Instance Structure

```json
{
  "id": "unique_instance_id",     // Unique instance identifier
  "component_id": "person_card",  // Reference to component definition
  "parameters": {...},            // Parameter values
  "position": {"x": 0, "y": 0},   // Instance position
  "transform": "...",             // Optional transform
  "overrides": {...},             // Property overrides
  "detached": false               // Whether instance is detached
}
```

### Multiple Instances

```json
{
  "instances": [
    {
      "id": "ceo",
      "component_id": "person_card",
      "parameters": {"name": "Sarah Johnson", "title": "CEO"},
      "position": {"x": 200, "y": 50}
    },
    {
      "id": "cto",
      "component_id": "person_card",
      "parameters": {"name": "Michael Chen", "title": "CTO"},
      "position": {"x": 100, "y": 200}
    },
    {
      "id": "cfo",
      "component_id": "person_card",
      "parameters": {"name": "Emily Rodriguez", "title": "CFO"},
      "position": {"x": 300, "y": 200}
    }
  ]
}
```

### Instance Positioning

Instances can be positioned using:

1. **Absolute position**: `{"x": 100, "y": 100}`
2. **Transform**: `"transform": "translate(100, 100) rotate(45)"`
3. **Layout algorithm**: Automatically positioned by layout

## Component Updates

### Updating Component Definition

When you update a component definition, all instances automatically reflect the changes:

```json
// Before
{
  "id": "person_card",
  "shapes": [
    {"type": "rect", "style": {"fill": "#3498db"}}
  ]
}

// After - all instances update automatically
{
  "id": "person_card",
  "shapes": [
    {"type": "rect", "style": {"fill": "#e74c3c"}}
  ]
}
```

### Instance Overrides

Instances can override specific properties:

```json
{
  "id": "instance1",
  "component_id": "person_card",
  "parameters": {"name": "John Doe"},
  "overrides": {
    "shapes[0].style.fill": "#e74c3c"
  }
}
```

Overrides persist even when the component definition changes.

### Detaching Instances

Detach an instance to break the link to the component:

```json
{
  "id": "instance1",
  "component_id": "person_card",
  "detached": true
}
```

Detached instances:
- No longer update when component changes
- Become independent shapes
- Can be edited freely

## Advanced Features

### Nested Components

Components can contain other component instances:

```json
{
  "id": "team_card",
  "shapes": [
    {
      "type": "component_instance",
      "component_id": "person_card",
      "parameters": {"name": "{{manager_name}}"}
    }
  ]
}
```

### Component Inheritance

Components can inherit from other components:

```json
{
  "id": "executive_card",
  "parent_component_id": "person_card",
  "parameters": [
    {"name": "department", "type": "string", "default": ""}
  ],
  "shapes": [
    // Inherits shapes from person_card
    {
      "type": "text",
      "geometry": {"x": 60, "y": 50},
      "text": "{{department}}"
    }
  ]
}
```

### Dynamic Shape Count

Use expressions to conditionally include shapes:

```json
{
  "shapes": [
    {
      "type": "circle",
      "style": {
        "opacity": "{{show_icon ? 1 : 0}}"
      }
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
    {"id": "right", "x": 1, "y": 0.5},
    {"id": "top-left", "x": 0.25, "y": 0},
    {"id": "top-right", "x": 0.75, "y": 0}
  ]
}
```

Connection points use relative coordinates (0-1).

### Component Metadata

Store additional information:

```json
{
  "id": "person_card",
  "metadata": {
    "category": "org-chart",
    "tags": ["people", "organization"],
    "version": "1.0",
    "author": "Your Name",
    "icon": "person.svg"
  }
}
```

## Examples

### Example 1: Network Device Component

```json
{
  "id": "network_device",
  "name": "Network Device",
  "parameters": [
    {"name": "device_name", "type": "string", "default": "Device"},
    {"name": "ip_address", "type": "string", "default": "0.0.0.0"},
    {"name": "device_type", "type": "string", "default": "server"},
    {"name": "status", "type": "string", "default": "online"}
  ],
  "shapes": [
    {
      "type": "rect",
      "class": "device {{device_type}}",
      "geometry": {"x": 0, "y": 0, "width": 100, "height": 80},
      "style": {
        "fill": "{{status == 'online' ? '#2ecc71' : '#e74c3c'}}",
        "rx": "5",
        "ry": "5"
      }
    },
    {
      "type": "text",
      "class": "device-name",
      "geometry": {"x": 50, "y": 30},
      "text": "{{device_name}}"
    },
    {
      "type": "text",
      "class": "device-ip",
      "geometry": {"x": 50, "y": 50},
      "text": "{{ip_address}}",
      "style": {"font-size": "10px"}
    }
  ],
  "connection_points": [
    {"id": "top", "x": 0.5, "y": 0},
    {"id": "bottom", "x": 0.5, "y": 1},
    {"id": "left", "x": 0, "y": 0.5},
    {"id": "right", "x": 1, "y": 0.5}
  ]
}
```

### Example 2: Flowchart Decision Node

```json
{
  "id": "decision_node",
  "name": "Decision Node",
  "parameters": [
    {"name": "question", "type": "string", "default": "Decision?"},
    {"name": "size", "type": "number", "default": 80}
  ],
  "shapes": [
    {
      "type": "path",
      "class": "flowchart-decision",
      "geometry": {
        "d": "M {{size/2}},0 L {{size}},{{size/2}} L {{size/2}},{{size}} L 0,{{size/2}} Z"
      },
      "style": {
        "fill": "#f39c12",
        "stroke": "#d68910"
      }
    },
    {
      "type": "text",
      "geometry": {"x": "{{size/2}}", "y": "{{size/2}}"},
      "text": "{{question}}",
      "style": {
        "text-anchor": "middle",
        "alignment-baseline": "middle"
      }
    }
  ],
  "connection_points": [
    {"id": "top", "x": 0.5, "y": 0},
    {"id": "bottom", "x": 0.5, "y": 1},
    {"id": "left", "x": 0, "y": 0.5},
    {"id": "right", "x": 1, "y": 0.5}
  ]
}
```

### Example 3: Card with Icon

```json
{
  "id": "icon_card",
  "name": "Icon Card",
  "parameters": [
    {"name": "title", "type": "string", "default": "Title"},
    {"name": "subtitle", "type": "string", "default": ""},
    {"name": "icon", "type": "string", "default": "📄"},
    {"name": "color", "type": "color", "default": "#3498db"},
    {"name": "show_icon", "type": "boolean", "default": true}
  ],
  "shapes": [
    {
      "type": "rect",
      "class": "card",
      "geometry": {"x": 0, "y": 0, "width": 200, "height": 100},
      "style": {
        "fill": "#ffffff",
        "stroke": "{{color}}",
        "rx": "8",
        "ry": "8"
      }
    },
    {
      "type": "rect",
      "class": "card-header",
      "geometry": {"x": 0, "y": 0, "width": 200, "height": 30},
      "style": {
        "fill": "{{color}}",
        "rx": "8",
        "ry": "8"
      }
    },
    {
      "type": "text",
      "geometry": {"x": 20, "y": 20},
      "text": "{{icon}}",
      "style": {
        "font-size": "16px",
        "opacity": "{{show_icon ? 1 : 0}}"
      }
    },
    {
      "type": "text",
      "class": "card-title",
      "geometry": {"x": "{{show_icon ? 45 : 20}}", "y": 20},
      "text": "{{title}}",
      "style": {
        "fill": "#ffffff",
        "font-weight": "bold"
      }
    },
    {
      "type": "text",
      "class": "card-subtitle",
      "geometry": {"x": 20, "y": 55},
      "text": "{{subtitle}}"
    }
  ]
}
```

### Example 4: Progress Bar Component

```json
{
  "id": "progress_bar",
  "name": "Progress Bar",
  "parameters": [
    {"name": "progress", "type": "number", "default": 0.5},
    {"name": "width", "type": "number", "default": 200},
    {"name": "height", "type": "number", "default": 20},
    {"name": "label", "type": "string", "default": ""}
  ],
  "shapes": [
    {
      "type": "rect",
      "class": "progress-background",
      "geometry": {"x": 0, "y": 0, "width": "{{width}}", "height": "{{height}}"},
      "style": {
        "fill": "#ecf0f1",
        "stroke": "#bdc3c7",
        "rx": "{{height/2}}",
        "ry": "{{height/2}}"
      }
    },
    {
      "type": "rect",
      "class": "progress-fill",
      "geometry": {
        "x": 0,
        "y": 0,
        "width": "{{progress * width}}",
        "height": "{{height}}"
      },
      "style": {
        "fill": "{{progress < 0.5 ? '#e74c3c' : progress < 0.8 ? '#f39c12' : '#27ae60'}}",
        "rx": "{{height/2}}",
        "ry": "{{height/2}}"
      }
    },
    {
      "type": "text",
      "geometry": {"x": "{{width/2}}", "y": "{{height/2}}"},
      "text": "{{label != '' ? label : (progress * 100 | fixed(0)) + '%'}}",
      "style": {
        "text-anchor": "middle",
        "alignment-baseline": "middle",
        "font-size": "12px",
        "font-weight": "bold"
      }
    }
  ]
}
```

## Best Practices

1. **Clear Parameters**: Use descriptive parameter names and provide defaults
2. **Validation**: Add validation rules to parameters
3. **Connection Points**: Always define connection points for connectors
4. **Relative Sizing**: Use relative coordinates for flexibility
5. **Documentation**: Add descriptions to components and parameters
6. **Reusability**: Design components to be reusable across diagrams
7. **Composition**: Build complex components from simpler ones
8. **Testing**: Test components with various parameter values

## Common Patterns

### Conditional Shapes

```json
{
  "shapes": [
    {
      "type": "circle",
      "style": {
        "display": "{{show_badge ? 'block' : 'none'}}"
      }
    }
  ]
}
```

### Responsive Sizing

```json
{
  "parameters": [
    {"name": "size", "type": "string", "default": "medium"}
  ],
  "shapes": [
    {
      "geometry": {
        "width": "{{size == 'small' ? 80 : size == 'large' ? 160 : 120}}"
      }
    }
  ]
}
```

### State-Based Styling

```json
{
  "parameters": [
    {"name": "state", "type": "string", "default": "normal"}
  ],
  "shapes": [
    {
      "style": {
        "fill": "{{state == 'error' ? '#e74c3c' : state == 'warning' ? '#f39c12' : '#3498db'}}"
      }
    }
  ]
}
```

## Troubleshooting

### Component Not Rendering

1. Check component ID is unique
2. Verify all required parameters have defaults
3. Check shape geometry is valid
4. Ensure expressions are correct

### Instance Not Updating

1. Verify component_id matches definition
2. Check instance is not detached
3. Ensure parameters are valid
4. Clear any conflicting overrides

### Parameter Not Working

1. Check parameter name matches usage
2. Verify parameter type is correct
3. Ensure default value is provided
4. Check expression syntax

## Next Steps

- Review [User Guide](USER_GUIDE.md)
- Learn about [Expression Language](EXPRESSIONS.md)
- Study [CSS Styling](CSS_STYLING.md)
- Explore [Component Examples](../../examples/ddf/)
