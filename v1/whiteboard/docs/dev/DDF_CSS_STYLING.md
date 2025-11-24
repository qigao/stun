# DDF CSS Styling Guide

## Overview

DDF uses a CSS-inspired styling system that provides familiar concepts like selectors, cascade, inheritance, and pseudo-states. This makes it easy to create consistent, maintainable styles for your diagrams.

## Table of Contents

1. [Basic Concepts](#basic-concepts)
2. [Selectors](#selectors)
3. [Style Properties](#style-properties)
4. [Cascade and Specificity](#cascade-and-specificity)
5. [Inheritance](#inheritance)
6. [Pseudo-States](#pseudo-states)
7. [Advanced Techniques](#advanced-techniques)
8. [Examples](#examples)

## Basic Concepts

### Stylesheet Structure

Stylesheets in DDF are defined in the `styles` section:

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

### Applying Classes to Shapes

```json
{
  "id": "shape1",
  "type": "rect",
  "class": "card primary",
  "geometry": { ... }
}
```

### Inline Styles

Inline styles have the highest priority:

```json
{
  "id": "shape1",
  "type": "rect",
  "class": "card",
  "style": {
    "fill": "#e74c3c"
  }
}
```

## Selectors

### Type Selectors

Match shapes by type:

```json
{
  "selector": "rect",
  "properties": {
    "fill": "#ffffff",
    "stroke": "#cbd5e0"
  }
}
```

Matches all `rect` shapes.

### Class Selectors

Match shapes by class:

```json
{
  "selector": ".card",
  "properties": {
    "fill": "#3498db"
  }
}
```

Matches shapes with `class="card"` or `class="card primary"`.

### ID Selectors

Match a specific shape by ID:

```json
{
  "selector": "#shape1",
  "properties": {
    "fill": "#e74c3c"
  }
}
```

Matches the shape with `id="shape1"`.

### Pseudo-Class Selectors

Match shapes in specific states:

```json
{
  "selector": ":hover",
  "properties": {
    "stroke-width": "3"
  }
}
```

Available pseudo-classes:
- `:hover` - Mouse is over the element
- `:selected` - Element is selected
- `:active` - Element is being clicked/dragged
- `:focus` - Element has keyboard focus

### Combined Selectors

Combine selectors for more specific matching:

```json
{
  "selector": "rect.card",
  "properties": {
    "rx": "8",
    "ry": "8"
  }
}
```

Matches `rect` shapes with class `card`.

```json
{
  "selector": ".card:hover",
  "properties": {
    "fill": "#2980b9"
  }
}
```

Matches shapes with class `card` when hovered.

### Universal Selector

Match all shapes:

```json
{
  "selector": "*",
  "properties": {
    "font-family": "Inter, sans-serif"
  }
}
```

## Style Properties

### Fill Properties

```json
{
  "fill": "#3498db",           // Solid color
  "fill-opacity": "0.8"        // Fill opacity (0-1)
}
```

### Stroke Properties

```json
{
  "stroke": "#2c3e50",         // Stroke color
  "stroke-width": "2",         // Stroke width
  "stroke-opacity": "1",       // Stroke opacity (0-1)
  "stroke-dasharray": "5,5",   // Dashed line pattern
  "stroke-linecap": "round",   // Line cap style
  "stroke-linejoin": "round"   // Line join style
}
```

### Text Properties

```json
{
  "fill": "#2c3e50",           // Text color
  "font-family": "Inter",      // Font family
  "font-size": "14px",         // Font size
  "font-weight": "bold",       // Font weight
  "font-style": "italic",      // Font style
  "text-anchor": "middle",     // Horizontal alignment
  "alignment-baseline": "middle" // Vertical alignment
}
```

### Transform Properties

```json
{
  "transform": "translate(10, 20) rotate(45) scale(1.5)"
}
```

### Opacity

```json
{
  "opacity": "0.8"             // Overall opacity (0-1)
}
```

### Border Radius (for rectangles)

```json
{
  "rx": "8",                   // Horizontal radius
  "ry": "8"                    // Vertical radius
}
```

### Filter Effects

```json
{
  "filter": "drop-shadow(0 2px 4px rgba(0,0,0,0.1))"
}
```

## Cascade and Specificity

### Specificity Calculation

Specificity determines which style rule wins when multiple rules match:

1. **Inline styles**: Highest priority (specificity = 1000)
2. **ID selectors**: `#shape1` (specificity = 100)
3. **Class selectors**: `.card`, `:hover` (specificity = 10)
4. **Type selectors**: `rect` (specificity = 1)
5. **Universal selector**: `*` (specificity = 0)

### Specificity Examples

```
*                    → 0
rect                 → 1
.card                → 10
rect.card            → 11
#shape1              → 100
.card:hover          → 20
rect.card:hover      → 21
```

### Cascade Order

When specificity is equal, the last rule wins:

```json
{
  "rules": [
    {
      "selector": ".card",
      "properties": {"fill": "#3498db"}
    },
    {
      "selector": ".card",
      "properties": {"fill": "#e74c3c"}  // This wins
    }
  ]
}
```

### Style Resolution Example

Given this shape:

```json
{
  "id": "shape1",
  "type": "rect",
  "class": "card primary",
  "style": {"stroke-width": "4"}
}
```

And these rules:

```json
{
  "rules": [
    {"selector": "*", "properties": {"fill": "#ffffff"}},
    {"selector": "rect", "properties": {"fill": "#ecf0f1"}},
    {"selector": ".card", "properties": {"fill": "#3498db"}},
    {"selector": "#shape1", "properties": {"fill": "#e74c3c"}}
  ]
}
```

Final computed style:
- `fill`: `#e74c3c` (from `#shape1`, specificity 100)
- `stroke-width`: `4` (from inline style, specificity 1000)

## Inheritance

Certain properties inherit from parent to child shapes.

### Inheritable Properties

- `color`
- `font-family`
- `font-size`
- `font-weight`
- `font-style`
- `text-align`
- `line-height`
- `opacity`

### Inheritance Example

```json
{
  "id": "group1",
  "type": "group",
  "style": {
    "font-family": "Inter",
    "font-size": "14px",
    "opacity": "0.9"
  },
  "children": [
    {
      "id": "text1",
      "type": "text",
      "text": "Hello"
      // Inherits font-family, font-size, and opacity from parent
    }
  ]
}
```

### Overriding Inherited Properties

Child shapes can override inherited properties:

```json
{
  "id": "text1",
  "type": "text",
  "style": {
    "font-size": "16px"  // Overrides inherited font-size
  }
}
```

## Pseudo-States

Pseudo-states allow styling based on interaction state.

### Hover State

```json
{
  "selector": ".card:hover",
  "properties": {
    "fill": "#2980b9",
    "stroke-width": "3",
    "filter": "drop-shadow(0 4px 8px rgba(0,0,0,0.2))"
  }
}
```

### Selected State

```json
{
  "selector": ".card:selected",
  "properties": {
    "stroke": "#e74c3c",
    "stroke-width": "4"
  }
}
```

### Active State

```json
{
  "selector": ".card:active",
  "properties": {
    "opacity": "0.8",
    "transform": "scale(0.98)"
  }
}
```

### Focus State

```json
{
  "selector": ".card:focus",
  "properties": {
    "stroke": "#3182ce",
    "stroke-dasharray": "4,2"
  }
}
```

### Multiple Pseudo-States

You can style combinations of states:

```json
{
  "selector": ".card:hover:selected",
  "properties": {
    "fill": "#c0392b"
  }
}
```

## Advanced Techniques

### Theme Switching

Create multiple stylesheets for different themes:

```json
{
  "styles": {
    "themes": {
      "light": {
        "rules": [ ... ]
      },
      "dark": {
        "rules": [ ... ]
      }
    },
    "active_theme": "light"
  }
}
```

### Responsive Styles

Use expressions for responsive styling:

```json
{
  "selector": ".card",
  "properties": {
    "width": "{{canvas.width < 600 ? 100 : 150}}"
  }
}
```

### Color Palettes

Define color variables:

```json
{
  "styles": {
    "variables": {
      "primary": "#3498db",
      "secondary": "#2ecc71",
      "danger": "#e74c3c"
    },
    "rules": [
      {
        "selector": ".primary",
        "properties": {"fill": "var(--primary)"}
      }
    ]
  }
}
```

### Conditional Styling

Use expressions for conditional styles:

```json
{
  "style": {
    "fill": "{{data.status == 'online' ? '#27ae60' : '#e74c3c'}}"
  }
}
```

### Animation Classes

Define animation states:

```json
{
  "selector": ".fade-in",
  "properties": {
    "opacity": "0",
    "transition": "opacity 0.3s ease-in"
  }
}

{
  "selector": ".fade-in:active",
  "properties": {
    "opacity": "1"
  }
}
```

## Examples

### Example 1: Card Component Styling

```json
{
  "rules": [
    {
      "selector": ".card",
      "properties": {
        "fill": "#ffffff",
        "stroke": "#e2e8f0",
        "stroke-width": "1",
        "rx": "8",
        "ry": "8",
        "filter": "drop-shadow(0 1px 3px rgba(0,0,0,0.1))"
      }
    },
    {
      "selector": ".card:hover",
      "properties": {
        "fill": "#f7fafc",
        "stroke": "#cbd5e0",
        "stroke-width": "2",
        "filter": "drop-shadow(0 4px 6px rgba(0,0,0,0.1))"
      }
    },
    {
      "selector": ".card:selected",
      "properties": {
        "stroke": "#3182ce",
        "stroke-width": "3"
      }
    },
    {
      "selector": ".card-title",
      "properties": {
        "fill": "#1a202c",
        "font-size": "14px",
        "font-weight": "bold"
      }
    }
  ]
}
```

### Example 2: Status Indicators

```json
{
  "rules": [
    {
      "selector": ".status",
      "properties": {
        "stroke": "none"
      }
    },
    {
      "selector": ".status-online",
      "properties": {
        "fill": "#48bb78"
      }
    },
    {
      "selector": ".status-offline",
      "properties": {
        "fill": "#f56565"
      }
    },
    {
      "selector": ".status-warning",
      "properties": {
        "fill": "#ed8936"
      }
    }
  ]
}
```

### Example 3: Connector Styling

```json
{
  "rules": [
    {
      "selector": ".connector",
      "properties": {
        "stroke": "#a0aec0",
        "stroke-width": "2",
        "fill": "none"
      }
    },
    {
      "selector": ".connector:hover",
      "properties": {
        "stroke": "#4a5568",
        "stroke-width": "3"
      }
    },
    {
      "selector": ".connector-dashed",
      "properties": {
        "stroke-dasharray": "5,5"
      }
    },
    {
      "selector": ".connector-label",
      "properties": {
        "fill": "#4a5568",
        "font-size": "10px",
        "font-style": "italic"
      }
    }
  ]
}
```

## Best Practices

1. **Use Classes**: Prefer classes over inline styles for reusability
2. **Organize by Component**: Group related styles together
3. **Consistent Naming**: Use consistent class naming conventions
4. **Leverage Cascade**: Use specificity to your advantage
5. **Pseudo-States**: Always style hover and selected states
6. **Color Palette**: Define a consistent color palette
7. **Typography**: Set base typography on universal selector
8. **Performance**: Avoid overly complex selectors

## Common Patterns

### Button-like Elements

```json
{
  "selector": ".button",
  "properties": {
    "fill": "#3498db",
    "stroke": "#2980b9",
    "rx": "4",
    "ry": "4"
  }
},
{
  "selector": ".button:hover",
  "properties": {
    "fill": "#2980b9"
  }
},
{
  "selector": ".button:active",
  "properties": {
    "transform": "scale(0.95)"
  }
}
```

### Disabled State

```json
{
  "selector": ".disabled",
  "properties": {
    "opacity": "0.5",
    "pointer-events": "none"
  }
}
```

### Highlight Effect

```json
{
  "selector": ".highlight",
  "properties": {
    "stroke": "#f39c12",
    "stroke-width": "3",
    "filter": "drop-shadow(0 0 8px rgba(243,156,18,0.5))"
  }
}
```

## Troubleshooting

### Styles Not Applying

1. Check selector syntax
2. Verify class names match exactly
3. Check specificity (use more specific selectors)
4. Ensure property names are correct
5. Check for typos in property values

### Unexpected Style Results

1. Check cascade order
2. Look for conflicting rules
3. Verify inheritance behavior
4. Check inline styles (highest priority)
5. Use browser dev tools to inspect computed styles

## Next Steps

- Review [User Guide](USER_GUIDE.md)
- Explore [Example Stylesheets](../../examples/ddf/styles/)
- Learn about [Expression Language](EXPRESSIONS.md)
- Study [Component System](COMPONENTS.md)
