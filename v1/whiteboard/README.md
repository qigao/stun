# Modern Whiteboard Application

A modern collaborative whiteboard application built with NanoGUI, inspired by Miro and Figma.

## Architecture

The application uses a clean **Hierarchical MVC (Model-View-Controller)** architecture:

- **📚 Full Documentation**: See [MVC_ARCHITECTURE.md](MVC_ARCHITECTURE.md)
- **🏗️ Architecture Overview**: See [ARCHITECTURE_REVIEW.md](ARCHITECTURE_REVIEW.md)
- **✅ Unit Tests**: Model layer has comprehensive test coverage
- **📊 Code Quality**: Clean separation of concerns, highly maintainable

**Key Features:**
- **WhiteboardDocument**: Single source of truth for all state
- **Observer Pattern**: Automatic UI synchronization
- **MVC Triads**: Canvas, Layers, Properties, Text, Toolbar
- **Fully Testable**: Model layer has no UI dependencies
- **Extensible**: Easy to add new tools and features

## Current Features

- **Modern UI**: Clean, minimal interface with floating panels
- **Drawing Tools**: Pen, shapes (rectangle, circle, line, arrow), sticky notes
- **SVG Shape Support**: Complex reusable shapes (UML diagrams, flowcharts, tables)
  - Shape library with categorized shapes
  - In-place text editing
  - Parametric shape customization
  - High-quality vector rendering with ThorVG
- **File Operations**: Save and load whiteboard files (.whiteboard format)
  - Native file dialogs (Ctrl+O to open, Ctrl+S to save)
  - JSON-based file format with full state preservation
  - Auto-save functionality
- **Selection & Manipulation**: Multi-select, move, duplicate, bring to front/back
- **Navigation**: Zoom, pan (mouse wheel, hand tool, space bar + drag, middle mouse)
- **Multi-Page Support**: Create and switch between multiple canvas pages
- **Undo/Redo**: Per-page history with keyboard shortcuts (Ctrl+Z/Y)
- **Infinite Canvas**: Grid, minimap, coordinate display
- **Resizable UI**: Adjustable left sidebar and zoom panel
- **Per-Page State**: Tool selection, colors, pen sizes preserved per page
- **Layers Panel**: View and manage all objects including SVG shapes
- **Properties Panel**: Edit shape properties including SVG-specific attributes

## Planned Enhancements

See `.kiro/specs/whiteboard-enhancements/` for detailed specifications:

### Phase 1: Core Enhancements (High Priority)
1. **Text Tool** - Add text annotations and labels
2. **Copy/Paste Support** - Clipboard operations (Ctrl+C/V/X)
3. **Keyboard Shortcuts Display** - Help panel with all shortcuts (F1)
4. **Dark Mode** - Toggle between light and dark themes

### Phase 2: Advanced Manipulation (Medium Priority)
5. **Shape Rotation** - Rotate objects with handles
6. **Snap to Grid** - Precise alignment with grid snapping
7. **Ruler Guides** - Horizontal/vertical alignment guides
8. **Shape Properties Panel** - Edit stroke, fill, opacity, rotation

### Phase 3: Organization & Management (Medium Priority)
9. **Layers Panel** - View and manage all objects
10. **Search/Filter Shapes** - Find objects quickly (Ctrl+F)
11. **Auto-Save** - Automatic progress saving to local storage

### Phase 4: Content & Collaboration (Lower Priority)
12. **Image Import** - Drag and drop images onto canvas
13. **Export Functionality** - Save as PNG/SVG/PDF
14. **Templates** - Pre-made layouts (flowcharts, wireframes, etc.)
15. **Collaboration Indicators** - Simulated multi-user cursors

## Building

This project is part of the NanoGUI examples. To build:

```bash
# From the nanogui root directory
mkdir build
cd build
cmake ..
cmake --build .
```

The executable will be in `build/bin/whiteboard` (or `whiteboard.exe` on Windows).

## File Format

The whiteboard uses a JSON-based `.whiteboard` file format that preserves all document state:

```json
{
  "version": "1.0",
  "created": 1697472000000,
  "zoom": 1.0,
  "pan_offset": [0, 0],
  "strokes": [
    {
      "tool": 10,
      "svg_shape_id": "uml.class",
      "svg_data": "<svg>...</svg>",
      "svg_parameters": {
        "className": "Customer"
      },
      "points": [[100, 100]],
      "svg_scale_x": 1.0,
      "svg_scale_y": 1.0
    }
  ]
}
```

### Shape Library Format

Custom shape libraries can be added using the `shapes/library.json` format:

```json
{
  "name": "UML Shapes",
  "version": "1.0",
  "shapes": [
    {
      "id": "uml.class",
      "name": "UML Class",
      "category": "UML",
      "svg_path": "shapes/uml/class.svg",
      "thumbnail": "shapes/uml/class_thumb.png",
      "parameters": [
        {
          "id": "className",
          "label": "Class Name",
          "type": "text",
          "default": "ClassName",
          "svg_selector": "#className"
        }
      ]
    }
  ]
}
```

## Controls

### Mouse
- **Left Click**: Draw/create shapes
- **Right Click**: Context menu (when shapes selected)
- **Middle Mouse**: Pan canvas
- **Mouse Wheel**: Zoom in/out
- **Shift + Click**: Multi-select shapes

### Keyboard
- **Space + Drag**: Temporary pan mode
- **Ctrl+O**: Open file
- **Ctrl+S**: Save file
- **Ctrl+Shift+S**: Save as
- **Ctrl+Z**: Undo
- **Ctrl+Y**: Redo
- **Delete/Backspace**: Delete selected shapes
- **Double-click**: Edit text in SVG shapes
- **Esc**: Exit application

## Project Structure

```
whiteboard/
├── include/whiteboard/
│   ├── model/                    # Model layer - single source of truth
│   │   ├── whiteboard_document.h # All application state
│   │   ├── document_observer.h   # Observer interface
│   │   └── document_state.h      # Undo/redo snapshots
│   ├── canvas/                   # Canvas MVC triad
│   │   ├── canvas_view.h         # Rendering
│   │   └── canvas_controller.h   # Input handling
│   ├── toolbar/                  # Toolbar MVC triad
│   │   ├── toolbar_view.h
│   │   └── toolbar_controller.h
│   ├── panels/                   # Panel MVC triads
│   │   ├── layers_view.h         # Layers panel with SVG support
│   │   ├── layers_controller.h
│   │   ├── properties_view.h     # Properties panel with SVG support
│   │   ├── properties_controller.h
│   │   ├── text_view.h
│   │   ├── text_controller.h
│   │   ├── shape_library_view.h  # Shape library panel
│   │   └── shape_library_controller.h
│   ├── svg/                      # SVG rendering and management
│   │   ├── svg_renderer.h        # ThorVG integration
│   │   ├── svg_shape_library.h   # Shape library loader
│   │   └── svg_parameter_editor.h # Parameter editing dialog
│   ├── *_module.h                # Legacy modules
│   ├── search_bar.h              # Legacy search
│   ├── template_gallery.h        # Legacy templates
│   ├── serialization.h           # JSON serialization
│   └── modern_whiteboard_app.h   # Application shell
├── src/                          # Implementation files
├── test/                         # Unit tests
├── shapes/                       # SVG shape library
│   ├── library.json              # Shape definitions
│   ├── basic/                    # Basic shapes
│   ├── flowchart/                # Flowchart shapes
│   ├── uml/                      # UML diagram shapes
│   └── table/                    # Table shapes
├── MVC_ARCHITECTURE.md           # Detailed architecture guide
├── ARCHITECTURE_REVIEW.md        # Architecture overview
└── README.md                     # This file
```

## Development

To implement the planned enhancements:

1. Review the spec files in `.kiro/specs/whiteboard-enhancements/`
2. Open `tasks.md` to see the implementation plan
3. Tasks are organized by priority (Phase 1-4)
4. Each task includes detailed implementation steps and requirement references

## License

Part of NanoGUI - developed by Wenzel Jakob <wenzel.jakob@epfl.ch>
