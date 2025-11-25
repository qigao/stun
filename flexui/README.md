# FlexUI - CSS-Driven GUI Library

A minimal GUI library built on top of nanovg_css, providing CSS-based styling for interactive applications.

## Features

- **CSS-First Design**: All styling through CSS (colors, layouts, animations, hover effects)
- **Simple Widget System**: Minimal C++ API for creating UI elements
- **SVG Support**: Create vector graphics with lines, circles, ellipses, rectangles, and paths
- **Event Handling**: Mouse events with callbacks
- **Flexbox/Grid Layouts**: Full CSS layout support via nanovg_css
- **CSS Variables**: Dynamic theming support
- **SDL3 Backend**: Cross-platform window management

## Quick Start

```cpp
#include <flexui/flexui.h>

int main() {
    // Create window
    flexui::Screen screen(800, 600, "My App");
    
    // Load CSS
    screen.loadCSS(R"(
        .button {
            width: 200px;
            height: 60px;
            background: #4a90e2;
            border-radius: 8px;
        }
        .button:hover {
            background: #5aa0f2;
        }
    )");
    
    // Create widget
    auto* button = screen.addWidget("rect");
    button->setClass("button");
    button->setPosition(300, 270);
    button->setClickCallback([](flexui::Widget* w) {
        std::cout << "Clicked!" << std::endl;
        return true;
    });
    
    // Main loop
    while (screen.pollEvents()) {
        screen.draw();
    }
    
    return 0;
}
```

## Building

```bash
cmake -B build -S .
cmake --build build --config Release
./build/bin/simple_button
```

## API Overview

### Screen
- `Screen(width, height, title)` - Create window
- `loadCSS(css_string)` - Load CSS styles
- `addWidget(tag)` - Create widget
- `pollEvents()` - Process events
- `draw()` - Render frame

### Widget
- `setClass(name)` - Add CSS class
- `setPosition(x, y)` - Set position
- `setSize(w, h)` - Set dimensions
- `addChild(widget)` - Add child widget
- `setClickCallback(fn)` - Handle clicks
- `setHoverCallback(fn)` - Handle hover

## Architecture

```
FlexUI (C++ Widget API)
    ↓
nanovg_css (CSS Engine)
    ↓
NanoVG (Vector Graphics)
    ↓
OpenGL (Rendering)
```

## License

Same as parent project (nanogui)
