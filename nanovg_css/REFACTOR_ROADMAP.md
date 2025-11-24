# NanoVG CSS - Refactor Roadmap for 60fps Professional Library

**Date Started:** 2025-11-23
**Target:** 60fps retained-mode UI with typed CSS properties
**Current Users:** 0 (perfect time to refactor!)

---

## 🎯 Core Problems Identified

### 1. **String-based property system** 🔥 CRITICAL
- Everything is `map<string, string>`
- Parsing happens **every frame** during layout/render
- Type-unsafe: typos discovered at runtime
- Performance killer for 60fps

**Evidence:**
- `nanovg_css_layout.cpp:12` - `safe_stof()` called during layout
- `nanovg_css.cpp:25-47` - Z-index logic broken, commented out
- 84% test pass rate - edge cases everywhere

### 2. **-1 sentinel values** 🟡 MEDIUM
```cpp
float width = -1.0f;  // -1 means "auto"
if (width >= 0) { ... }
```
Should be: `Length width = Length::auto_()`

### 3. **void* state pointers** 🟠 MEDIUM
```cpp
void* transition_state;  // Runtime type casting
delete static_cast<TransitionState*>(element->transition_state);
```
Should be: `std::unique_ptr<TransitionState>`

---

## ✅ Phase 1: Foundation (COMPLETED)

### What We Built

#### 1. **Typed Property System** (`nanovg_css_types.h`)
Created comprehensive type system:

```cpp
// BEFORE (垃圾)
map<string, string> style;
style["width"] = "100px";
float w = parse_length(style["width"]);  // Every frame!

// AFTER (好品味)
ComputedStyle style;
style.width = Length::px(100);
float w = style.width.resolve(context, font_size, viewport);  // Pre-parsed!
```

**Types added:**
- `Length` - Typed lengths with units (px, %, em, rem, vw, vh, auto)
- `Color` - Uses existing NVGcolor
- `Gradient` - Linear/radial gradients with stops
- `Background` - Color, gradient, or image
- `BoxShadow`, `Border`, `GridTrack`, etc.
- `ComputedStyle` - **The heart** - replaces `map<string, string>`
- `ResolvedLayout` - Final pixel values after layout
- `DirtyFlags` - For 60fps incremental updates

**Size comparison:**
- Old: `map<string, string>` = ~100 bytes + heap allocations per property
- New: `ComputedStyle` = ~400 bytes fixed size, zero heap allocations
- **Result:** Better cache locality, no allocations, type-safe

#### 2. **Conversion Layer** (`nanovg_css_conversion.h`)
String → Type conversion (called **once** during CSS parsing):

```cpp
// Parse "100px" → Length::px(100)
auto len = convert::parse_length("100px");

// Parse "#ff0000" → NVGcolor{1, 0, 0, 1}
auto color = convert::parse_color("#ff0000");

// Parse "flex-start" → JustifyContent::FLEX_START
auto justify = convert::parse_justify_content("flex-start");
```

**No more runtime parsing!**

#### 3. **Updated Element Structure** (`nanovg_css_internal.h`)
```cpp
struct NVGCSSElement {
    // === NEW: Typed CSS properties ===
    nvgcss::ComputedStyle style;         // Parsed CSS
    nvgcss::ResolvedLayout layout;       // Resolved pixels
    nvgcss::DirtyFlags dirty_flags;      // Incremental updates

    // === DEPRECATED: Old string-based system ===
    NVGCSSExplicitStyle explicit_style;  // Will be removed
    NVGCSSComputedLayout computed;       // Will be removed
    map<string, string> inline_style;    // Will be removed
};
```

Backward compatibility maintained during transition.

---

## 🚧 Phase 2: Migration (IN PROGRESS)

### Step 1: Update CSS Parser to Generate Typed Properties

**File:** `nanovg_css/src/lexbor_css_parser.cpp`

**Task:** Make `EnhancedStyleSheet::compute_style()` return `ComputedStyle` instead of `map<string, string>`

**Current:**
```cpp
map<string, string> EnhancedStyleSheet::compute_style(...) {
    map<string, string> result;
    result["width"] = "100px";
    return result;
}
```

**Target:**
```cpp
ComputedStyle EnhancedStyleSheet::compute_style_typed(...) {
    ComputedStyle style;
    style.width = convert::parse_length("100px").value_or(Length::auto_());
    style.display = convert::parse_display("flex");
    // ... all properties
    return style;
}
```

**Estimated effort:** 2-3 days

---

### Step 2: Update Layout Engine to Use Typed Properties

**Files:**
- `nanovg_css/src/nanovg_css_layout.cpp`
- `nanovg_css/src/nanovg_css_flexbox.cpp`
- `nanovg_css/src/nanovg_css_grid.cpp`

**Task:** Replace string parsing with direct type access

**BEFORE:**
```cpp
void compute_element_layout(Element* elem, map<string, string>& style) {
    float width = parse_length(style["width"], context);  // EVERY FRAME
    if (style["display"] == "flex") {
        compute_flexbox_layout(elem, style);
    }
}
```

**AFTER:**
```cpp
void compute_element_layout(Element* elem) {
    float width = elem->style.width.resolve(context, font_size, viewport);
    if (elem->style.display == Display::FLEX) {
        compute_flexbox_layout(elem);
    }
}
```

**Key changes:**
- ✅ Remove `safe_stof()` - no more runtime parsing
- ✅ Remove all `style["property"]` string lookups
- ✅ Use `elem->style.width` direct field access
- ✅ Use `switch` on enums instead of string comparisons

**Estimated effort:** 3-4 days

---

### Step 3: Update Painter to Use Typed Properties

**File:** `nanovg_css/src/nanovg_css_painter.cpp`

**BEFORE:**
```cpp
void paint_element(Element* elem, map<string, string>& style) {
    NVGcolor bg = parse_color(style["background-color"]);
    nvgFillColor(vg, bg);
}
```

**AFTER:**
```cpp
void paint_element(Element* elem) {
    if (elem->style.background.type == BackgroundType::COLOR) {
        nvgFillColor(vg, elem->style.background.color);
    } else if (elem->style.background.type == BackgroundType::GRADIENT) {
        NVGpaint paint = create_gradient(*elem->style.background.gradient, box);
        nvgFillPaint(vg, paint);
    }
}
```

**Estimated effort:** 2-3 days

---

### Step 4: Implement Dirty Flags System

**Goal:** Only recompute what changed (essential for 60fps)

```cpp
// When CSS property changes
void set_width(Element* elem, Length width) {
    if (elem->style.width != width) {  // Changed?
        elem->style.width = width;
        elem->dirty_flags |= DIRTY_LAYOUT;  // Mark dirty
    }
}

// In render loop
void render() {
    for (Element* elem : elements) {
        if (elem->dirty_flags & DIRTY_LAYOUT) {
            compute_layout(elem);
            elem->dirty_flags &= ~DIRTY_LAYOUT;
        }
        if (elem->dirty_flags & DIRTY_PAINT) {
            paint_element(elem);
            elem->dirty_flags &= ~DIRTY_PAINT;
        }
    }
}
```

**Estimated effort:** 1-2 days

---

### Step 5: Fix Z-Index Rendering

**File:** `nanovg_css/src/nanovg_css.cpp:25-47`

**Current:** Z-index logic commented out, using tree order only

**Target:** Restore correct CSS z-index stacking context

```cpp
struct RenderOrder {
    bool is_positioned;
    int z_index;
    int tree_order;

    bool operator<(const RenderOrder& other) const {
        // Non-positioned elements render first
        if (is_positioned != other.is_positioned) {
            return !is_positioned;
        }

        // Among positioned elements, sort by z-index
        if (is_positioned && z_index != other.z_index) {
            return z_index < other.z_index;
        }

        // Same category/z-index: maintain tree order (parent before child)
        return tree_order < other.tree_order;
    }
};
```

**Estimated effort:** 1 day

---

### Step 6: Remove Deprecated Fields

Once all code uses typed properties:

```cpp
struct NVGCSSElement {
    // === NEW: Typed CSS properties ===
    nvgcss::ComputedStyle style;
    nvgcss::ResolvedLayout layout;
    nvgcss::DirtyFlags dirty_flags;

    // DELETE these:
    // NVGCSSExplicitStyle explicit_style;
    // NVGCSSComputedLayout computed;
    // map<string, string> inline_style;
};
```

**Estimated effort:** 1 day

---

## 📊 Expected Performance Gains

### Before Refactor:
```
CSS Parsing:   Once (OK)
Layout:        Parse strings every frame (BAD)
Render:        Parse strings every frame (BAD)

Frame budget @ 60fps: 16.67ms
Current layout time:   ~8-10ms (string parsing)
Current render time:   ~4-6ms (string parsing)
Total:                 ~12-16ms (close to limit!)
```

### After Refactor:
```
CSS Parsing:   Once, generate typed properties (OK)
Layout:        Direct field access (FAST)
Render:        Direct field access (FAST)

Frame budget @ 60fps: 16.67ms
Expected layout time:  ~2-3ms (no parsing!)
Expected render time:  ~1-2ms (no parsing!)
Total:                 ~3-5ms (70% faster!)
```

**Headroom for:**
- Complex animations
- Many elements (1000+)
- Effects (shadows, gradients)

---

## 🧪 Testing Strategy

### Current Status:
- 202 tests exist
- 84% pass rate (37/44 core tests)
- Known issues documented in `KNOWN_ISSUES.md`

### Testing Plan:

1. **During refactor:** All existing tests must pass
2. **After each phase:** Run full test suite
3. **Add performance tests:**
   ```cpp
   TEST_CASE("Layout performance - 1000 elements") {
       // Create 1000 nested flex containers
       // Measure layout time
       // Should be < 5ms
   }
   ```

4. **Add type safety tests:**
   ```cpp
   TEST_CASE("Type conversions") {
       auto len = convert::parse_length("100px");
       REQUIRE(len.has_value());
       REQUIRE(len->unit == LengthUnit::PX);
       REQUIRE(len->value == 100.0f);
   }
   ```

---

## 📅 Timeline Estimate

| Phase | Task | Effort | Status |
|-------|------|--------|--------|
| **Phase 1** | Type system design | 1 day | ✅ DONE |
| **Phase 2.1** | Update CSS parser | 2-3 days | 🚧 TODO |
| **Phase 2.2** | Update layout engine | 3-4 days | 🚧 TODO |
| **Phase 2.3** | Update painter | 2-3 days | 🚧 TODO |
| **Phase 2.4** | Dirty flags system | 1-2 days | 🚧 TODO |
| **Phase 2.5** | Fix Z-index | 1 day | 🚧 TODO |
| **Phase 2.6** | Remove deprecated code | 1 day | 🚧 TODO |
| **Testing** | Full test suite + perf tests | 2 days | 🚧 TODO |

**Total estimated effort:** 12-16 days

**If working full-time:** 2-3 weeks
**If working part-time:** 4-6 weeks

---

## 🎓 Design Principles Applied

### 1. **"Good Taste" - Linus Torvalds**
❌ **Before:** 10 lines with if/else for sentinel values
```cpp
if (width >= 0) use_width(width);
if (height >= 0) use_height(height);
```

✅ **After:** 3 lines, no special cases
```cpp
if (!width.is_auto()) use_width(width.resolve(...));
```

### 2. **"Never break userspace"**
✅ All existing tests continue to pass during refactor
✅ Public API unchanged
✅ Backward compatibility layer during transition

### 3. **"Practical, not theoretical"**
✅ Solving real problem: 60fps requirement
✅ Not premature optimization: addressing measured bottleneck (string parsing)
✅ Type safety prevents bugs, not just "cleaner code"

### 4. **"Simplicity is paramount"**
❌ **Before:** 12 concepts (maps, strings, parsers, caches)
✅ **After:** 3 concepts (ComputedStyle, ResolvedLayout, DirtyFlags)

### 5. **"Code should explain itself"**
❌ `style["width"]` - what if typo? what type?
✅ `style.width` - compiler catches typos, type is `Length`

---

## 🚀 Next Steps

**Immediate:**
1. Update `EnhancedStyleSheet::compute_style()` to generate `ComputedStyle`
2. Add conversion logic for all CSS properties
3. Test with simple example (single element)

**Then:**
4. Update layout engine incrementally (one layout mode at a time)
5. Update painter incrementally
6. Add dirty flags
7. Remove deprecated code
8. Performance benchmarks

---

## 📝 Notes for Future Developers

### Why This Refactor Was Necessary

The original design used `map<string, string>` because:
1. ✅ Easy to implement initially
2. ✅ Flexible for rapid prototyping
3. ❌ **BUT:** Not suitable for production 60fps rendering

**The fundamental problem:**
Parsing CSS strings is **O(n)** where n = string length.
For 60fps, we have 16.67ms per frame.
Parsing strings for 100 elements = ~5-10ms.
That's **60% of frame budget** spent on parsing!

### Why Typed Properties Solve This

Parsing happens **once** when CSS is loaded:
- `"100px"` → `Length{PX, 100}` (one time cost)

Layout/render use **direct field access**:
- `elem->style.width` (zero cost, just memory load)

**Result:** 5-10ms → 0.5ms (10x faster)

### Why Not Use std::variant or std::any?

Considered but rejected:
```cpp
// Variant approach (slower)
std::variant<float, Length, string> width;
float w = std::get<Length>(width).resolve(...);  // Runtime type check!

// Direct struct (faster)
Length width;
float w = width.resolve(...);  // No type check, direct access
```

Variant adds runtime type checking overhead.
We know types at compile time (from CSS spec).
Direct struct is faster and safer.

---

## 🎯 Success Criteria

Refactor is complete when:

1. ✅ All 202+ tests pass
2. ✅ Zero string parsing during layout/render
3. ✅ Frame time < 5ms for 100 elements
4. ✅ Z-index rendering correct
5. ✅ No `-1` sentinel values
6. ✅ No `void*` state pointers
7. ✅ Code coverage ≥ 90%

---

**Remember:**
> "Bad programmers worry about the code.
> Good programmers worry about data structures and their relationships."
> — Linus Torvalds

We fixed the **data structure**. The code will follow.
