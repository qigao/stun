# NanoVG CSS

**CSS-driven rendering for NanoVG - Standalone Module**

NanoVG CSS provides a declarative CSS-based layer on top of NanoVG's imperative Canvas API. Style your graphics with familiar CSS syntax and let NanoVG CSS handle the rendering.

**This module is completely self-contained** - it includes its own copy of NanoVG and can be built independently.

## Features

### Phase 1-3: Core CSS Features ✅
- **CSS Parsing** - Full CSS selector support (class, ID, type, pseudo-states)
- **Box Model** - Width, height, padding, margin, border
- **Colors & Gradients** - Linear and radial gradients with multi-color stops
- **Transforms** - Translate, rotate, scale, skew with transform-origin
- **Visual Effects** - Box shadows (multiple), border radius, opacity
- **Text Rendering** - Font size, family, color, alignment
- **CSS Variables** - `var(--name)` support
- **Pseudo-states** - `:hover`, `:active`, `:focus`, etc.

### Phase 4 Sprint 1: CSS Transitions ✅
- **Time Management** - Frame-based animation updates
- **Transitions** - Smooth property interpolation with `transition` property
- **Easing Functions** - Linear, ease, ease-in, ease-out, ease-in-out
- **Property Interpolation** - Numbers, colors, CSS units (px, %, deg)
- **Multiple Properties** - Transition multiple properties simultaneously

### Coming Soon
- **Phase 4 Sprint 2:** Keyframe animations (`@keyframes`)
- **Phase 4 Sprint 3:** Flexbox layout
- **Phase 4 Sprint 4:** Filter effects, background images

## Quick Start

### Include Headers

```cpp
#include <nanovg.h>
#include <cssbox.h>
```

### Create Renderer

```cpp
NVGcontext* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
cssboxRenderer* renderer = cssboxCreateRenderer(vg);
```

### Define CSS

```cpp
const char* css = R"(
    .button {
        width: 120px;
        height: 40px;
        background: linear-gradient(to bottom, #00adb5, #007c82);
        border-radius: 20px;
        opacity: 1.0;
        transition: all 0.3s ease-in-out;
    }

    .button:hover {
        opacity: 0.8;
        transform: scale(1.05);
    }
)";

cssboxParseCSS(renderer, css);
```

### Create Elements

```cpp
cssboxElement* button = cssboxCreateElement(renderer, "btn1", "rect");
cssboxAddClass(button, "button");
cssboxSetStyle(button, "x", "100px");
cssboxSetStyle(button, "y", "100px");
```

### Render Loop

```cpp
while (running) {
    // Update time and transitions
    cssboxUpdate(renderer, deltaTime);

    // Handle events
    if (mouseOverButton) {
        cssboxSetPseudoState(button, "hover", 1);
    } else {
        cssboxSetPseudoState(button, "hover", 0);
    }

    // Render
    nvgBeginFrame(vg, windowWidth, windowHeight, pixelRatio);
    cssboxRender(renderer);
    nvgEndFrame(vg);
}
```

## Building

NanoVG CSS is a standalone CMake module with NanoVG embedded:

```bash
# Build standalone
cd cssbox
cmake -B build -S .
cmake --build build --config Release

# Or as part of parent project
cd ..  # to nanogui root
cmake -B build -S .
cmake --build build --config Release --target cssbox

# Run tests
./build/Release/test_cssbox_phase4
```

### Dependencies

- **Lexbor** - Fast CSS/HTML parser (install via vcpkg: `vcpkg install lexbor`)
- **NanoVG** - Included internally (copy in `nanovg/` subdirectory)
- **OpenGL** - Required by NanoVG for rendering (platform-specific)

## API Reference

### Core Functions

```cpp
// Renderer management
cssboxRenderer* cssboxCreateRenderer(NVGcontext* vg);
void cssboxDeleteRenderer(cssboxRenderer* renderer);

// CSS management
int cssboxParseCSS(cssboxRenderer* renderer, const char* css);
void cssboxSetVariable(cssboxRenderer* renderer, const char* name, const char* value);

// Element management
cssboxElement* cssboxCreateElement(cssboxRenderer* renderer, const char* id, const char* type);
void cssboxAddClass(cssboxElement* element, const char* class_name);
void cssboxSetStyle(cssboxElement* element, const char* property, const char* value);
void cssboxSetText(cssboxElement* element, const char* text);

// State management
void cssboxSetPseudoState(cssboxElement* element, const char* state, int active);

// Rendering (Phase 4: Transitions)
void cssboxUpdate(cssboxRenderer* renderer, float delta_time);
void cssboxRender(cssboxRenderer* renderer);
void cssboxComputeLayout(cssboxRenderer* renderer);
```

## CSS Syntax Support

### Selectors
```css
.class { }          /* Class selector */
#id { }             /* ID selector */
type { }            /* Type selector */
:hover { }          /* Pseudo-state selector */
.class:hover { }    /* Compound selector */
```

### Properties

**Box Model:**
```css
width: 100px;
height: 50px;
padding: 10px;
margin: 5px;
border: 2px solid #333;
border-radius: 15px;
```

**Colors & Backgrounds:**
```css
background: blue;
background: #ff0000;
background: rgb(255, 0, 0);
background: rgba(255, 0, 0, 0.5);
background: linear-gradient(to bottom, red, blue);
background: radial-gradient(circle at center, white, black);
```

**Transforms:**
```css
transform: translate(10px, 20px);
transform: rotate(45deg);
transform: scale(1.5);
transform: skew(10deg, 20deg);
transform-origin: center center;
```

**Transitions (Phase 4):**
```css
transition: all 0.3s ease-in-out;
transition: opacity 0.5s linear;
transition: transform 0.4s ease-out;
```

**Text:**
```css
font-size: 16px;
font-family: "sans-serif";
color: #333;
text-align: center;
```

**Visual Effects:**
```css
opacity: 0.8;
box-shadow: 2px 2px 5px rgba(0,0,0,0.3);
box-shadow: 2px 2px 5px black, -2px -2px 5px white;
```

## Examples

### Button with Hover Effect

```cpp
const char* css = R"(
    .button {
        width: 120px;
        height: 40px;
        background: linear-gradient(to bottom, #4a90e2, #2e5c8a);
        border-radius: 5px;
        box-shadow: 0 2px 4px rgba(0,0,0,0.2);
        transition: all 0.2s ease-out;
    }

    .button:hover {
        background: linear-gradient(to bottom, #5aa0f2, #3e6c9a);
        box-shadow: 0 4px 8px rgba(0,0,0,0.3);
        transform: translateY(-2px);
    }

    .button:active {
        transform: translateY(0px);
        box-shadow: 0 1px 2px rgba(0,0,0,0.2);
    }
)";
```

### Animated Card

```cpp
const char* css = R"(
    .card {
        width: 300px;
        height: 200px;
        background: white;
        border: 1px solid #ddd;
        border-radius: 10px;
        box-shadow: 2px 2px 10px rgba(0,0,0,0.1);
        transform: scale(1);
        transition: all 0.3s ease-in-out;
    }

    .card:hover {
        transform: scale(1.05);
        box-shadow: 4px 4px 20px rgba(0,0,0,0.2);
    }
)";
```

## Test Coverage

- **Phase 1:** 13/13 tests passing ✅
- **Phase 2:** 8/8 tests passing ✅
- **Phase 3:** 7/7 tests passing ✅
- **Phase 4 Sprint 1:** 5/5 tests passing ✅

**Total:** 33/33 tests passing (100%)

## Documentation

See `docs/` directory for detailed documentation:
- `PHASE2_SUMMARY.md` - Visual effects implementation
- `PHASE3_SUMMARY.md` - Advanced gradients and text
- `PHASE4_PLAN.md` - Animation and layout roadmap
- `PHASE4_SPRINT1_SUMMARY.md` - CSS transitions implementation

## Architecture

NanoVG CSS is designed as a lightweight, standalone CSS engine:

- **Embedded NanoVG** - Own copy of NanoVG (in `nanovg/` subdirectory)
- **SimpleStyleSheet** - Standalone CSS parser with specificity calculation
- **cssboxPainter** - Translates CSS properties to NanoVG calls
- **cssboxLayoutEngine** - Box model and transform computation
- **TransitionState** - Per-element animation state tracking

### Module Structure

```
cssbox/
├── nanovg/              # Embedded NanoVG sources
│   ├── nanovg.c
│   ├── nanovg.h
│   └── ...
├── src/                 # CSS implementation
│   ├── cssbox.cpp
│   ├── cssbox_stylesheet.cpp
│   ├── cssbox_painter.cpp
│   ├── cssbox_layout.cpp
│   └── cssbox_utils.cpp
├── include/             # Public API
│   └── cssbox.h
├── test/                # Test suite
└── docs/                # Documentation
```

## Performance

- **No heap allocations during rendering** - Pre-computed layouts
- **Efficient property matching** - Sorted rules by specificity
- **Smart dirty flags** - Only recompute when needed
- **Transition optimizations** - Only active transitions processed

## License

Same as NanoVG - see parent project license.

## Credits

Built on top of [NanoVG](https://github.com/memononen/nanovg) by Mikko Mononen.

CSS parsing powered by [Lexbor](https://github.com/lexbor/lexbor).
