# SVG Generator - DDF-Style Shape Descriptions

## Overview

The SVG Generator allows you to create SVG graphics using a simple DDF-like JSON structure instead of writing raw SVG markup. This makes it much easier to programmatically generate SVG shapes.

## Basic Usage

### C++ API

```cpp
#include <whiteboard/svg/svg_generator.h>

using namespace whiteboard;

// Create a rectangle
SVGShapeDesc rect;
rect.type = "rect";
rect.geometry["x"] = 10;
rect.geometry["y"] = 10;
rect.geometry["width"] = 100;
rect.geometry["height"] = 50;
rect.style["fill"] = "#3498db";
rect.style["stroke"] = "#2c3e50";
rect.style["stroke-width"] = "2";

SVGGenerator generator;
std::string svg = generator.generate(rect);
```

### JSON-Like Description

```json
{
  "type": "rect",
  "geometry": {
    "x": 10,
    "y": 10,
    "width": 100,
    "height": 50
  },
  "style": {
    "fill": "#3498db",
    "stroke": "#2c3e50",
    "stroke-width": "2"
  }
}
```

## Shape Types

### Rectangle

```cpp
SVGShapeDesc rect;
rect.type = "rect";
rect.geometry["x"] = 10;
rect.geometry["y"] = 10;
rect.geometry["width"] = 100;
rect.geometry["height"] = 50;
rect.geometry["rx"] = 5;  // Rounded corners (optional)
rect.style["fill"] = "#3498db";
```

### Circle

```cpp
SVGShapeDesc circle;
circle.type = "circle";
circle.geometry["cx"] = 50;  // Center X
circle.geometry["cy"] = 50;  // Center Y
circle.geometry["r"] = 30;   // Radius
circle.style["fill"] = "#e74c3c";
```

### Ellipse

```cpp
SVGShapeDesc ellipse;
ellipse.type = "ellipse";
ellipse.geometry["cx"] = 50;
ellipse.geometry["cy"] = 50;
ellipse.geometry["rx"] = 40;  // Horizontal radius
ellipse.geometry["ry"] = 20;  // Vertical radius
ellipse.style["fill"] = "#2ecc71";
```

### Path

```cpp
SVGShapeDesc path;
path.type = "path";
path.path_data = "M 10 10 L 90 90 L 10 90 Z";  // SVG path commands
path.style["fill"] = "none";
path.style["stroke"] = "#9b59b6";
path.style["stroke-width"] = "3";
```

### Text

```cpp
SVGShapeDesc text;
text.type = "text";
text.geometry["x"] = 50;
text.geometry["y"] = 50;
text.text = "Hello SVG!";
text.style["font-size"] = "16px";
text.style["fill"] = "#34495e";
text.style["text-anchor"] = "middle";
```

### Group

```cpp
SVGShapeDesc group;
group.type = "group";
group.style["opacity"] = "0.8";

// Add children
SVGShapeDesc rect1;
rect1.type = "rect";
rect1.geometry["x"] = 10;
rect1.geometry["y"] = 10;
rect1.geometry["width"] = 50;
rect1.geometry["height"] = 50;
rect1.style["fill"] = "#3498db";

SVGShapeDesc rect2;
rect2.type = "rect";
rect2.geometry["x"] = 70;
rect2.geometry["y"] = 10;
rect2.geometry["width"] = 50;
rect2.geometry["height"] = 50;
rect2.style["fill"] = "#e74c3c";

group.children.push_back(rect1);
group.children.push_back(rect2);

SVGGenerator generator;
std::string svg = generator.generate(group);
```

## Multiple Shapes

```cpp
std::vector<SVGShapeDesc> shapes;

// Add multiple shapes
shapes.push_back(rect);
shapes.push_back(circle);
shapes.push_back(text);

SVGGenerator generator;
std::string svg = generator.generate_multi(shapes, 200, 200);
```

## Common Styles

### Fill and Stroke

```cpp
shape.style["fill"] = "#3498db";           // Fill color
shape.style["stroke"] = "#2c3e50";         // Stroke color
shape.style["stroke-width"] = "2";         // Stroke width
shape.style["fill-opacity"] = "0.5";       // Fill transparency
shape.style["stroke-opacity"] = "0.8";     // Stroke transparency
```

### Text Styling

```cpp
text.style["font-family"] = "Arial, sans-serif";
text.style["font-size"] = "16px";
text.style["font-weight"] = "bold";
text.style["text-anchor"] = "middle";      // left, middle, end
text.style["dominant-baseline"] = "middle"; // top, middle, bottom
```

### Effects

```cpp
shape.style["opacity"] = "0.7";            // Overall opacity
shape.style["filter"] = "drop-shadow(2px 2px 4px rgba(0,0,0,0.3))";
```

## Complete Example: Network Diagram

```cpp
#include <whiteboard/svg/svg_generator.h>

using namespace whiteboard;

std::string create_network_diagram() {
  SVGGenerator generator;
  std::vector<SVGShapeDesc> shapes;
  
  // Router (rectangle with rounded corners)
  SVGShapeDesc router;
  router.type = "rect";
  router.geometry["x"] = 150;
  router.geometry["y"] = 20;
  router.geometry["width"] = 100;
  router.geometry["height"] = 60;
  router.geometry["rx"] = 10;
  router.style["fill"] = "#3498db";
  router.style["stroke"] = "#2c3e50";
  router.style["stroke-width"] = "2";
  shapes.push_back(router);
  
  // Router label
  SVGShapeDesc router_label;
  router_label.type = "text";
  router_label.geometry["x"] = 200;
  router_label.geometry["y"] = 55;
  router_label.text = "Router";
  router_label.style["fill"] = "white";
  router_label.style["font-size"] = "14px";
  router_label.style["text-anchor"] = "middle";
  shapes.push_back(router_label);
  
  // Server (circle)
  SVGShapeDesc server;
  server.type = "circle";
  server.geometry["cx"] = 100;
  server.geometry["cy"] = 150;
  server.geometry["r"] = 30;
  server.style["fill"] = "#2ecc71";
  server.style["stroke"] = "#27ae60";
  server.style["stroke-width"] = "2";
  shapes.push_back(server);
  
  // Connection line (path)
  SVGShapeDesc connection;
  connection.type = "path";
  connection.path_data = "M 200 80 L 100 150";
  connection.style["stroke"] = "#95a5a6";
  connection.style["stroke-width"] = "2";
  connection.style["fill"] = "none";
  shapes.push_back(connection);
  
  return generator.generate_multi(shapes, 400, 200);
}
```

## Integration with DDF

You can use the SVG Generator to create custom shapes for DDF documents:

```cpp
// Generate SVG from DDF-like description
SVGShapeDesc shape;
shape.type = "rect";
shape.geometry["x"] = 0;
shape.geometry["y"] = 0;
shape.geometry["width"] = 100;
shape.geometry["height"] = 50;
shape.style["fill"] = "#3498db";

SVGGenerator generator;
std::string svg_data = generator.generate(shape);

// Use in DDF
ddf::Shape ddf_shape;
ddf_shape.type = "svg";
ddf_shape.svg_data = svg_data;
ddf_shape.geometry["x"] = 100;
ddf_shape.geometry["y"] = 100;
ddf_shape.geometry["width"] = 100;
ddf_shape.geometry["height"] = 50;
```

## Benefits

1. **Easier to write** - No XML/SVG syntax to remember
2. **Programmatic** - Easy to generate shapes in code
3. **Type-safe** - C++ structs instead of string manipulation
4. **DDF-compatible** - Same structure as DDF shapes
5. **Composable** - Build complex shapes from simple ones

## Future Enhancements

- **Gradients and patterns**
- **Animations**
- **Filters and effects**
- **Markers (arrowheads)**
- **Clipping paths**
- **Transformations** (rotate, scale, translate)
