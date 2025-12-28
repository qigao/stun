# Meta Editor

A professional vector graphics editor built on Flex and FlexUI.

## Features

- **Layer-based canvas** with hierarchical organization
- **Camera system** with zoom and pan
- **Tool system** with pluggable tools:
  - Select Tool (V) - Selection and movement
  - Pen Tool (P) - Path drawing
  - Rectangle Tool (R) - Rectangle creation
  - Circle Tool (C) - Circle creation
- **Undo/Redo** command system
- **Snap to grid** support
- **Transform system** using Flex's matrix operations

## Building

### Prerequisites

- CMake 3.15+
- C++17 compiler
- SDL2
- ThorVG
- Flex (parent project)
- FlexUI (parent project)

### Build Steps

```bash
cd meta_editor
mkdir build
cd build
cmake ..
cmake --build .
```

## Running the Demo

```bash
./meta_editor_demo
```

### Controls

- **V** - Select tool
- **P** - Pen tool  
- **R** - Rectangle tool
- **C** - Circle tool
- **Middle Mouse + Drag** - Pan camera
- **Mouse Wheel** - Zoom at cursor
- **0** - Reset camera
- **Ctrl+Z** - Undo
- **Ctrl+Y** - Redo
- **ESC** - Quit

## Architecture

### Core Components

- **Canvas** - Main editing surface with layer management and camera
- **Layer** - Wrapper around `flex::Group` with metadata
- **SelectionManager** - Tracks selected nodes and renders indicators
- **ToolManager** - Manages active tool and routes events
- **CommandManager** - Undo/redo stack
- **MetaEditor** - Main application class

### Tools

- **Tool** - Base class for editing tools
- **SelectTool** - Selection, movement, and transformation
- **PenTool** - Path drawing with click-to-add-points
- **ShapeTool** - Basic shape creation (rect, circle, etc.)

### Design Principles

1. **Leverage Flex Scene Graph** - Use `flex::Group` as layers (no separate tree)
2. **Camera Transform** - Applied to content root, UI overlay in screen space
3. **Coordinate Conversion** - Screen ↔ World transforms for proper event handling
4. **Command Pattern** - All edits are commands for undo/redo
5. **Tool System** - Pluggable tools with consistent interface

## Project Structure

```
meta_editor/
├── include/meta_editor/
│   ├── canvas.h
│   ├── layer.h
│   ├── selection_manager.h
│   ├── tool_manager.h
│   ├── tool.h
│   ├── command.h
│   ├── meta_editor.h
│   └── tools/
│       ├── select_tool.h
│       ├── pen_tool.h
│       └── shape_tool.h
├── src/
│   ├── canvas.cpp
│   ├── layer.cpp
│   ├── selection_manager.cpp
│   ├── tool_manager.cpp
│   ├── command.cpp
│   ├── meta_editor.cpp
│   └── tools/
│       ├── select_tool.cpp
│       ├── pen_tool.cpp
│       └── shape_tool.cpp
├── examples/
│   └── demo.cpp
└── CMakeLists.txt
```

## Future Enhancements

- UI panels (toolbar, layer panel, property panel)
- More tools (text, transform, zoom)
- File I/O (save/load .flex files)
- Advanced features (guides, rulers, advanced snapping)
- Keyboard shortcuts
- Context menus

## License

Same as parent Flex project.
