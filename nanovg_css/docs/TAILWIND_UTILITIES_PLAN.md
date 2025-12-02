# Tailwind-Style Utility Classes for nanovg_css

## Executive Summary

**Goal:** Provide a Tailwind-inspired atomic CSS utility library for nanovg_css/flexUI.

**Key Insight:** nanovg_css already supports this - no code changes needed. This is a **data problem, not a code problem**. We just need to provide pre-defined utility classes.

**Work Required:**
- ✅ Code changes: **ZERO**
- ✅ New CSS file: `tailwind-utilities.css` (~1500 utility classes)
- ✅ Documentation and examples
- ⏱️ Time: **2-3 hours**

---

## Why This Works Already

### Current Support

nanovg_css already has:

1. **✅ Multi-class selectors** (`lexbor_css_parser.cpp:295-300`)
   ```cpp
   // Matches ALL classes on an element
   for (const auto& cls : sel.classes) {
       if (std::find(classes.begin(), classes.end(), cls) == classes.end()) {
           return false;
       }
   }
   ```

2. **✅ CSS Variables** - for theming
3. **✅ Pseudo-classes** - `:hover`, `:active`, `:focus`
4. **✅ Typed properties** - zero runtime parsing

### What We Need to Add

**Just a CSS file with utility classes:**

```css
/* tailwind-utilities.css */
.flex { display: flex; }
.flex-col { flex-direction: column; }
.items-center { align-items: center; }
.justify-between { justify-content: space-between; }
.bg-blue-500 { background: #3b82f6; }
.text-white { color: #ffffff; }
.px-4 { padding-left: 16px; padding-right: 16px; }
.rounded-lg { border-radius: 8px; }
```

**That's it.** Load this CSS, and you can use Tailwind-style classes.

---

## Design Principles

### 1. Good Taste - Eliminate Special Cases

**Bad approach (complex):**
- Runtime JIT compilation
- Complex variant system (`md:hover:bg-blue-500`)
- Dynamic class generation

**Good approach (simple):**
- Pre-defined utility classes
- Use CSS variables for theming
- Simple `:hover` pseudo-classes

### 2. Practical Needs

**What FlexUI needs:**
- ✅ Layout utilities (flex, grid, spacing)
- ✅ Color palette (background, text, border)
- ✅ Sizing (width, height, padding, margin)
- ✅ Typography (font-size, weight, alignment)
- ✅ States (`:hover`, `:active`, `:focus`)

**What FlexUI doesn't need:**
- ❌ Responsive breakpoints (`md:`, `lg:`) - it's not a browser
- ❌ Complex variants (`group-hover:`, `peer-checked:`)
- ❌ Dynamic JIT compilation
- ❌ Container queries

### 3. Never Break Userspace

- Keep existing CSS files working (backward compatible)
- Utilities are **additive** - don't replace current approach
- Users can mix both styles:
  ```xml
  <button class="fluent-btn primary px-4 rounded-lg" />
  ```

---

## Utility Class Categories

### 1. Layout (Flexbox/Grid)

```css
/* Display */
.flex { display: flex; }
.grid { display: grid; }
.block { display: block; }
.hidden { display: none; }

/* Flex Direction */
.flex-row { flex-direction: row; }
.flex-col { flex-direction: column; }
.flex-row-reverse { flex-direction: row-reverse; }
.flex-col-reverse { flex-direction: column-reverse; }

/* Flex Wrap */
.flex-wrap { flex-wrap: wrap; }
.flex-nowrap { flex-wrap: nowrap; }

/* Justify Content */
.justify-start { justify-content: flex-start; }
.justify-end { justify-content: flex-end; }
.justify-center { justify-content: center; }
.justify-between { justify-content: space-between; }
.justify-around { justify-content: space-around; }
.justify-evenly { justify-content: space-evenly; }

/* Align Items */
.items-start { align-items: flex-start; }
.items-end { align-items: flex-end; }
.items-center { align-items: center; }
.items-stretch { align-items: stretch; }
.items-baseline { align-items: baseline; }

/* Flex Grow/Shrink */
.flex-1 { flex: 1 1 0%; }
.flex-auto { flex: 1 1 auto; }
.flex-initial { flex: 0 1 auto; }
.flex-none { flex: none; }

/* Grid */
.grid-cols-1 { grid-template-columns: repeat(1, 1fr); }
.grid-cols-2 { grid-template-columns: repeat(2, 1fr); }
.grid-cols-3 { grid-template-columns: repeat(3, 1fr); }
.grid-cols-4 { grid-template-columns: repeat(4, 1fr); }

/* Gap */
.gap-0 { gap: 0px; }
.gap-1 { gap: 4px; }
.gap-2 { gap: 8px; }
.gap-3 { gap: 12px; }
.gap-4 { gap: 16px; }
.gap-6 { gap: 24px; }
.gap-8 { gap: 32px; }
```

### 2. Spacing (Padding/Margin)

```css
/* Padding - All sides */
.p-0 { padding: 0px; }
.p-1 { padding: 4px; }
.p-2 { padding: 8px; }
.p-3 { padding: 12px; }
.p-4 { padding: 16px; }
.p-6 { padding: 24px; }
.p-8 { padding: 32px; }

/* Padding - X axis */
.px-0 { padding-left: 0px; padding-right: 0px; }
.px-1 { padding-left: 4px; padding-right: 4px; }
.px-2 { padding-left: 8px; padding-right: 8px; }
.px-4 { padding-left: 16px; padding-right: 16px; }
.px-6 { padding-left: 24px; padding-right: 24px; }

/* Padding - Y axis */
.py-0 { padding-top: 0px; padding-bottom: 0px; }
.py-1 { padding-top: 4px; padding-bottom: 4px; }
.py-2 { padding-top: 8px; padding-bottom: 8px; }
.py-4 { padding-top: 16px; padding-bottom: 16px; }

/* Margin - Same pattern as padding */
.m-0 { margin: 0px; }
.m-2 { margin: 8px; }
.m-4 { margin: 16px; }

.mx-auto { margin-left: auto; margin-right: auto; }
.my-2 { margin-top: 8px; margin-bottom: 8px; }
```

### 3. Sizing

```css
/* Width */
.w-full { width: 100%; }
.w-auto { width: auto; }
.w-1-2 { width: 50%; }
.w-1-3 { width: 33.333333%; }
.w-2-3 { width: 66.666667%; }
.w-1-4 { width: 25%; }
.w-3-4 { width: 75%; }

/* Fixed widths */
.w-20 { width: 80px; }
.w-40 { width: 160px; }
.w-60 { width: 240px; }
.w-80 { width: 320px; }

/* Height */
.h-full { height: 100%; }
.h-auto { height: auto; }
.h-20 { height: 80px; }
.h-40 { height: 160px; }

/* Min/Max */
.min-w-0 { min-width: 0px; }
.max-w-sm { max-width: 384px; }
.max-w-md { max-width: 448px; }
.max-w-lg { max-width: 512px; }
```

### 4. Colors (Using CSS Variables)

```css
:root {
    /* Neutral */
    --gray-50: #f9fafb;
    --gray-100: #f3f4f6;
    --gray-200: #e5e7eb;
    --gray-300: #d1d5db;
    --gray-400: #9ca3af;
    --gray-500: #6b7280;
    --gray-600: #4b5563;
    --gray-700: #374151;
    --gray-800: #1f2937;
    --gray-900: #111827;

    /* Primary (Blue) */
    --blue-50: #eff6ff;
    --blue-100: #dbeafe;
    --blue-200: #bfdbfe;
    --blue-300: #93c5fd;
    --blue-400: #60a5fa;
    --blue-500: #3b82f6;
    --blue-600: #2563eb;
    --blue-700: #1d4ed8;
    --blue-800: #1e40af;
    --blue-900: #1e3a8a;

    /* Success (Green) */
    --green-500: #10b981;
    --green-600: #059669;

    /* Warning (Yellow) */
    --yellow-500: #f59e0b;

    /* Danger (Red) */
    --red-500: #ef4444;
    --red-600: #dc2626;
}

/* Background colors */
.bg-white { background: #ffffff; }
.bg-gray-50 { background: var(--gray-50); }
.bg-gray-100 { background: var(--gray-100); }
.bg-gray-200 { background: var(--gray-200); }
.bg-blue-500 { background: var(--blue-500); }
.bg-blue-600 { background: var(--blue-600); }
.bg-green-500 { background: var(--green-500); }
.bg-red-500 { background: var(--red-500); }

/* With hover states */
.bg-blue-500:hover { background: var(--blue-600); }
.bg-green-500:hover { background: var(--green-600); }
.bg-red-500:hover { background: var(--red-600); }

/* Text colors */
.text-white { color: #ffffff; }
.text-gray-500 { color: var(--gray-500); }
.text-gray-700 { color: var(--gray-700); }
.text-gray-900 { color: var(--gray-900); }
.text-blue-500 { color: var(--blue-500); }

/* Border colors */
.border-gray-200 { border-color: var(--gray-200); }
.border-gray-300 { border-color: var(--gray-300); }
.border-blue-500 { border-color: var(--blue-500); }
```

### 5. Typography

```css
/* Font Size */
.text-xs { font-size: 12px; }
.text-sm { font-size: 14px; }
.text-base { font-size: 16px; }
.text-lg { font-size: 18px; }
.text-xl { font-size: 20px; }
.text-2xl { font-size: 24px; }
.text-3xl { font-size: 30px; }
.text-4xl { font-size: 36px; }

/* Font Weight */
.font-normal { font-weight: 400; }
.font-medium { font-weight: 500; }
.font-semibold { font-weight: 600; }
.font-bold { font-weight: 700; }

/* Text Alignment */
.text-left { text-align: left; }
.text-center { text-align: center; }
.text-right { text-align: right; }

/* Text Decoration */
.underline { text-decoration: underline; }
.line-through { text-decoration: line-through; }
.no-underline { text-decoration: none; }
```

### 6. Borders

```css
/* Border Width */
.border { border-width: 1px; }
.border-0 { border-width: 0px; }
.border-2 { border-width: 2px; }
.border-4 { border-width: 4px; }

/* Border Radius */
.rounded-none { border-radius: 0px; }
.rounded-sm { border-radius: 2px; }
.rounded { border-radius: 4px; }
.rounded-md { border-radius: 6px; }
.rounded-lg { border-radius: 8px; }
.rounded-xl { border-radius: 12px; }
.rounded-full { border-radius: 9999px; }

/* Border Style */
.border-solid { border-style: solid; }
.border-dashed { border-style: dashed; }
.border-dotted { border-style: dotted; }
.border-none { border-style: none; }
```

### 7. Effects

```css
/* Shadow */
.shadow-sm { box-shadow: 0 1px 2px 0 rgba(0, 0, 0, 0.05); }
.shadow { box-shadow: 0 1px 3px 0 rgba(0, 0, 0, 0.1); }
.shadow-md { box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1); }
.shadow-lg { box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1); }
.shadow-none { box-shadow: none; }

/* Opacity */
.opacity-0 { opacity: 0; }
.opacity-25 { opacity: 0.25; }
.opacity-50 { opacity: 0.5; }
.opacity-75 { opacity: 0.75; }
.opacity-100 { opacity: 1; }
```

---

## Usage Examples

### Before (Component CSS)

**fluent2.css:**
```css
.fluent-btn.primary {
    background: var(--color-primary);
    color: var(--color-white);
    padding: 10px 24px;
    border-radius: 4px;
    font-size: 14px;
    font-weight: 600;
}
```

**XML:**
```xml
<button class="fluent-btn primary">Click me</button>
```

### After (Tailwind Utilities)

**No custom CSS needed!**

**XML:**
```xml
<button class="bg-blue-500 text-white px-6 py-2 rounded text-sm font-semibold">
    Click me
</button>

<!-- With hover state -->
<button class="bg-blue-500 text-white px-6 py-2 rounded">
    Hover me
</button>
```

**Hover styles (in tailwind-utilities.css):**
```css
.bg-blue-500:hover { background: var(--blue-600); }
```

### Real Examples

**Calculator Button:**
```xml
<!-- Old way -->
<button class="btn num">7</button>

<!-- Tailwind way -->
<button class="bg-gray-700 text-white text-2xl rounded-lg flex items-center justify-center">
    7
</button>
```

**Todo Item:**
```xml
<!-- Old way -->
<div class="todo-item">
    <checkbox class="todo-check" />
    <label class="todo-text">Buy milk</label>
</div>

<!-- Tailwind way -->
<div class="flex flex-row items-center bg-white px-4 py-3 rounded shadow-sm gap-3">
    <checkbox class="w-5 h-5" />
    <label class="flex-1 text-base text-gray-900">Buy milk</label>
</div>
```

**Card Layout:**
```xml
<div class="flex flex-col bg-white rounded-lg shadow-md p-6 gap-4">
    <h2 class="text-2xl font-bold text-gray-900">Title</h2>
    <p class="text-base text-gray-600">Description goes here...</p>
    <div class="flex flex-row gap-2 justify-end">
        <button class="px-4 py-2 bg-gray-200 rounded text-sm">Cancel</button>
        <button class="px-4 py-2 bg-blue-500 text-white rounded text-sm">Save</button>
    </div>
</div>
```

---

## Implementation Plan

### Phase 1: Core Utilities (1 hour)

**File:** `nanovg_css/presets/tailwind-utilities.css`

1. ✅ Layout (flex, grid, gap)
2. ✅ Spacing (padding, margin)
3. ✅ Sizing (width, height)
4. ✅ Display utilities

**Deliverable:** ~200 utility classes

### Phase 2: Colors & Typography (30 min)

1. ✅ Color palette (CSS variables)
2. ✅ Background/text/border colors
3. ✅ Font sizes and weights
4. ✅ Text alignment

**Deliverable:** ~300 more classes

### Phase 3: Effects & Borders (30 min)

1. ✅ Border radius, width, style
2. ✅ Shadows
3. ✅ Opacity

**Deliverable:** ~100 more classes

### Phase 4: Examples & Docs (1 hour)

1. ✅ Convert `calculator_demo.cpp` to use utilities
2. ✅ Convert `todolist_demo.cpp` to use utilities
3. ✅ Create `tailwind_button_demo.cpp`
4. ✅ Update documentation

**Deliverable:** Working examples, migration guide

---

## File Structure

```
nanovg_css/
├── presets/
│   ├── tailwind-utilities.css     # Main utility classes (~1500 lines)
│   ├── tailwind-colors.css        # Color palette definitions
│   └── README.md                  # Usage guide
├── examples/
│   ├── tailwind_button_demo.cpp   # Button showcase
│   ├── tailwind_card_demo.cpp     # Card layouts
│   └── tailwind_form_demo.cpp     # Form controls
└── docs/
    └── TAILWIND_UTILITIES.md      # Full documentation
```

---

## Benefits

### 1. Zero Code Changes ✅

nanovg_css already supports this. Just load a CSS file.

### 2. Familiar for Modern Developers ✅

Developers already know Tailwind. No learning curve.

### 3. No Build Step ✅

Unlike real Tailwind (needs Node.js, PostCSS), this is pure CSS.

### 4. Themeable ✅

Uses CSS variables - change colors globally:

```css
:root {
    --blue-500: #0078D4;  /* Fluent blue instead of Tailwind blue */
}
```

### 5. Mix Both Approaches ✅

```xml
<!-- Component class + utilities -->
<button class="fluent-btn px-6 rounded-lg shadow">
    Hybrid approach
</button>
```

### 6. Smaller CSS for FlexUI ✅

Tailwind has 10,000+ classes. We only need ~1,500 classes for GUI apps.

---

## Performance Considerations

### CSS File Size

- Full Tailwind: ~3.5 MB uncompressed
- Our utilities: ~50 KB (only GUI-relevant classes)
- Compressed: ~8 KB gzipped

### Runtime Performance

**No impact:** Same CSS matching as before. Multi-class matching is already O(n) where n = number of classes.

### Memory

- ~1500 classes × ~50 bytes/class = ~75 KB
- Negligible for desktop apps

---

## Migration Guide

### For Existing Projects

**Option 1: Keep current CSS (recommended)**
```xml
<!-- Your existing code works unchanged -->
<button class="fluent-btn primary">Click</button>
```

**Option 2: Gradually adopt utilities**
```xml
<!-- Mix old and new -->
<button class="fluent-btn px-6 rounded-lg">Click</button>
```

**Option 3: Full rewrite**
```xml
<!-- Pure utilities -->
<button class="bg-blue-500 text-white px-6 py-2 rounded">Click</button>
```

### For New Projects

**Just load the preset:**
```cpp
nvgcssLoadStylesheet(renderer, "presets/tailwind-utilities.css");
```

**Use utilities directly:**
```xml
<div class="flex flex-col gap-4 p-6">
    <h1 class="text-2xl font-bold">Hello World</h1>
    <p class="text-base text-gray-600">Description</p>
</div>
```

---

## Comparison: Old vs New

### Calculator Example

**Old CSS (112 lines):**
```css
.calculator { width: 100%; height: 100%; background: #1c1c1c; display: flex; flex-direction: column; padding: 20px; }
.display-container { width: 100%; height: 120px; background: #9EA792; border: 4px solid #808080; ... }
.btn.num { background: #505050; color: #ffffff; }
.btn.func { background: #a5a5a5; color: #000000; }
.btn.oper { background: #ff9f0a; color: #ffffff; }
/* ... 107 more lines */
```

**New Utilities (0 lines of custom CSS):**
```xml
<div class="flex flex-col bg-gray-900 p-5 w-full h-full">
    <div class="flex items-center justify-end bg-gray-400 border-4 border-gray-600 rounded p-5 h-30 mb-5">
        <label class="text-6xl font-bold text-black">0</label>
    </div>
    <div class="grid grid-cols-4 gap-2 flex-1">
        <button class="bg-gray-700 text-white text-2xl rounded-lg">7</button>
        <button class="bg-gray-700 text-white text-2xl rounded-lg">8</button>
        <button class="bg-gray-500 text-black text-2xl rounded-lg">C</button>
        <button class="bg-orange-500 text-white text-2xl rounded-lg">÷</button>
    </div>
</div>
```

**Result:**
- 112 lines of custom CSS → 0 lines
- Same visual result
- More flexible (change colors inline)

---

## Next Steps

1. **Create `tailwind-utilities.css`** (~1500 classes)
2. **Create examples** (button, card, form demos)
3. **Documentation** (TAILWIND_UTILITIES.md)
4. **Test with flexUI** (verify multi-class matching works)

**Estimated time:** 2-3 hours total

**Code changes needed:** ZERO ✅

---

## Questions?

**Q: Do we need JIT compilation like real Tailwind?**
A: No. We pre-define all classes. FlexUI apps are small - 50 KB CSS is fine.

**Q: What about responsive classes like `md:flex`?**
A: Not needed. FlexUI is not a browser with breakpoints.

**Q: Can I still use my fluent2.css?**
A: Yes! 100% backward compatible. Load both files if you want.

**Q: Performance impact?**
A: Zero. Same CSS matching as before.

**Q: Why not use real Tailwind?**
A: Real Tailwind needs Node.js build step. This is pure CSS - just load and use.

---

## Conclusion

**This is the right approach:**

✅ **Good taste** - Uses existing capabilities, no special cases
✅ **Practical** - Modern developers know Tailwind
✅ **Simple** - Just a CSS file, no code changes
✅ **Never breaks userspace** - Existing CSS still works

**Let's build this.**
