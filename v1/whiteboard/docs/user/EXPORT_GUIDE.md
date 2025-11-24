# Export Guide - Drawing and Exporting to SVG

## Quick Start

### 1. Draw Your Diagram

**Option A: Use Shape Tools**
1. Click shape tool in toolbar
2. Select a shape from the library
3. Click on canvas to place it
4. Repeat to add more shapes

**Option B: Import DDF**
1. File → Import DDF Document
2. Select a `.json` file (e.g., `comprehensive_diagram.json`)
3. Edit as needed

**Option C: Draw Freehand**
1. Select Pen tool
2. Draw on canvas
3. Add shapes, text, etc.

### 2. Export to SVG

**Method 1: Export Menu**
1. File → Export
2. Select "SVG" format
3. Choose filename
4. Click Save

**Method 2: DDF Export (if using DDF)**
1. File → Export DDF to SVG
2. Choose filename
3. Click Save

## What Gets Exported

### Regular Strokes
- ✅ Rectangles
- ✅ Circles
- ✅ Lines
- ✅ Arrows
- ✅ Text
- ✅ Freehand drawings
- ✅ SVG shapes from library

### DDF Documents
- ✅ All shapes
- ✅ Connectors
- ✅ Text labels
- ✅ Styles
- ✅ SVG shapes

## Export Formats

### SVG (Vector)
- **Best for:** Scalable graphics, web, print
- **Pros:** Infinite scaling, small file size, editable
- **Cons:** Limited browser support for complex features

### PNG (Raster)
- **Best for:** Screenshots, presentations, social media
- **Pros:** Universal support, exact rendering
- **Cons:** Fixed resolution, larger file size

### PDF (Document)
- **Best for:** Documents, printing, archiving
- **Pros:** Professional, portable, printable
- **Cons:** Larger file size

## Examples

### Example 1: Simple Flowchart

1. **Draw:**
   - Add Process box (from Flowchart category)
   - Add Decision diamond
   - Add connectors between them
   - Add text labels

2. **Export:**
   - File → Export → SVG
   - Save as `flowchart.svg`

3. **Result:**
   ```xml
   <svg xmlns="http://www.w3.org/2000/svg" width="800" height="600">
     <rect x="100" y="100" width="120" height="60" fill="#3498db"/>
     <text x="160" y="130">Process</text>
     <path d="M 160 160 L 160 200" stroke="#000"/>
     <path d="M 160 200 L 220 240 L 160 280 L 100 240 Z" fill="#f39c12"/>
   </svg>
   ```

### Example 2: Network Diagram

1. **Import DDF:**
   - File → Import DDF
   - Select `network_svg.json`

2. **Edit:**
   - Click shapes to convert to editable
   - Move, resize as needed
   - Add labels

3. **Export:**
   - File → Export → SVG
   - Save as `network.svg`

### Example 3: Custom Shape to SVG

1. **Create .svgshape:**
   ```json
   {
     "type": "rect",
     "geometry": {"x": 0, "y": 0, "width": 100, "height": 50},
     "style": {"fill": "#3498db"}
   }
   ```

2. **Generate SVG:**
   ```cpp
   SVGGenerator gen;
   std::string svg = gen.generate(shape);
   // Save to file
   ```

3. **Result:**
   ```xml
   <svg xmlns="http://www.w3.org/2000/svg" width="100" height="50">
     <rect x="0" y="0" width="100" height="50" fill="#3498db"/>
   </svg>
   ```

## Programmatic Export

### From DDF Document

```cpp
#include <whiteboard/ddf/ddf_document.h>

// Load DDF
ddf::DDFDocument doc;
doc.load_from_file("diagram.json");

// Export to SVG
std::string svg = doc.export_to_svg();

// Save to file
doc.export_to_svg_file("output.svg");
```

### From .svgshape

```cpp
#include <whiteboard/svg/svg_generator.h>

// Create shape
SVGShapeDesc rect;
rect.type = "rect";
rect.geometry["x"] = 10;
rect.geometry["y"] = 10;
rect.geometry["width"] = 100;
rect.geometry["height"] = 50;
rect.style["fill"] = "#3498db";

// Generate SVG
SVGGenerator gen;
std::string svg = gen.generate(rect);

// Save to file
std::ofstream file("shape.svg");
file << svg;
```

### From Multiple Shapes

```cpp
std::vector<SVGShapeDesc> shapes;

// Add shapes
shapes.push_back(rect);
shapes.push_back(circle);
shapes.push_back(text);

// Generate combined SVG
SVGGenerator gen;
std::string svg = gen.generate_multi(shapes, 400, 300);

// Save
std::ofstream file("diagram.svg");
file << svg;
```

## Tips

### For Best Results

1. **Use vector shapes** - SVG shapes scale better than raster images
2. **Organize layers** - Group related shapes
3. **Use styles** - Apply consistent colors and strokes
4. **Add labels** - Text exports as editable SVG text
5. **Check bounds** - Ensure all shapes are within canvas

### Common Issues

**Problem:** Shapes cut off in export
- **Solution:** Zoom to fit before exporting

**Problem:** Text looks different
- **Solution:** Use web-safe fonts (Arial, sans-serif)

**Problem:** Colors don't match
- **Solution:** Use hex colors (#RRGGBB) for consistency

**Problem:** File too large
- **Solution:** Simplify paths, reduce number of shapes

## Editing Exported SVG

The exported SVG can be edited in:
- **Inkscape** (free, open-source)
- **Adobe Illustrator** (professional)
- **Figma** (web-based)
- **Any text editor** (it's just XML!)

### Example: Edit in Text Editor

```xml
<!-- Original -->
<rect x="10" y="10" width="100" height="50" fill="#3498db"/>

<!-- Change color -->
<rect x="10" y="10" width="100" height="50" fill="#e74c3c"/>

<!-- Add stroke -->
<rect x="10" y="10" width="100" height="50" 
      fill="#3498db" stroke="#2c3e50" stroke-width="2"/>
```

## Workflow Examples

### Workflow 1: Design → Export → Web

1. Design diagram in whiteboard
2. Export to SVG
3. Optimize with SVGO
4. Use in HTML: `<img src="diagram.svg">`

### Workflow 2: Template → Customize → Export

1. Create `.svgshape` template
2. Add to shape library
3. Place and customize in diagram
4. Export to SVG

### Workflow 3: Data → DDF → SVG

1. Generate DDF from data
2. Import in whiteboard
3. Auto-layout shapes
4. Export to SVG

## Advanced: Batch Export

```cpp
// Export multiple diagrams
std::vector<std::string> ddf_files = {
  "diagram1.json",
  "diagram2.json",
  "diagram3.json"
};

for (const auto& file : ddf_files) {
  ddf::DDFDocument doc;
  doc.load_from_file(file);
  
  std::string output = file;
  output.replace(output.find(".json"), 5, ".svg");
  
  doc.export_to_svg_file(output);
}
```

## Summary

You can now:
- ✅ Draw diagrams in the whiteboard
- ✅ Use 111+ pre-built shapes
- ✅ Create custom shapes with `.svgshape`
- ✅ Import DDF documents
- ✅ Export to SVG (vector)
- ✅ Export to PNG (raster)
- ✅ Export to PDF (document)
- ✅ Edit exported SVG in other tools
- ✅ Automate with code

The complete workflow from design to export is seamless! 🎨
