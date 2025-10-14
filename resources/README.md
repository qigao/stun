# NanoGUI Resources

This directory contains resources that are compiled into the NanoGUI library.

## Contents

### Fonts (TTF format)
- **FontAwesome-Solid.ttf** - Icon font (FontAwesome 5.10.1 free variant)
- **FluentSystemIcons-Resizable.ttf** - Fluent System Icons (line + filled glyphs)
- **Roboto-Regular.ttf** - Default sans-serif font
- **Roboto-Bold.ttf** - Bold sans-serif font
- **Inconsolata-Regular.ttf** - Monospace font
- **Inter-Regular.woff2** - Modern UI font (regular weight)
- **Inter-SemiBold.woff2** - Modern UI font (semi-bold weight)
- **Inter-Bold.woff2** - Modern UI font (bold weight)

### Shaders
Backend-specific shader files are selected based on `NANOGUI_BACKEND`:
- **\*.gl** - OpenGL shaders
- **\*.gles** - OpenGL ES shaders (GLES 2/3)
- **\*.metal** - Metal shaders (macOS/iOS)

Shader files:
- `imageview_vertex.*` - Vertex shader for image rendering
- `imageview_fragment.*` - Fragment shader for image rendering

### Build Scripts
- **bin2c.cmake** - Converts binary resources to C++ source files
- **fa-import.py** - Script to import FontAwesome icon definitions
- **check-style.sh** - Code style checking script
- **nanoguiConfig.cmake.in** - CMake package configuration template

### Lottie Animations
- **lottie/** - Directory containing Lottie animation files (e.g., spinner.json)

## Build Process

The `CMakeLists.txt` in this directory:

1. **Globs resources** - Collects all `.ttf` fonts and backend-specific shaders
2. **Precompiles Metal shaders** - On macOS, compiles `.metal` files to `.metallib` (unless `NANOGUI_SKIP_METAL_SHADER_PRECOMPILATION` is set)
3. **Runs bin2c** - Converts all resources into C++ arrays embedded in:
   - `nanogui_resources.cpp` - Resource data
   - `nanogui_resources.h` - Resource declarations

These generated files are then compiled into the main NanoGUI library.

## Icon System

NanoGUI uses **icon fonts** (TTF format) rather than SVG or PNG files. Two families
are provided out of the box:

- **FontAwesome 5.10.1 Solid** - Legacy icon set exposed via include/nanogui/icons.h.
- **Microsoft Fluent System Icons (Resizable)** - Fluent UI glyphs exposed via theme/fluent/include/nanogui/fluent_icons.h.

**Benefits**:
- Scalable without quality loss
- Single color (customizable via `nvgFillColor`)
- Compact binaries (~200KB for FontAwesome, ~1.4MB for Fluent System Icons)
- Fast rendering through NanoVG's text pipeline

### Example Usage
```cpp
// Create button with icon
auto btn = new Button(parent, "Save", FA_SAVE);

// Use icon in custom drawing
nvgFontFace(ctx, "icons");
nvgText(ctx, x, y, utf8(FA_CHECK).data(), nullptr);

// Fluent theme example
nvgFontFace(ctx, "fluent-icons");
nvgText(ctx, x, y, utf8(FLUENT_ICON_SEARCH).data(), nullptr);
```

