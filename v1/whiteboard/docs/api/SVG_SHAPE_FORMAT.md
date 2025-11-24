# SVG Shape Description Format (.svgshape)

## Overview

The SVG Shape Description Format is a JSON-based format for describing SVG shapes using a DDF-like structure. This makes it easier to create and edit SVG shapes programmatically without writing raw XML.

## File Extension

`.svgshape` or `.svgshape.json`

## Format Specification

### Single Shape

```json
{
  "version": "1.0",
  "type": "rect",
  "geometry": {
    "x": 10,
    "y": 10,
    "width": 100,
    "height": 50,
    "rx": 5
  },
  "style": {
    "fill": "#3498db",
    "stroke": "#2c3e50",
    "stroke-width": "2"
  }
}
```

### Multiple Shapes

```json
{
  "version": "1.0",
  "shapes": [
    {
      "type": "rect",
      "geometry": {
        "x": 10,
        "y": 10,
        "width": 100,
        "height": 50
      },
      "style": {
        "fill": "#3498db"
      }
    },
    {
      "type": "circle",
      "geometry": {
        "cx": 200,
        "cy": 35,
        "r": 25
      },
      "style": {
        "fill": "#e74c3c"
      }
    }
  ],
  "metadata": {
    "title": "My Shapes",
    "author": "User",
    "description": "A collection of shapes"
  }
}
```

### Grouped Shapes

```json
{
  "version": "1.0",
  "type": "group",
  "style": {
    "opacity": "0.8"
  },
  "children": [
    {
      "type": "rect",
      "geometry": {
        "x": 10,
        "y": 10,
        "width": 50,
        "height": 50
      },
      "style": {
        "fill": "#3498db"
      }
    },
    {
      "type": "circle",
      "geometry": {
        "cx": 85,
        "cy": 35,
        "r": 25
      },
      "style": {
        "fill": "#e74c3c"
      }
    }
  ]
}
```

## Shape Types

### Rectangle

```json
{
  "type": "rect",
  "geometry": {
    "x": 10,
    "y": 10,
    "width": 100,
    "height": 50,
    "rx": 5,
    "ry": 5
  },
  "style": {
    "fill": "#3498db",
    "stroke": "#2c3e50",
    "stroke-width": "2"
  }
}
```

**Geometry Properties:**
- `x` - X coordinate of top-left corner
- `y` - Y coordinate of top-left corner
- `width` - Width of rectangle
- `height` - Height of rectangle
- `rx` - Horizontal corner radius (optional)
- `ry` - Vertical corner radius (optional)

### Circle

```json
{
  "type": "circle",
  "geometry": {
    "cx": 50,
    "cy": 50,
    "r": 30
  },
  "style": {
    "fill": "#e74c3c",
    "stroke": "#c0392b",
    "stroke-width": "2"
  }
}
```

**Geometry Properties:**
- `cx` - Center X coordinate
- `cy` - Center Y coordinate
- `r` - Radius

### Ellipse

```json
{
  "type": "ellipse",
  "geometry": {
    "cx": 50,
    "cy": 50,
    "rx": 40,
    "ry": 20
  },
  "style": {
    "fill": "#2ecc71"
  }
}
```

**Geometry Properties:**
- `cx` - Center X coordinate
- `cy` - Center Y coordinate
- `rx` - Horizontal radius
- `ry` - Vertical radius

### Path

```json
{
  "type": "path",
  "path_data": "M 10 10 L 90 90 L 10 90 Z",
  "style": {
    "fill": "none",
    "stroke": "#9b59b6",
    "stroke-width": "3"
  }
}
```

**Properties:**
- `path_data` - SVG path commands (d attribute)

### Text

```json
{
  "type": "text",
  "geometry": {
    "x": 50,
    "y": 50
  },
  "text": "Hello SVG!",
  "style": {
    "font-size": "16px",
    "font-family": "Arial, sans-serif",
    "fill": "#34495e",
    "text-anchor": "middle"
  }
}
```

**Properties:**
- `text` - Text content
- `geometry.x` - X coordinate
- `geometry.y` - Y coordinate

### Group

```json
{
  "type": "group",
  "style": {
    "opacity": "0.8"
  },
  "children": [
    {
      "type": "rect",
      "geometry": {"x": 10, "y": 10, "width": 50, "height": 50},
      "style": {"fill": "#3498db"}
    }
  ]
}
```

**Properties:**
- `children` - Array of child shapes

## Style Properties

### Common Styles

```json
{
  "style": {
    "fill": "#3498db",
    "fill-opacity": "0.5",
    "stroke": "#2c3e50",
    "stroke-width": "2",
    "stroke-opacity": "0.8",
    "opacity": "0.9"
  }
}
```

### Text Styles

```json
{
  "style": {
    "font-family": "Arial, sans-serif",
    "font-size": "16px",
    "font-weight": "bold",
    "font-style": "italic",
    "text-anchor": "middle",
    "dominant-baseline": "middle"
  }
}
```

### Advanced Styles

```json
{
  "style": {
    "stroke-dasharray": "5,5",
    "stroke-linecap": "round",
    "stroke-linejoin": "round",
    "filter": "drop-shadow(2px 2px 4px rgba(0,0,0,0.3))"
  }
}
```

## Complete Example: Network Diagram

```json
{
  "version": "1.0",
  "metadata": {
    "title": "Simple Network Diagram",
    "author": "DDF System",
    "description": "A basic network topology"
  },
  "shapes": [
    {
      "type": "rect",
      "geometry": {
        "x": 150,
        "y": 20,
        "width": 100,
        "height": 60,
        "rx": 10
      },
      "style": {
        "fill": "#3498db",
        "stroke": "#2c3e50",
        "stroke-width": "2"
      }
    },
    {
      "type": "text",
      "geometry": {
        "x": 200,
        "y": 55
      },
      "text": "Router",
      "style": {
        "fill": "white",
        "font-size": "14px",
        "text-anchor": "middle",
        "dominant-baseline": "middle"
      }
    },
    {
      "type": "circle",
      "geometry": {
        "cx": 100,
        "cy": 150,
        "r": 30
      },
      "style": {
        "fill": "#2ecc71",
        "stroke": "#27ae60",
        "stroke-width": "2"
      }
    },
    {
      "type": "circle",
      "geometry": {
        "cx": 300,
        "cy": 150,
        "r": 30
      },
      "style": {
        "fill": "#2ecc71",
        "stroke": "#27ae60",
        "stroke-width": "2"
      }
    },
    {
      "type": "path",
      "path_data": "M 200 80 L 100 120",
      "style": {
        "stroke": "#95a5a6",
        "stroke-width": "2",
        "fill": "none"
      }
    },
    {
      "type": "path",
      "path_data": "M 200 80 L 300 120",
      "style": {
        "stroke": "#95a5a6",
        "stroke-width": "2",
        "fill": "none"
      }
    }
  ]
}
```

## Integration with DDF

SVG Shape files can be referenced in DDF documents:

```json
{
  "shapes": [
    {
      "type": "svg",
      "svg_shape_file": "shapes/network_router.svgshape",
      "geometry": {
        "x": 100,
        "y": 100,
        "width": 100,
        "height": 60
      }
    }
  ]
}
```

## Conversion

### From .svgshape to .svg

```cpp
#include <whiteboard/svg/svg_generator.h>
#include <nlohmann/json.hpp>

// Load JSON
std::ifstream file("shape.svgshape");
nlohmann::json j;
file >> j;

// Parse to SVGShapeDesc
SVGShapeDesc shape;
shape.type = j["type"];
// ... parse geometry and style

// Generate SVG
SVGGenerator generator;
std::string svg = generator.generate(shape);

// Save
std::ofstream out("shape.svg");
out << svg;
```

### From .svg to .svgshape

This would require an SVG parser to extract shapes and convert them to the JSON format. This is more complex and could be a future enhancement.

## Benefits

1. **Human-readable** - JSON is easier to read and edit than XML
2. **Programmatic** - Easy to generate and manipulate in code
3. **Version control friendly** - Clean diffs in git
4. **DDF compatible** - Same structure as DDF shapes
5. **Extensible** - Easy to add new properties

## Future Enhancements

- **Transformations** - rotate, scale, translate, skew
- **Gradients** - linear and radial gradients
- **Patterns** - fill patterns
- **Masks** - clipping and masking
- **Animations** - SMIL animations
- **Markers** - arrowheads and other markers
- **Filters** - blur, drop-shadow, etc.
