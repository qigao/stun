# Meta Editor SDK

A component-based vector graphics editor SDK built on Flex and FlexUI.

## Architecture

```
meta_editor/
├── include/meta_editor/
│   ├── core/
│   │   ├── editor.h          ← SDK entry point
│   │   ├── editor_event.h    ← Platform-agnostic events
│   │   └── sdl_adapter.h     ← SDL → EditorEvent conversion
│   ├── canvas.h              ← Layers, camera, coordinate conversion
│   ├── selection_manager.h   ← Selection state and style operations
│   ├── tool_manager.h        ← Tool registration and dispatch
│   ├── tool.h                ← Base class for custom tools
│   ├── command.h             ← Undo/redo system
│   ├── view/
│   │   ├── panel.h           ← Base class for custom panels
│   │   └── *.h               ← Built-in panels (optional)
│   └── tools/
│       └── *.h               ← Built-in tools
└── examples/
    ├── sdk_minimal.cpp       ← Minimal SDK usage
    ├── line_tool_demo.cpp    ← Custom tool + panel example
    └── demo.cpp              ← Full editor with all panels
```

## Usage Patterns

### Minimal (SDK Core Only)

```cpp
#include <meta_editor/core/editor.h>

Editor editor(800, 600);
editor.init();

// Handle events
EditorEvent ev = sdl_to_editor_event(sdl_event);
editor.handle_event(ev);

// Render
editor.update(dt);
editor.render(renderer);
```

### Custom Tool

```cpp
#include <meta_editor/tool.h>

class MyTool : public Tool {
public:
    const char* name() const override { return "MyTool"; }
    
    bool on_pointer_down(const Vec2& screen, const Vec2& world) override {
        // Use canvas_, selection_, commands_ (auto-injected)
        return true;
    }
};

// Register
editor.tools()->register_tool(std::make_unique<MyTool>());
editor.tools()->set_active_tool("MyTool");
```

### Custom Panel

```cpp
#include <meta_editor/view/panel.h>

class MyPanel : public Panel {
public:
    void render(flex::Renderer& r) override {
        render_background(r);
        // Draw your content...
    }
    
    bool handle_click(float x, float y) override {
        // Handle interaction
        return true;
    }
};

// Use
MyPanel panel;
panel.set_position(16, 200);
panel.render(renderer);
```

### Full Application (All Built-in Panels)

See `examples/demo.cpp` and `examples/full_editor_app.cpp` for a complete reference implementation with all built-in panels.

## Building

```bash
cmake -B build
cmake --build build

# Run examples
./build/sdk_minimal_demo      # Minimal SDK usage
./build/line_tool_demo        # Custom tool example
./build/meta_editor_demo      # Full editor with all panels
```

## Controls

| Key | Action |
|-----|--------|
| V | Select tool |
| P | Pen tool |
| L | Line tool (with arrows) |
| C | Connector tool |
| D | Freehand/Draw tool |
| R | Rectangle tool |
| O | Circle tool |
| E | Ellipse tool |
| T | Text tool |
| S | Star tool |
| G | Toggle snap to grid |
| A | Cycle arrow style (in Line tool) |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+G | Group |
| Ctrl+Shift+G | Ungroup |
| Middle mouse | Pan |
| Scroll | Zoom |
| Delete | Delete selection |

## Design Principles

1. **Component-based** - Use only what you need
2. **SDK-first** - Editor core has no UI dependencies
3. **Extensible** - Tool and Panel base classes for customization
4. **Platform-agnostic** - EditorEvent abstraction for any input system
