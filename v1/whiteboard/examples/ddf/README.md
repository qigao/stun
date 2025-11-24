# DDF Examples - Usage Guide

## Overview

This directory contains example DDF (Diagram Definition Format) documents that demonstrate various diagram types and features. These examples can be loaded and rendered in the modern_whiteboard.exe application.

## Available Examples

### 1. Organization Chart (`org_chart.json`)
A hierarchical organization chart demonstrating:
- Data-driven diagram generation
- Component system (person cards)
- Tree layout algorithm
- CSS-like styling with hover effects
- Interactive events

### 2. Network Diagram (`network_diagram.json`)
A network topology diagram showing:
- Network devices (routers, servers, firewalls)
- Force-directed layout
- Status-based styling
- Smart connectors
- Device components

### 3. Flowchart (`flowchart.json`)
A user authentication flowchart featuring:
- Flowchart components (start/end, process, decision)
- Orthogonal connector routing
- Decision branching
- Component-based design

## How to Use with modern_whiteboard.exe

### ✅ Import Button Available!

The "Import DDF" button has been added to the application! You can now load DDF files directly from the UI.

### Quick Start

1. **Build and launch** the application:
   ```cmd
   cd build
   cmake --build . --config Release --target modern_whiteboard
   cd Release
   modern_whiteboard.exe
   ```

2. **Import a DDF file**:
   - Click the **File** menu (or menu icon)
   - Select **"Import DDF"**
   - Browse to `whiteboard\examples\ddf\`
   - Select an example file (org_chart.json, network_diagram.json, or flowchart.json)
   - Click **Open**

3. The diagram will load and automatically zoom to fit!

### Alternative Methods

### Method 1: Programmatic Loading (Current Method)

The DDF system is integrated into the canvas view. To load examples, you need to modify the code:

```cpp
#include "whiteboard/ddf/ddf_document.h"

// In your application initialization or canvas setup
whiteboard::ddf::DDFDocument doc;
if (doc.load_from_file("../../whiteboard/examples/ddf/flowchart.json")) {
    // Document loaded successfully
    // The canvas view will render DDF elements automatically
}
```

### Method 2: Add Import Button (Recommended for Users)

To add a file import button to the application, you would need to:

1. **Add a menu item** in `ModernWhiteboardApp`:
   ```cpp
   // Add to menu bar
   auto *file_menu = new nanogui::PopupButton(menu_bar, "File");
   auto *popup = file_menu->popup();
   
   popup->add_button("Import DDF...", [this]() {
       // Show file dialog
       auto result = nanogui::file_dialog({
           {"json", "DDF Document"}
       }, false);
       
       if (!result.empty()) {
           load_ddf_file(result[0]);
       }
   });
   ```

2. **Implement the load function**:
   ```cpp
   void ModernWhiteboardApp::load_ddf_file(const std::string& filepath) {
       auto ddf_doc = std::make_shared<whiteboard::ddf::DDFDocument>();
       if (ddf_doc->load_from_file(filepath)) {
           // Set in canvas view
           canvas_view->set_ddf_document(ddf_doc);
           canvas_view->redraw();
       }
   }
   ```

### Method 3: Test via Unit Tests

The examples can be tested using the test suite:

```cmd
cd build\Release
ctest -R ddf
```

Or run specific tests:
```cmd
test_ddf_document.exe
test_svg_import.exe
```

## Interacting with DDF Diagrams

### Mouse Interactions

- **Hover**: Hover over shapes to see hover effects and tooltips
- **Click**: Click shapes to select them
- **Drag**: Drag shapes to move them (if enabled)
- **Right-click**: Context menu for shape operations

### Keyboard Shortcuts

- **Delete**: Delete selected shapes
- **Ctrl+C**: Copy selected shapes
- **Ctrl+V**: Paste shapes
- **Ctrl+Z**: Undo
- **Ctrl+Y**: Redo

## Modifying Examples

### Editing JSON Files

You can edit the example JSON files directly to customize them:

1. Open the JSON file in a text editor
2. Modify properties (colors, text, positions, etc.)
3. Save the file
4. Reload in modern_whiteboard.exe

### Example Modifications

#### Change Colors

```json
{
  "styles": {
    "rules": [
      {
        "selector": ".card",
        "properties": {
          "fill": "#e74c3c"  // Change from blue to red
        }
      }
    ]
  }
}
```

#### Add New Data Nodes

```json
{
  "data": {
    "nodes": [
      {
        "id": "new_person",
        "type": "person",
        "properties": {
          "name": "Jane Smith",
          "title": "VP Engineering"
        }
      }
    ]
  }
}
```

#### Modify Layout Parameters

```json
{
  "layouts": [
    {
      "algorithm": "tree",
      "parameters": {
        "horizontal_spacing": 100,  // Increase spacing
        "vertical_spacing": 150
      }
    }
  ]
}
```

## Creating Your Own DDF Documents

### Step 1: Start with a Template

Copy one of the example files as a starting point:

```cmd
copy org_chart.json my_diagram.json
```

### Step 2: Modify the Metadata

```json
{
  "version": "1.0",
  "metadata": {
    "title": "My Custom Diagram",
    "author": "Your Name",
    "created": "2025-10-19T00:00:00Z"
  }
}
```

### Step 3: Update Data

Replace the data nodes and relationships with your own data.

### Step 4: Customize Styles

Modify the CSS rules to match your design preferences.

### Step 5: Test in Application

Load your custom DDF file in modern_whiteboard.exe to see the results.

## Applying Custom Stylesheets

You can apply the example stylesheets to your diagrams:

### In JSON

```json
{
  "styles": {
    "external_stylesheets": [
      "styles/theme_light.css",
      "styles/components.css"
    ]
  }
}
```

### Programmatically

```cpp
#include "whiteboard/ddf/stylesheet_loader.h"

whiteboard::ddf::StyleSheetLoader loader;
auto stylesheet = loader.load_from_file("examples/ddf/styles/theme_light.css");
doc.style_layer().add_stylesheet(stylesheet);
```

## Exporting Diagrams

### Export to SVG

Once you've loaded a DDF document, you can export it to SVG:

1. **Via File Menu**: File → Export → SVG
2. **Programmatically**:
   ```cpp
   std::string svg = doc.export_to_svg();
   doc.export_to_svg_file("output.svg");
   ```

### Export to PNG/Image

Use the application's screenshot or export feature to save as an image.

## Troubleshooting

### Diagram Not Loading

**Problem**: File doesn't load or shows errors

**Solutions**:
- Check JSON syntax is valid (use a JSON validator)
- Verify file path is correct
- Check console output for error messages
- Ensure all required fields are present

### Shapes Not Appearing

**Problem**: Diagram loads but shapes are invisible

**Solutions**:
- Check geometry values are within viewport
- Verify fill/stroke colors are set
- Check opacity is not 0
- Zoom out to see if shapes are off-screen

### Connectors Not Routing

**Problem**: Connectors don't appear or route incorrectly

**Solutions**:
- Verify connection points exist on shapes
- Check shape IDs match in connector definitions
- Ensure routing algorithm is appropriate
- Check connector style has stroke color

### Styles Not Applying

**Problem**: CSS styles don't affect shapes

**Solutions**:
- Verify class names match exactly
- Check selector syntax
- Ensure stylesheet is loaded
- Check specificity order

### Performance Issues

**Problem**: Diagram is slow or laggy

**Solutions**:
- Reduce number of shapes
- Simplify connector routing
- Use viewport culling
- Optimize expressions

## Advanced Usage

### Data-Driven Diagrams

Generate diagrams from external data sources:

```cpp
// Load data from database/API
auto data = load_data_from_source();

// Create DDF document
whiteboard::ddf::DDFDocument doc;

// Populate data layer
for (const auto& item : data) {
    whiteboard::ddf::DataNode node;
    node.id = item.id;
    node.type = item.type;
    node.properties = item.properties;
    doc.data_layer().add_node(node);
}

// Generate diagram with layout
doc.generate_from_data("tree");
```

### Real-Time Updates

Update diagrams in real-time:

```cpp
// Update data node
doc.data_layer().update_node("node1", {{"status", "offline"}});

// Shapes bound to this data will update automatically
doc.shape_layer().update_from_data("node1");

// Re-render
doc.render(nvg_context, viewport);
```

### Custom Components

Create and register custom components:

```cpp
whiteboard::ddf::ComponentDefinition custom_component;
custom_component.id = "my_component";
custom_component.name = "My Custom Component";
// ... define parameters and shapes

doc.component_layer().register_component(custom_component);
```

## Resources

- **User Guide**: `../../docs/ddf/USER_GUIDE.md`
- **CSS Styling Guide**: `../../docs/ddf/CSS_STYLING.md`
- **Expression Language**: `../../docs/ddf/EXPRESSIONS.md`
- **Component System**: `../../docs/ddf/COMPONENTS.md`
- **API Reference**: `../../docs/ddf/API_REFERENCE.md`

## Example Workflows

### Workflow 1: Create Organization Chart

1. Start with `org_chart.json`
2. Update employee data in `data.nodes`
3. Update reporting relationships in `data.relationships`
4. Adjust layout spacing if needed
5. Customize colors in styles
6. Load in modern_whiteboard.exe
7. Export to SVG

### Workflow 2: Design Network Topology

1. Start with `network_diagram.json`
2. Add your network devices to `data.nodes`
3. Define connections in `data.relationships`
4. Customize device component colors
5. Adjust force-directed layout parameters
6. Load and visualize
7. Export for documentation

### Workflow 3: Create Process Flowchart

1. Start with `flowchart.json`
2. Define process steps in `data.nodes`
3. Define flow relationships
4. Use appropriate flowchart components
5. Configure orthogonal routing
6. Add decision labels
7. Load and refine
8. Export final diagram

## Getting Help

If you encounter issues or have questions:

1. Check the documentation in `docs/ddf/`
2. Review example files for reference
3. Validate JSON syntax
4. Check console output for errors
5. Refer to the API documentation

## Contributing Examples

To contribute new examples:

1. Create a new JSON file in this directory
2. Follow the DDF format specification
3. Add documentation to this README
4. Test thoroughly in modern_whiteboard.exe
5. Submit for review

## License

These examples are provided as part of the DDF project and are available under the same license as the main project.
