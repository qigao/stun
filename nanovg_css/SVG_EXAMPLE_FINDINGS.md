# SVG Example - Debug Findings

**Date:** 2025-11-25  
**Issue:** Blank window when running example_svg

## Root Cause Analysis

### Logs Show:
```
CSS parsed: 1 rules          ← Should be ~15 rules
Created circle: visible=true, display=0  ← display=0 means Display::NONE
```

### Problem Identified:

**The CSS parser does not recognize SVG-specific properties:**
- `cx`, `cy`, `r` (circle coordinates/radius)
- `rx`, `ry` (ellipse radii)
- `x1`, `y1`, `x2`, `y2` (line coordinates)
- `fill` (SVG fill color)
- `stroke`, `stroke-width`, `stroke-dasharray` (SVG stroke properties)

These properties are **not part of the CSS specification** that lexbor parses. They are SVG attributes, not CSS properties.

### Why Only 1 Rule Parsed:

The parser likely only recognized the last rule block (`.title`) which uses standard CSS properties:
```css
.title {
    font-size: 18px;        ← Standard CSS
    font-weight: bold;      ← Standard CSS
    color: #2c3e50;         ← Standard CSS
}
```

All other rules with SVG properties were ignored/rejected by the parser.

### Why display=0 (NONE):

When no valid CSS properties are found for an element, it defaults to `Display::NONE`, making it invisible.

## Architecture Limitation

**nanovg_css was designed for HTML/CSS box model, not SVG:**

1. **CSS Parser (lexbor):** Parses standard CSS properties only
   - Recognizes: `width`, `height`, `background`, `border`, `padding`, etc.
   - Does NOT recognize: `cx`, `cy`, `r`, `fill`, `stroke`, etc.

2. **Layout Engine:** Computes box model layouts
   - Works with: rectangles, flexbox, grid
   - Does NOT work with: SVG coordinate systems

3. **Painter:** Has some SVG rendering code
   - Can render circles, lines, ellipses
   - BUT needs geometry data that isn't being set

## What Works vs What Doesn't

### ✅ Works (Standard CSS):
```css
.box {
    width: 100px;
    height: 100px;
    background: blue;
    border: 2px solid black;
    border-radius: 10px;
}
```

### ❌ Doesn't Work (SVG Properties):
```css
.circle {
    cx: 100px;      ← Not recognized
    cy: 100px;      ← Not recognized
    r: 40px;        ← Not recognized
    fill: blue;     ← Not recognized
    stroke: black;  ← Not recognized
}
```

## Solutions

### Option 1: Use Inline Styles (Workaround)
Instead of CSS, set SVG properties directly:
```cpp
auto circle = nvgcssCreateElement(renderer, "circle1", "circle");
circle->circle_geometry.defined = true;
circle->circle_geometry.cx = 100;
circle->circle_geometry.cy = 100;
circle->circle_geometry.rx = 40;
circle->circle_geometry.ry = 40;
circle->inline_style["fill"] = "#4a90e2";
circle->inline_style["stroke"] = "#2e5c8a";
circle->inline_style["stroke-width"] = "3px";
```

### Option 2: Use Box Model (Recommended)
Create shapes using standard CSS box model:
```cpp
auto circle = nvgcssCreateElement(renderer, "circle1", "div");
circle->inline_style["width"] = "80px";
circle->inline_style["height"] = "80px";
circle->inline_style["background"] = "#4a90e2";
circle->inline_style["border"] = "3px solid #2e5c8a";
circle->inline_style["border-radius"] = "50%";  // Makes it circular
circle->inline_style["position"] = "absolute";
circle->inline_style["left"] = "60px";
circle->inline_style["top"] = "60px";
```

### Option 3: Extend Parser (Major Work)
Would require:
1. Extending lexbor CSS parser to recognize SVG properties
2. Creating SVG-specific layout engine
3. Mapping SVG properties to geometry structures
4. Estimated effort: 2-3 weeks

## Recommendation

**Use Option 2 (Box Model)** for the example. This demonstrates what nanovg_css actually supports and provides a working example.

The SVG rendering code in the painter exists but was designed to be used programmatically (setting geometry directly), not through CSS parsing.

## Updated Documentation

The `SVG_SUPPORT.md` document should be updated to clarify:
- SVG rendering is supported **programmatically** (direct API calls)
- SVG properties are **NOT supported in CSS**
- Use standard CSS box model for declarative styling
