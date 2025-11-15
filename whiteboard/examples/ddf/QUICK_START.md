# Quick Start Guide - Using DDF Examples

## For modern_whiteboard.exe Users

### ✅ Import Button Now Available!

The "Import DDF" button has been added to the application!

### Quick Steps to Load Examples

1. **Build the application** (if not already built):
   ```cmd
   cd build
   cmake --build . --config Release --target modern_whiteboard
   ```

2. **Launch the application**:
   ```cmd
   cd build\Release
   modern_whiteboard.exe
   ```

3. **Import a DDF file**:
   - Click **File** menu → **Import DDF**
   - Navigate to `whiteboard\examples\ddf\`
   - Select `flowchart.json` (or any other example)
   - Click **Open**

4. **View the diagram** - it will automatically zoom to fit!

### Alternative Options

If you want to test programmatically:

#### Option 2: Test via Code

If you're comfortable with C++, you can test the examples by modifying the application code:

```cpp
// In ModernWhiteboardApp or CanvasView initialization
#include "whiteboard/ddf/ddf_document.h"

auto ddf_doc = std::make_shared<whiteboard::ddf::DDFDocument>();
if (ddf_doc->load_from_file("../../whiteboard/examples/ddf/flowchart.json")) {
    // Document loaded - it will render automatically
    std::cout << "DDF document loaded successfully!" << std::endl;
}
```

Then rebuild and run:
```cmd
cd build
cmake --build . --config Release
cd Release
modern_whiteboard.exe
```

#### Option 3: Use the Test Suite

The examples are tested in the unit tests. You can run them to verify they work:

```cmd
cd build\Release
ctest -R ddf -V
```

Or view the test code to see how they're loaded:
- `whiteboard/test/test_ddf_document.cpp`
- `whiteboard/test/test_svg_import.cpp`

### What the Examples Demonstrate

Even though you can't load them via UI yet, the examples show:

1. **org_chart.json** - Hierarchical data visualization with components
2. **network_diagram.json** - Network topology with force-directed layout  
3. **flowchart.json** - Process flow with orthogonal connectors

You can open these JSON files in a text editor to see the DDF format structure.

## Testing the Examples

### Test 1: Load and Render

```cpp
// In your test or application code
#include "whiteboard/ddf/ddf_document.h"

void test_load_example() {
    whiteboard::ddf::DDFDocument doc;
    
    // Load flowchart example
    if (doc.load_from_file("examples/ddf/flowchart.json")) {
        std::cout << "✓ Flowchart loaded successfully" << std::endl;
        
        // Validate
        if (doc.validate()) {
            std::cout << "✓ Document is valid" << std::endl;
        }
        
        // Render (requires NanoVG context)
        // doc.render(nvg_ctx, viewport);
    }
}
```

### Test 2: Export to SVG

```cpp
void test_export_svg() {
    whiteboard::ddf::DDFDocument doc;
    
    if (doc.load_from_file("examples/ddf/org_chart.json")) {
        // Export to SVG
        if (doc.export_to_svg_file("output/org_chart.svg")) {
            std::cout << "✓ Exported to SVG successfully" << std::endl;
        }
    }
}
```

### Test 3: Modify and Save

```cpp
void test_modify_example() {
    whiteboard::ddf::DDFDocument doc;
    
    if (doc.load_from_file("examples/ddf/network_diagram.json")) {
        // Add a new data node
        whiteboard::ddf::DataNode new_device;
        new_device.id = "server3";
        new_device.type = "server";
        new_device.properties = {
            {"name", "Web Server 3"},
            {"ip", "192.168.1.12"},
            {"status", "online"}
        };
        
        doc.data_layer().add_node(new_device);
        
        // Save modified document
        doc.save_to_file("output/modified_network.json");
        std::cout << "✓ Modified and saved" << std::endl;
    }
}
```

## Integration with Whiteboard

The DDF system is integrated with the whiteboard through `WhiteboardDDFIntegration`:

```cpp
#include "whiteboard/ddf/whiteboard_ddf_integration.h"

// In CanvasView or similar
void setup_ddf() {
    // Create DDF document
    auto ddf_doc = std::make_shared<whiteboard::ddf::DDFDocument>();
    
    // Load example
    ddf_doc->load_from_file("examples/ddf/flowchart.json");
    
    // Set in canvas view
    canvas_view->set_ddf_document(ddf_doc);
    
    // The canvas will now render DDF elements
}
```

## Current Status

As of the current implementation:

✅ **Completed:**
- Data Layer
- Shape Layer (with hierarchy)
- Style Layer (CSS-like)
- Component Layer
- Connector Layer (with smart routing)
- Event Layer
- Expression Parser
- Layout Algorithms
- SVG Export/Import
- Whiteboard Integration

⚠️ **Partially Complete:**
- Performance Optimizations (viewport culling, caching)
- Some optional unit tests

## Viewing Examples Without Application

You can also view the example JSON files directly:

1. Open in text editor: `notepad examples\ddf\flowchart.json`
2. Use online JSON viewer
3. Validate with JSON schema validator

## Creating Your Own Diagrams

1. **Copy an example:**
   ```cmd
   copy examples\ddf\flowchart.json my_diagram.json
   ```

2. **Edit the JSON:**
   - Update `metadata` section
   - Modify `data.nodes` with your data
   - Adjust `styles` for your colors
   - Customize `components` if needed

3. **Load in application:**
   ```cpp
   doc.load_from_file("my_diagram.json");
   ```

## Troubleshooting

### Example Won't Load

**Check:**
- File path is correct
- JSON syntax is valid
- All required fields are present

**Debug:**
```cpp
if (!doc.load_from_file("example.json")) {
    auto errors = doc.get_validation_errors();
    for (const auto& err : errors) {
        std::cerr << err << std::endl;
    }
}
```

### Shapes Not Visible

**Check:**
- Viewport includes shape coordinates
- Fill/stroke colors are set
- Opacity is not 0

**Debug:**
```cpp
auto shapes = doc.shape_layer().get_all_shapes();
for (auto* shape : shapes) {
    std::cout << "Shape: " << shape->id 
              << " at (" << shape->geometry["x"] 
              << ", " << shape->geometry["y"] << ")" << std::endl;
}
```

### Connectors Not Routing

**Check:**
- Connection points exist on shapes
- Shape IDs are correct
- Routing algorithm is appropriate

**Debug:**
```cpp
auto connectors = doc.connector_layer().get_all_connectors();
for (auto* conn : connectors) {
    doc.connector_layer().compute_path(conn->id);
    std::cout << "Connector: " << conn->id 
              << " has " << conn->path_points.size() 
              << " points" << std::endl;
}
```

## Next Steps

1. **Read the documentation:**
   - [User Guide](../../docs/ddf/USER_GUIDE.md)
   - [API Reference](../../docs/ddf/API_REFERENCE.md)

2. **Explore examples:**
   - Study the JSON structure
   - Modify and experiment
   - Create your own diagrams

3. **Integrate with your app:**
   - Use the API to load/save DDF
   - Render in your canvas
   - Handle user interactions

## Support

For questions or issues:
- Check the documentation in `docs/ddf/`
- Review example files
- Examine test files in `test/`
- Check validation errors

## Example File Locations

```
whiteboard/
├── examples/ddf/
│   ├── org_chart.json          ← Organization chart
│   ├── network_diagram.json    ← Network topology
│   ├── flowchart.json          ← Process flowchart
│   ├── styles/
│   │   ├── theme_light.css     ← Light theme
│   │   ├── theme_dark.css      ← Dark theme
│   │   └── components.css      ← Component styles
│   ├── README.md               ← Detailed usage guide
│   └── QUICK_START.md          ← This file
└── docs/ddf/
    ├── USER_GUIDE.md           ← Complete user guide
    ├── API_REFERENCE.md        ← API documentation
    ├── CSS_STYLING.md          ← Styling guide
    ├── EXPRESSIONS.md          ← Expression language
    └── COMPONENTS.md           ← Component system
```
