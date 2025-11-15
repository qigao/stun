# .svgshape Format Implementation

## Overview

The SVGShapeLibrary now supports loading shapes in DDF-like JSON format (`.svgshape` files) in addition to traditional SVG files.

## Implementation

### File Detection

The library automatically detects file format by extension:
- `.svg` → Load as traditional SVG
- `.svgshape` or `.svgshape.json` → Parse JSON and generate SVG

### Loading Process

```
.svgshape file
    ↓
Load JSON
    ↓
Parse to SVGShapeDesc
    ↓
SVGGenerator
    ↓
Generated SVG
    ↓
Apply parameters ({{placeholders}})
    ↓
Final SVG
```

### Code Changes

**whiteboard/src/svg/svg_shape_library.cpp:**
- `load_svg_file()` - Detects `.svgshape` files
- `load_svgshape_file()` - Loads and generates SVG from JSON
- `parse_svgshape_json()` - Parses JSON to SVGShapeDesc
- `parse_svgshape_array()` - Parses array of shapes

**whiteboard/include/whiteboard/svg/svg_shape_library.h:**
- Added method declarations
- Added forward declaration for SVGShapeDesc

## Usage

### 1. Create a .svgshape File

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

### 2. Reference in library.json

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

### 3. Use Like Any Other Shape

```cpp
SVGShapeLibrary library;
library.load_library("shapes/library.json");

// Works exactly the same as .svg files
Stroke stroke = library.create_shape("basic.rectangle", Point(100, 100));
```

## Parametric Shapes

Parameters work the same way with `.svgshape` files:

```json
{
  "version": "1.0",
  "shapes": [
    {
      "type": "rect",
      "geometry": {"x": 0, "y": 0, "width": 120, "height": 60},
      "style": {"fill": "#3498db", "stroke": "#2c3e50", "stroke-width": "2"}
    },
    {
      "type": "text",
      "geometry": {"x": 60, "y": 30},
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

The `{{label}}` placeholder will be replaced just like in SVG files.

## Benefits

1. **Easier to Edit** - JSON is more readable than SVG XML
2. **Version Control** - Cleaner diffs in git
3. **Programmatic** - Can generate shapes in code
4. **Backward Compatible** - Existing `.svg` files still work
5. **Same API** - No code changes needed to use new format

## Migration Path

### Phase 1: Test with New Shapes
- Create new shapes in `.svgshape` format
- Test thoroughly
- Keep existing `.svg` files

### Phase 2: Convert Gradually
- Convert shapes one category at a time
- Test each conversion
- Keep `.svg` as backup

### Phase 3: Complete Migration
- All shapes in `.svgshape` format
- Remove old `.svg` files
- Update documentation

## Example Conversions

### Simple Rectangle

**Before (rectangle.svg):**
```xml
<svg xmlns="http://www.w3.org/2000/svg" width="100" height="50">
  <rect x="0" y="0" width="100" height="50" 
        fill="#3498db" stroke="#2c3e50" stroke-width="2"/>
</svg>
```

**After (rectangle.svgshape):**
```json
{
  "version": "1.0",
  "type": "rect",
  "geometry": {"x": 0, "y": 0, "width": 100, "height": 50},
  "style": {"fill": "#3498db", "stroke": "#2c3e50", "stroke-width": "2"}
}
```

### Flowchart Process with Label

**Before (process.svg):**
```xml
<svg xmlns="http://www.w3.org/2000/svg" width="120" height="60">
  <rect x="0" y="0" width="120" height="60" fill="#3498db" stroke="#2c3e50" stroke-width="2"/>
  <text x="60" y="30" fill="white" font-size="14" text-anchor="middle" dominant-baseline="middle">{{label}}</text>
</svg>
```

**After (process.svgshape):**
```json
{
  "version": "1.0",
  "shapes": [
    {
      "type": "rect",
      "geometry": {"x": 0, "y": 0, "width": 120, "height": 60},
      "style": {"fill": "#3498db", "stroke": "#2c3e50", "stroke-width": "2"}
    },
    {
      "type": "text",
      "geometry": {"x": 60, "y": 30},
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

## Testing

Test each converted shape:

```cpp
// Load library
SVGShapeLibrary library;
library.load_library("shapes/library.json");

// Create shape
Stroke stroke = library.create_shape("basic.rectangle", Point(100, 100));

// Verify SVG data is generated
assert(!stroke.svg_data.empty());

// Test with parameters
std::map<std::string, std::string> params;
params["label"] = "Test";
std::string svg = library.generate_svg("flowchart.process", params);
assert(svg.find("Test") != std::string::npos);
```

## Logging

The implementation includes detailed logging:

```
SVGShapeLibrary: Loading DDF-format shape from 'shapes/basic/rectangle.svgshape'
SVGShapeLibrary: Loaded .svgshape JSON from 'shapes/basic/rectangle.svgshape'
SVGGenerator: Generating SVG for shape type 'rect'
SVGGenerator: Generated SVG document (234 bytes)
SVGShapeLibrary: Generated SVG from single shape
```

## Future Enhancements

1. **Validation** - JSON schema validation for `.svgshape` files
2. **Editor** - Visual editor for `.svgshape` files
3. **Converter** - Tool to convert `.svg` to `.svgshape`
4. **Templates** - Shape templates with more complex parametrization
5. **Composition** - Compose shapes from other shapes

## Conclusion

The `.svgshape` format makes shape creation and maintenance much easier while maintaining full backward compatibility with existing `.svg` files. The implementation is transparent to users - shapes work exactly the same regardless of format.
