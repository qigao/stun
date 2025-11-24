# Shape Library Migration to DDF Format

## Overview

Migrate the shape library from raw SVG files to DDF-like JSON format (.svgshape) for easier editing and maintenance.

## Benefits

1. **Easier to edit** - JSON is more human-readable than SVG XML
2. **Version control friendly** - Cleaner diffs
3. **Consistent format** - Same as DDF shapes
4. **Programmatic generation** - Can generate shapes in code
5. **Better parametrization** - Easier to add parameters

## Migration Strategy

### Phase 1: Basic Shapes (Priority)
Convert simple geometric shapes first:
- Rectangle
- Circle
- Ellipse
- Triangle
- Diamond
- Hexagon
- Pentagon
- Trapezoid
- Parallelogram
- Star

### Phase 2: Flowchart Shapes
- Process
- Decision
- Data
- Document
- Manual Input
- Predefined Process
- Internal Storage

### Phase 3: Network Shapes
- Router
- Switch
- Server
- Firewall
- Cloud
- Database

### Phase 4: UML Shapes
- Class
- Interface
- Component
- Package
- Actor
- Use Case

## File Structure

```
shapes/
├── basic/
│   ├── rectangle.svgshape
│   ├── circle.svgshape
│   ├── triangle.svgshape
│   └── ...
├── flowchart/
│   ├── process.svgshape
│   ├── decision.svgshape
│   └── ...
├── network/
│   ├── router.svgshape
│   ├── server.svgshape
│   └── ...
└── uml/
    ├── class.svgshape
    ├── interface.svgshape
    └── ...
```

## Example Conversions

### Rectangle (Before - SVG)
```xml
<svg xmlns="http://www.w3.org/2000/svg" width="100" height="50">
  <rect x="0" y="0" width="100" height="50" 
        fill="#3498db" stroke="#2c3e50" stroke-width="2"/>
</svg>
```

### Rectangle (After - DDF Format)
```json
{
  "version": "1.0",
  "type": "rect",
  "geometry": {
    "x": 0,
    "y": 0,
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

### Diamond (Before - SVG)
```xml
<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
  <path d="M 50 0 L 100 50 L 50 100 L 0 50 Z" 
        fill="#e74c3c" stroke="#c0392b" stroke-width="2"/>
</svg>
```

### Diamond (After - DDF Format)
```json
{
  "version": "1.0",
  "type": "path",
  "path_data": "M 50 0 L 100 50 L 50 100 L 0 50 Z",
  "style": {
    "fill": "#e74c3c",
    "stroke": "#c0392b",
    "stroke-width": "2"
  }
}
```

## Implementation Changes

### 1. Update SVGShapeLibrary

Add support for loading .svgshape files:

```cpp
class SVGShapeLibrary {
  // Existing methods...
  
  // New: Load shape from .svgshape file
  std::string load_svgshape_file(const std::string& filepath);
  
  // Modified: generate_svg checks file extension
  std::string generate_svg(const std::string& shape_id, 
                          const std::map<std::string, std::string>& parameters);
};
```

### 2. Backward Compatibility

Support both formats during migration:
- `.svg` files - Load as-is (existing behavior)
- `.svgshape` files - Generate SVG using SVGGenerator

### 3. Library Definition

Update `library.json` to reference new format:

```json
{
  "shapes": [
    {
      "id": "basic.rectangle",
      "name": "Rectangle",
      "category": "Basic",
      "svg_file": "basic/rectangle.svgshape",
      "parameters": []
    }
  ]
}
```

## Migration Process

1. **Create .svgshape files** for each shape
2. **Update library.json** to reference new files
3. **Test each shape** to ensure it renders correctly
4. **Keep .svg files** as backup during migration
5. **Remove .svg files** once migration is complete

## Parametric Shapes

DDF format makes parametrization easier:

```json
{
  "version": "1.0",
  "shapes": [
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
        "stroke": "#2c3e50",
        "stroke-width": "2"
      }
    },
    {
      "type": "text",
      "geometry": {
        "x": 50,
        "y": 25
      },
      "text": "{{label}}",
      "style": {
        "fill": "white",
        "font-size": "14px",
        "text-anchor": "middle",
        "dominant-baseline": "middle"
      }
    }
  ]
}
```

Parameters like `{{label}}` can be replaced just like in current SVG files.

## Timeline

- **Week 1**: Implement .svgshape loading in SVGShapeLibrary
- **Week 2**: Convert basic shapes (10 shapes)
- **Week 3**: Convert flowchart shapes (15 shapes)
- **Week 4**: Convert network shapes (20 shapes)
- **Week 5**: Convert UML shapes (30 shapes)
- **Week 6**: Testing and cleanup

## Testing

For each converted shape:
1. Load in shape library
2. Place on canvas
3. Verify rendering matches original
4. Test parametrization (if applicable)
5. Test in DDF documents
