# FlexMaid

**FlexMaid** is a high-performance Mermaid diagram renderer built on top of Flex/FlexUI, providing native C++ diagram rendering without browser dependencies.

## Features

- 🚀 **Fast**: Native C++ implementation, no browser overhead
- 🎨 **Native Rendering**: Uses ThorVG for hardware-accelerated vector graphics
- 📝 **Unified Parser**: Handwritten recursive-descent parser for robust, unified parsing
- 🔧 **Flexible**: Leverage Flex's layout engine and rendering capabilities
- 📦 **Standalone**: No Node.js, Chromium, or external dependencies

## Supported Diagram Types

- [x] Flowcharts (in progress)
- [ ] Sequence Diagrams
- [ ] Class Diagrams
- [ ] State Diagrams
- [ ] ER Diagrams
- [ ] Pie Charts
- [ ] Gantt Charts
- [ ] Timeline
- [ ] Journey
- [ ] Mindmap
- [ ] Git Graph
- [x] XY Charts
- [x] Requirement Diagrams
- [x] Sankey Diagrams

## Architecture

FlexMaid follows a streamlined pipeline:

```
Mermaid Text → Unified Parser (Handwritten) → IR (UnifiedDiagram) → Layout (Flex) → Render (ThorVG)
```

### Components

1. **Parser** (`parser/`): re2c lexer + lemon grammar → Diagram IR
2. **IR** (`ir/`): Intermediate representation (nodes, edges, styling)
3. **Layout** (`layout/`): Dagre-style graph layout using Flex
4. **Renderer** (`render/`): ThorVG-based SVG/PNG rendering

## Building

### Prerequisites

- CMake 3.20+
- C++20 compiler
- re2c (for lexer generation)
- lemon (for parser generation)
- Flex library
- ThorVG library

### Build Instructions

```bash
# Configure
cmake -B build -S . -DFLEXMAID_BUILD_TESTS=ON

# Build
cmake --build build

# Test
ctest --test-dir build
```

## Usage

### High-Level API

```cpp
#include <flex/modules/flexmaid/flexmaid.h>

using namespace flex::modules::flexmaid;

FlexMaid maid;

// Render to SVG
maid.render_to_svg(R"(
    flowchart LR
        A[Start] --> B{Decision}
        B -->|Yes| C[OK]
        B -->|No| D[Cancel]
)", "output.svg");

// Render to PNG
maid.render_to_png("diagram.mmd", "output.png", 1920, 1080);
```

### Pipeline Control

```cpp
FlexMaid maid;

// Parse
auto result = maid.parse("flowchart LR; A-->B");
if (!result.success()) {
    std::cerr << "Parse error: " << result.error.value() << std::endl;
    return;
}

// Layout
auto layout = maid.layout(*result.diagram);

// Render
maid.render(layout);

// Export
maid.to_svg_string();
```

### Custom Theme

```cpp
Theme theme = Theme::modern();
theme.primary_color = "#E0F2FE";
theme.line_color = "#0284C7";
theme.font_family = "Roboto, sans-serif";

maid.set_theme(theme);
```

## Development Status

FlexMaid is currently in **early development**. The initial focus is on:

1. ✅ Core infrastructure (IR, interfaces)
2. 🚧 Flowchart parser (re2c + lemon)
3. 🚧 Basic layout engine
4. 🚧 ThorVG renderer
5. ⏳ Additional diagram types

## Comparison with mermaid-rs-renderer

| Feature | FlexMaid | mermaid-rs-renderer |
|---------|----------|---------------------|
| Language | C++ | Rust |
| Parser | re2c + lemon | Regex-based |
| Layout | Flex layout engine | dagre-rust |
| Rendering | ThorVG (native) | String-based SVG |
| Integration | Native Flex/FlexUI | Standalone |
| Output | SVG, PNG (ThorVG) | SVG, PNG (resvg) |

## Contributing

Contributions are welcome! Please see the design document in `.kiro/specs/flexmaid/design.md` for architecture details.

## License

[License TBD - should match Flex/FlexUI license]

## Acknowledgments

- Inspired by [mermaid-rs-renderer](https://github.com/1jehuang/mermaid-rs-renderer)
- Uses [ThorVG](https://github.com/thorvg/thorvg) for rendering
- Built on [Flex](https://github.com/...) framework
