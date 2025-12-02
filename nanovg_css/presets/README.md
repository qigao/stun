# Tailwind Utilities for nanovg_css

A curated set of **Tailwind-inspired atomic CSS utility classes** for building modern UIs with nanovg_css and FlexUI.

## Features

✅ **~1500 utility classes** covering layouts, colors, typography, spacing, borders, and effects
✅ **Zero code changes required** - nanovg_css already supports multi-class selectors
✅ **No build step** - pure CSS, just load and use
✅ **CSS variables** for easy theming
✅ **Familiar syntax** - developers already know Tailwind
✅ **100% compatible** with existing component CSS

## Quick Start

### 1. Load the CSS file

```cpp
#include <nanovg_css.h>

NVGCSSRenderer* renderer = nvgcssCreateRenderer(vg);
nvgcssLoadStylesheet(renderer, "presets/tailwind-utilities.css");
```

### 2. Use utility classes in your XML

```xml
<button class="bg-blue-500 text-white px-6 py-2 rounded-lg font-semibold">
    Click me
</button>

<div class="flex flex-col gap-4 p-6 bg-white rounded-xl shadow-md">
    <label class="text-2xl font-bold text-gray-900">Card Title</label>
    <label class="text-base text-gray-600">Card description goes here...</label>
</div>
```

### 3. Done!

No custom CSS needed. Build complete UIs with atomic classes.

---

## Utility Class Categories

### Layout

**Display:**
```css
.flex .inline-flex .grid .block .hidden
```

**Flexbox:**
```css
.flex-row .flex-col .flex-wrap
.justify-start .justify-center .justify-between
.items-start .items-center .items-stretch
.flex-1 .flex-auto .flex-none
```

**Grid:**
```css
.grid-cols-1 .grid-cols-2 .grid-cols-3 .grid-cols-4
.col-span-1 .col-span-2 .col-span-full
.gap-0 .gap-2 .gap-4 .gap-6
```

### Spacing

**Padding:**
```css
.p-0 .p-1 .p-2 .p-4 .p-6 .p-8
.px-4 .py-2  /* X/Y axis */
.pt-4 .pr-2 .pb-4 .pl-2  /* Individual sides */
```

**Margin:**
```css
.m-0 .m-2 .m-4 .m-auto
.mx-auto .my-4
.mt-2 .mr-4 .mb-6 .ml-2
```

**Gap:**
```css
.gap-0 .gap-1 .gap-2 .gap-3 .gap-4 .gap-6 .gap-8
```

### Sizing

**Width:**
```css
.w-full .w-auto .w-1-2 .w-1-3 .w-1-4
.w-20 .w-40 .w-64 .w-96
.min-w-0 .max-w-sm .max-w-lg
```

**Height:**
```css
.h-full .h-auto .h-1-2 .h-1-4
.h-10 .h-20 .h-40 .h-64
.min-h-full .max-h-screen
```

### Typography

**Font Size:**
```css
.text-xs .text-sm .text-base .text-lg .text-xl
.text-2xl .text-3xl .text-4xl
```

**Font Weight:**
```css
.font-light .font-normal .font-medium .font-semibold .font-bold
```

**Text Alignment:**
```css
.text-left .text-center .text-right
.underline .line-through .no-underline
```

### Colors

**Background:**
```css
.bg-white .bg-black .bg-transparent
.bg-gray-50 .bg-gray-100 ... .bg-gray-900
.bg-blue-500 .bg-green-500 .bg-red-500 .bg-yellow-500
.bg-orange-500 .bg-purple-500 .bg-pink-500
```

**Text:**
```css
.text-white .text-black
.text-gray-500 .text-gray-700 .text-gray-900
.text-blue-500 .text-green-600 .text-red-500
```

**Border:**
```css
.border-gray-200 .border-gray-300
.border-blue-500 .border-green-500
```

**Hover States (built-in):**
```css
.bg-blue-500:hover   /* → bg-blue-600 */
.bg-green-500:hover  /* → bg-green-600 */
.bg-gray-100:hover   /* → bg-gray-200 */
```

### Borders

**Width:**
```css
.border .border-0 .border-2 .border-4
.border-t .border-r .border-b .border-l
```

**Style:**
```css
.border-solid .border-dashed .border-dotted .border-none
```

**Radius:**
```css
.rounded-none .rounded-sm .rounded .rounded-lg .rounded-xl .rounded-full
```

### Effects

**Shadows:**
```css
.shadow-sm .shadow .shadow-md .shadow-lg .shadow-xl
```

**Opacity:**
```css
.opacity-0 .opacity-25 .opacity-50 .opacity-75 .opacity-100
```

---

## Examples

### Button Variations

```xml
<!-- Primary button -->
<button class="bg-blue-500 text-white px-6 py-2 rounded font-medium">
    Primary
</button>

<!-- Secondary button -->
<button class="bg-gray-200 text-gray-800 px-6 py-2 rounded font-medium">
    Secondary
</button>

<!-- Outlined button -->
<button class="bg-white border-2 border-gray-300 text-gray-700 px-6 py-2 rounded">
    Outlined
</button>

<!-- Large success button -->
<button class="bg-green-500 text-white px-8 py-3 rounded-lg text-lg font-semibold">
    Success
</button>

<!-- Pill-shaped button -->
<button class="bg-purple-500 text-white px-6 py-2 rounded-full">
    Pill Button
</button>

<!-- Icon button -->
<button class="bg-blue-500 text-white w-10 h-10 rounded flex items-center justify-center">
    +
</button>
```

### Card Component

```xml
<div class="flex flex-col bg-white rounded-xl shadow-lg p-6 gap-4 max-w-sm">
    <!-- Card header -->
    <label class="text-2xl font-bold text-gray-900">Card Title</label>

    <!-- Card content -->
    <label class="text-base text-gray-600">
        This is a beautiful card component built entirely with utility classes.
    </label>

    <!-- Card actions -->
    <div class="flex flex-row gap-3 mt-2">
        <button class="flex-1 bg-blue-500 text-white px-4 py-2 rounded">
            Primary
        </button>
        <button class="px-4 py-2 border border-gray-300 text-gray-700 rounded">
            Cancel
        </button>
    </div>
</div>
```

### Stat Card

```xml
<div class="flex flex-col bg-white rounded-lg shadow p-6 gap-3">
    <label class="text-sm font-medium text-gray-500">Total Users</label>
    <label class="text-3xl font-bold text-gray-900">24,583</label>
    <label class="text-sm text-green-600">+12.5% from last month</label>
</div>
```

### Form Layout

```xml
<div class="flex flex-col gap-4 p-6 bg-white rounded-lg shadow">
    <!-- Form title -->
    <label class="text-xl font-bold text-gray-900 mb-2">Login Form</label>

    <!-- Email input -->
    <div class="flex flex-col gap-2">
        <label class="text-sm font-medium text-gray-700">Email</label>
        <input type="text" class="border border-gray-300 rounded px-4 py-2 w-full" />
    </div>

    <!-- Password input -->
    <div class="flex flex-col gap-2">
        <label class="text-sm font-medium text-gray-700">Password</label>
        <input type="password" class="border border-gray-300 rounded px-4 py-2 w-full" />
    </div>

    <!-- Submit button -->
    <button class="bg-blue-500 text-white px-6 py-3 rounded-lg font-semibold w-full mt-4">
        Sign In
    </button>
</div>
```

### Grid Layout

```xml
<div class="grid grid-cols-3 gap-6">
    <div class="bg-blue-100 p-6 rounded-lg">Column 1</div>
    <div class="bg-green-100 p-6 rounded-lg">Column 2</div>
    <div class="bg-purple-100 p-6 rounded-lg">Column 3</div>
</div>
```

### List with Avatars

```xml
<div class="flex flex-col gap-4">
    <!-- List item -->
    <div class="flex flex-row gap-4 items-center">
        <div class="w-12 h-12 bg-blue-500 rounded-full flex items-center justify-center flex-shrink-0">
            <label class="text-white font-bold">JD</label>
        </div>
        <div class="flex flex-col flex-1">
            <label class="text-base font-medium text-gray-900">John Doe</label>
            <label class="text-sm text-gray-500">john@example.com</label>
        </div>
    </div>
</div>
```

---

## Theming with CSS Variables

All colors use CSS variables, making theming easy:

```css
/* Custom theme - override in your CSS */
:root {
    --blue-500: #0078D4;  /* Fluent blue */
    --green-500: #10B981; /* Keep Tailwind green */
    --gray-900: #1A1A1A;  /* Darker gray */
}
```

Now all `.bg-blue-500` elements use your custom blue!

---

## Comparison: Before vs After

### Before (Component CSS)

**calculator.css (112 lines):**
```css
.calculator {
    width: 100%;
    height: 100%;
    background: #1c1c1c;
    display: flex;
    flex-direction: column;
    padding: 20px;
}

.btn.num {
    background: #505050;
    color: #ffffff;
    font-size: 24px;
    border-radius: 8px;
}

/* ... 107 more lines */
```

**XML:**
```xml
<button class="btn num">7</button>
```

### After (Tailwind Utilities)

**Custom CSS: 0 lines!**

**XML:**
```xml
<button class="bg-gray-700 text-white text-2xl rounded-lg px-6 py-4">
    7
</button>
```

**Benefits:**
- No custom CSS file needed
- Change styling inline (faster iteration)
- Consistent design system
- Less CSS to maintain

---

## Performance

| Metric | Value |
|--------|-------|
| **CSS File Size** | ~50 KB uncompressed, ~8 KB gzipped |
| **Number of Classes** | ~1500 utilities |
| **Runtime Impact** | Zero - same CSS matching as before |
| **Memory** | ~75 KB (negligible for desktop apps) |

---

## Mixing with Component CSS

You can use both approaches together:

```xml
<!-- Component class + utilities -->
<button class="fluent-btn px-6 rounded-lg shadow">
    Hybrid approach
</button>
```

Existing CSS files (`fluent2.css`, `calculator.css`) continue to work unchanged.

---

## Migration Guide

### For Existing Projects

**Option 1: Keep current approach (no changes needed)**
```xml
<button class="fluent-btn primary">Click</button>
```

**Option 2: Gradually adopt utilities**
```xml
<!-- Replace padding/margin/radius with utilities -->
<button class="fluent-btn px-6 py-2 rounded-lg">Click</button>
```

**Option 3: Full rewrite (when convenient)**
```xml
<button class="bg-blue-500 text-white px-6 py-2 rounded">Click</button>
```

### For New Projects

Just use utilities from the start:

```cpp
nvgcssLoadStylesheet(renderer, "presets/tailwind-utilities.css");
```

```xml
<div class="flex flex-col p-6 gap-4">
    <h1 class="text-2xl font-bold">Hello World</h1>
</div>
```

---

## Demo Applications

See complete working examples in `nanovg_css/examples/`:

### 1. Button Showcase (`tailwind_button_demo.cpp`)

Demonstrates all button variations:
- Size variations (sm, md, lg)
- Color variations (primary, secondary, semantic)
- Border radius (square, rounded, pill)
- Shadows and hover states
- Icon buttons and button groups

**Run:**
```bash
./tailwind_button_demo
```

### 2. Dashboard Layout (`tailwind_card_demo.cpp`)

A complete dashboard with:
- Stats cards grid
- Activity feed
- Quick actions sidebar
- Project cards with progress bars
- 100% built with utilities - **zero custom CSS**

**Run:**
```bash
./tailwind_card_demo
```

---

## Differences from Real Tailwind

| Feature | Real Tailwind | This Implementation |
|---------|---------------|---------------------|
| **Class Generation** | JIT compiler | Pre-defined classes |
| **Build Step** | Requires Node.js/PostCSS | None - pure CSS |
| **Responsive** | `md:flex`, `lg:p-4` | Not needed (no breakpoints) |
| **Variants** | `hover:`, `focus:`, `group-hover:` | Only `:hover` pseudo-class |
| **File Size** | 3.5 MB full, purged in production | 50 KB (GUI-focused subset) |
| **Customization** | `tailwind.config.js` | CSS variables in `:root` |

**Why these differences?**

FlexUI is a desktop GUI framework, not a web browser:
- No responsive breakpoints needed
- No complex variant system needed
- Smaller, focused class set is better
- Pure CSS is simpler than build tools

---

## FAQ

### Q: Do I need to change my nanovg_css code?

**A: No.** Just load the CSS file. Multi-class selectors already work.

### Q: Can I still use my existing CSS files?

**A: Yes!** 100% backward compatible. Load both:
```cpp
nvgcssLoadStylesheet(renderer, "fluent2.css");
nvgcssLoadStylesheet(renderer, "presets/tailwind-utilities.css");
```

### Q: What about responsive design like `md:flex`?

**A: Not needed.** FlexUI renders at one resolution. Use flexbox/grid for dynamic layouts instead.

### Q: Can I customize the colors?

**A: Yes!** Override CSS variables:
```css
:root {
    --blue-500: #YourColor;
}
```

### Q: Performance impact?

**A: Zero.** CSS matching is the same whether you have 1 class or 10 classes on an element.

### Q: Why not use real Tailwind with JIT?

**A: Simplicity.** Real Tailwind needs Node.js, PostCSS, build step. This is pure CSS - just load and use.

---

## Complete Color Palette

All colors available in 50-900 shades:

- **Gray**: `gray-50` to `gray-900`
- **Blue** (Primary): `blue-50` to `blue-900`
- **Green** (Success): `green-50` to `green-900`
- **Red** (Danger): `red-50` to `red-900`
- **Yellow** (Warning): `yellow-50` to `yellow-600`
- **Orange**: `orange-50` to `orange-900`
- **Purple**: `purple-50` to `purple-900`
- **Pink**: `pink-50` to `pink-900`

Each color works with:
- `bg-{color}-{shade}` - background
- `text-{color}-{shade}` - text color
- `border-{color}-{shade}` - border color

---

## Credits

Inspired by [Tailwind CSS](https://tailwindcss.com/) by Adam Wathan and the Tailwind Labs team.

Adapted for nanovg_css with a focus on desktop GUI applications.

---

## License

Same license as nanovg_css (check main project LICENSE).

---

## Get Started

Load the utilities and start building:

```cpp
nvgcssLoadStylesheet(renderer, "presets/tailwind-utilities.css");
```

```xml
<div class="flex flex-col items-center justify-center h-full bg-gray-100 gap-4">
    <label class="text-4xl font-bold text-gray-900">Welcome!</label>
    <button class="bg-blue-500 text-white px-8 py-3 rounded-lg font-semibold">
        Get Started
    </button>
</div>
```

**Happy building! 🎨**
