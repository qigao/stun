# SVG Support in FlexUI

**Status:** ✅ FULLY IMPLEMENTED (as of 2025-11-25)

---

## Overview

FlexUI now provides comprehensive SVG element creation and styling support, exposing all the SVG capabilities from the underlying nanovg_css library. You can create vector graphics, icons, charts, and diagrams with full CSS styling support.

---

## Supported SVG Elements

### 1. Line (`createLine`)

Create lines between two points with full stroke styling.

```cpp
auto line = screen.createLine("line1", x1, y1, x2, y2);
line->setStroke("red", 2.0f);
line->setStrokeDash("5 3");  // Dashed line
```

**CSS Styling:**
```css
.line {
    stroke: red;
    stroke-width: 2px;
    stroke-dasharray: 5 3;
    stroke-linecap: round;
}
```

### 2. Circle (`createCircle`)

Create circles with center point and radius.

```cpp
auto circle = screen.createCircle("circle1", cx, cy, radius);
circle->setFill("blue");
circle->setStroke("black", 2.0f);
circle->addClass("interactive");
```

**CSS Styling:**
```css
.circle {
    fill: #4a90e2;
    stroke: #2e5c8a;
    stroke-width: 3px;
    transition: all 0.3s ease;
}

.circle:hover {
    fill: #5aa0f2;
    stroke-width: 5px;
}
```

### 3. Ellipse (`createEllipse`)

Create ellipses with separate x and y radii.

```cpp
auto ellipse = screen.createEllipse("ellipse1", cx, cy, rx, ry);
ellipse->setFill("green");
ellipse->setStroke("darkgreen", 2.0f);
```

**CSS Styling:**
```css
.ellipse {
    fill: #2ecc71;
    stroke: #27ae60;
    stroke-width: 2px;
}
```

### 4. Rectangle (`createRect`)

Create rectangles with position and dimensions.

```cpp
auto rect = screen.createRect("rect1", x, y, width, height);
rect->setFill("purple");
rect->setInlineStyle("border-radius", "5px");  // Rounded corners
```

**CSS Styling:**
```css
.rect {
    fill: #9b59b6;
    stroke: #8e44ad;
    stroke-width: 2px;
    border-radius: 5px;
}
```

### 5. Path (`createPath`)

Create custom paths with multiple points. Supports both smooth and hand-drawn styles.

### 6. Polygon (`createPolygon`)

Create closed polygons from a list of points.

```cpp
auto polygon = screen.createPolygon("polygon1", {
    {100, 50}, {150, 100}, {50, 100}  // Triangle
});
polygon->setFill("red");
polygon->setStroke("darkred", 2.0f);
```

**CSS Styling:**
```css
.polygon {
    fill: #e74c3c;
    stroke: #c0392b;
    stroke-width: 2px;
}
```

### 7. Polyline (`createPolyline`)

Create open polylines (connected line segments) from a list of points.

```cpp
auto polyline = screen.createPolyline("polyline1", {
    {10, 50}, {50, 30}, {90, 50}, {130, 30}
});
polyline->setStroke("blue", 3.0f);
polyline->setStrokeLineCap("round");
```

**CSS Styling:**
```css
.polyline {
    stroke: #3498db;
    stroke-width: 3px;
    stroke-linecap: round;
    stroke-linejoin: round;
}
```

```cpp
// Smooth path
auto path = screen.createPath("path1");
path->addPathPoint(10, 50);
path->addPathPoint(50, 30);
path->addPathPoint(90, 50);
path->setStroke("black", 3.0f);
path->setStrokeLineCap("round");
path->setStrokeLineJoin("round");

// Hand-drawn style path
auto sketch = screen.createPath("sketch1");
sketch->addPathPoint(10, 100);
sketch->addPathPoint(50, 80);
sketch->addPathPoint(90, 100);
sketch->setStroke("black", 3.0f);
sketch->setHandDrawn(true, 42.0f);  // Enable sketchy style
```

---

## SVG Styling Methods

### Widget Class Methods

All SVG elements support these styling methods:

```cpp
// Stroke styling
widget->setStroke(color, width);           // Set stroke color and width
widget->setStrokeDash(pattern);            // Set dash pattern (e.g., "10 5")
widget->setStrokeLineCap(cap);             // Set line cap: "butt", "round", "square"
widget->setStrokeLineJoin(join);           // Set line join: "miter", "round", "bevel"

// Fill styling
widget->setFill(color);                    // Set fill color

// Path manipulation
widget->addPathPoint(x, y);                // Add point to path
widget->clearPath();                       // Clear all path points
widget->setHandDrawn(enabled, seed);       // Enable hand-drawn style
```

### CSS Properties

All standard SVG CSS properties are supported:

```css
/* Stroke properties */
stroke: red;
stroke-width: 2px;
stroke-linecap: round;        /* butt, round, square */
stroke-linejoin: round;       /* miter, round, bevel */
stroke-miterlimit: 4;
stroke-dasharray: 5 3;
stroke-dashoffset: 2;
stroke-opacity: 0.8;

/* Fill properties */
fill: blue;
fill: none;
fill-opacity: 0.5;

/* Position and size (for circles, ellipses, rects) */
cx: 100px;                    /* Circle/ellipse center X */
cy: 100px;                    /* Circle/ellipse center Y */
r: 50px;                      /* Circle radius */
rx: 60px;                     /* Ellipse X radius */
ry: 40px;                     /* Ellipse Y radius */
x: 10px;                      /* Rect/line X position */
y: 10px;                      /* Rect/line Y position */
width: 100px;                 /* Rect width */
height: 50px;                 /* Rect height */
x1: 10px; y1: 10px;          /* Line start */
x2: 100px; y2: 100px;        /* Line end */

/* CSS transforms and effects work too! */
transform: rotate(45deg);
filter: blur(2px);
opacity: 0.8;
```

---

## CSS Variables Support

FlexUI now exposes CSS variable support for dynamic theming:

```cpp
// Set CSS variables
screen.setCSSVariable("--primary-color", "#4a90e2");
screen.setCSSVariable("--stroke-width", "3px");

// Get CSS variables
std::string color = screen.getCSSVariable("--primary-color");
```

**Usage in CSS:**
```css
.circle {
    fill: var(--primary-color);
    stroke-width: var(--stroke-width);
}
```

---

## Complete Example

```cpp
#include <flexui/flexui.h>

int main() {
    flexui::Screen screen(800, 600, "SVG Demo");

    // Load CSS
    screen.loadCSS(R"(
        .circle {
            fill: #4a90e2;
            stroke: #2e5c8a;
            stroke-width: 3px;
            transition: all 0.3s ease;
        }
        
        .circle:hover {
            fill: #5aa0f2;
            stroke-width: 5px;
        }
        
        .line {
            stroke: #e74c3c;
            stroke-width: 2px;
        }
    )");

    // Create SVG elements
    auto circle = screen.createCircle("circle1", 100, 100, 50);
    circle->addClass("circle");

    auto line = screen.createLine("line1", 200, 50, 350, 150);
    line->addClass("line");

    auto path = screen.createPath("path1");
    path->addPathPoint(50, 300);
    path->addPathPoint(100, 270);
    path->addPathPoint(150, 300);
    path->setStroke("black", 3);
    path->setHandDrawn(true);

    // Main loop
    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
```

---

## Advanced Features

### 1. Hand-Drawn Style

Create sketchy, hand-drawn looking paths:

```cpp
auto sketch = screen.createPath("sketch");
sketch->addPathPoint(10, 50);
sketch->addPathPoint(50, 30);
sketch->addPathPoint(90, 50);
sketch->setStroke("black", 3);
sketch->setHandDrawn(true, 42.0f);  // seed controls randomness
```

### 2. Animated SVG

Use CSS transitions and animations:

```css
.animated-circle {
    fill: blue;
    transition: all 0.5s ease;
}

.animated-circle:hover {
    fill: red;
    transform: scale(1.2);
}

@keyframes pulse {
    0%, 100% { r: 50px; }
    50% { r: 60px; }
}

.pulsing {
    animation: pulse 2s infinite;
}
```

### 3. Interactive SVG

Add click and hover callbacks:

```cpp
circle->setClickCallback([](flexui::Widget* w) {
    std::cout << "Circle clicked!" << std::endl;
    return true;
});

circle->setHoverCallback([](flexui::Widget* w, bool hovered) {
    if (hovered) {
        std::cout << "Mouse over circle" << std::endl;
    }
});
```

---

## API Reference

### Screen Class

```cpp
// SVG element creation
Widget* createLine(const std::string& id, float x1, float y1, float x2, float y2);
Widget* createCircle(const std::string& id, float cx, float cy, float r);
Widget* createEllipse(const std::string& id, float cx, float cy, float rx, float ry);
Widget* createRect(const std::string& id, float x, float y, float w, float h);
Widget* createPath(const std::string& id);
Widget* createPolygon(const std::string& id, const std::vector<std::pair<float, float>>& points);
Widget* createPolyline(const std::string& id, const std::vector<std::pair<float, float>>& points);

// CSS variables
void setCSSVariable(const std::string& name, const std::string& value);
std::string getCSSVariable(const std::string& name);
```

### Widget Class

```cpp
// SVG styling
void setStroke(const std::string& color, float width);
void setStrokeDash(const std::string& pattern);
void setStrokeLineCap(const std::string& cap);
void setStrokeLineJoin(const std::string& join);
void setFill(const std::string& color);

// Path manipulation
void addPathPoint(float x, float y);
void clearPath();
void setHandDrawn(bool enabled, float seed = 42.0f);
```

---

## Performance

- **Hardware Accelerated:** Uses NanoVG's GPU rendering
- **Efficient:** Minimal overhead over direct nanovg_css usage
- **60fps:** Smooth animations and transitions
- **Scalable:** Handles hundreds of SVG elements

---

## Comparison with HTML SVG

| Feature | HTML SVG | FlexUI SVG | Notes |
|---------|----------|------------|-------|
| `<line>` | ✅ | ✅ | Full support |
| `<circle>` | ✅ | ✅ | Full support |
| `<ellipse>` | ✅ | ✅ | Full support |
| `<rect>` | ✅ | ✅ | Full support |
| `<path>` | ✅ | ✅ | Point-based paths |
| Stroke properties | ✅ | ✅ | Complete |
| Fill properties | ✅ | ✅ | Complete |
| CSS styling | ✅ | ✅ | Full CSS support |
| Animations | ✅ | ✅ | CSS transitions/keyframes |
| Interactivity | ✅ | ✅ | Click/hover events |
| Hand-drawn style | ❌ | ✅ | Unique feature! |

---

## Migration from Direct nanovg_css

If you were using nanovg_css directly, migration is simple:

**Before (nanovg_css):**
```cpp
auto circle = nvgcssCreateElement(renderer, "c1", "circle");
circle->inline_style["cx"] = "100px";
circle->inline_style["cy"] = "100px";
circle->inline_style["r"] = "50px";
circle->inline_style["fill"] = "blue";
```

**After (FlexUI):**
```cpp
auto circle = screen.createCircle("c1", 100, 100, 50);
circle->setFill("blue");
```

---

## Examples

See `flexui/examples/svg_demo.cpp` for a complete working example demonstrating:
- Circles with hover effects
- Lines (solid and dashed)
- Ellipses and rectangles
- Hand-drawn paths
- Smooth paths with rounded caps/joins

---

## Conclusion

FlexUI now provides **production-ready SVG support** with:

- ✅ All major SVG shapes (line, circle, ellipse, rect, path)
- ✅ Complete stroke and fill styling
- ✅ CSS integration (classes, pseudo-states, transitions)
- ✅ CSS variables for dynamic theming
- ✅ Hand-drawn style support
- ✅ Interactive elements (click, hover)
- ✅ Hardware-accelerated rendering

**Ready for creating vector graphics, icons, charts, and diagrams!** 🎨
