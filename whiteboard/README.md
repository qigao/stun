# Modern Whiteboard Application

A modern collaborative whiteboard application built with NanoGUI, inspired by Miro and Figma.

## Current Features

- **Modern UI**: Clean, minimal interface with floating panels
- **Drawing Tools**: Pen, shapes (rectangle, circle, line, arrow), sticky notes
- **Selection & Manipulation**: Multi-select, move, duplicate, bring to front/back
- **Navigation**: Zoom, pan (mouse wheel, hand tool, space bar + drag, middle mouse)
- **Multi-Page Support**: Create and switch between multiple canvas pages
- **Undo/Redo**: Per-page history with keyboard shortcuts (Ctrl+Z/Y)
- **Infinite Canvas**: Grid, minimap, coordinate display
- **Resizable UI**: Adjustable left sidebar and zoom panel
- **Per-Page State**: Tool selection, colors, pen sizes preserved per page

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

## Controls

### Mouse
- **Left Click**: Draw/create shapes
- **Right Click**: Context menu (when shapes selected)
- **Middle Mouse**: Pan canvas
- **Mouse Wheel**: Zoom in/out
- **Shift + Click**: Multi-select shapes

### Keyboard
- **Space + Drag**: Temporary pan mode
- **Ctrl+Z**: Undo
- **Ctrl+Y**: Redo
- **Delete/Backspace**: Delete selected shapes
- **Esc**: Exit application

## Project Structure

```
whiteboard/
├── whiteboard.cpp          # Main application source
├── .kiro/
│   └── specs/
│       └── whiteboard-enhancements/
│           ├── requirements.md  # Feature requirements
│           ├── design.md        # Technical design
│           └── tasks.md         # Implementation tasks
└── README.md               # This file
```

## Development

To implement the planned enhancements:

1. Review the spec files in `.kiro/specs/whiteboard-enhancements/`
2. Open `tasks.md` to see the implementation plan
3. Tasks are organized by priority (Phase 1-4)
4. Each task includes detailed implementation steps and requirement references

## License

Part of NanoGUI - developed by Wenzel Jakob <wenzel.jakob@epfl.ch>
