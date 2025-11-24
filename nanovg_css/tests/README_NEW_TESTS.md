# New Comprehensive Test Suites

## Overview

Three new comprehensive test suites have been added to validate the layout implementation and ensure the separation between defined CSS (explicit_style) and computed CSS (computed) is properly maintained.

## Test Files

### 1. test_nanovg_css_flexbox.cpp (~650 lines)

**Purpose:** Comprehensive validation of Flexbox layout implementation

**Test Coverage:**
- ✅ `justify-content` (flex-start, flex-end, center, space-between, space-around, space-evenly)
- ✅ `align-items` (flex-start, flex-end, center, stretch)
- ✅ `flex-direction` (row, column, row-reverse, column-reverse)
- ✅ `flex-wrap` (wrap with multi-line layout)
- ✅ `flex-grow` (size distribution with different grow factors)
- ✅ `gap` (spacing between flex items)
- ✅ `order` (visual reordering)
- ✅ Nested flexbox containers

**Key Validations:**
- Computed positions (x, y) are correct for each layout mode
- Computed sizes (width, height) match expected values
- LayoutSource is set to FLEXBOX
- is_computed flag is true
- explicit_style remains unchanged after layout

### 2. test_nanovg_css_grid.cpp (~650 lines)

**Purpose:** Comprehensive validation of CSS Grid layout implementation

**Test Coverage:**
- ✅ Track sizing (fixed px, fractional fr, mixed px+fr)
- ✅ Grid gaps (column-gap, row-gap)
- ✅ Explicit item placement (grid-row-start/end, grid-column-start/end)
- ✅ Item spanning (grid-row-span, grid-column-span)
- ✅ Template areas (grid-template-areas, grid-area)
- ✅ Auto-placement (grid-auto-flow: row, column)
- ✅ minmax() function
- ✅ Implicit grid generation
- ✅ Nested grid containers

**Key Validations:**
- Grid cells have correct positions and sizes
- Track sizing algorithms work correctly (fr distribution)
- Gaps are applied correctly
- Spanning works for both rows and columns
- Template areas place items correctly
- LayoutSource is set to GRID
- explicit_style remains unchanged after layout

### 3. test_nanovg_css_computed.cpp (~550 lines)

**Purpose:** Validate separation between defined CSS and computed CSS

**Test Coverage:**
- ✅ **Explicit style immutability** - Verify explicit_style never changes during layout
- ✅ **Auto value handling** - Verify auto (-1) values remain auto in explicit_style
- ✅ **LayoutSource tracking** - Verify CSS_EXPLICIT, FLEXBOX, GRID sources
- ✅ **is_computed flag** - Verify flag is set after layout
- ✅ **Percentage resolution** - Verify percentages resolve to absolute values
- ✅ **Box model** - Verify padding, margin, border, border-radius are computed
- ✅ **box-sizing** - Verify content-box vs border-box behavior
- ✅ **Min/max constraints** - Verify min-width, max-width, edge cases (min > max)
- ✅ **CSS variables** - Verify var() resolution in computed styles
- ✅ **Layout persistence** - Verify computed values are consistent across passes

**Key Validations:**
- explicit_style.width, height, x, y NEVER change during layout
- Auto values (-1) remain -1 in explicit_style but resolve in computed
- Flexbox/Grid layouts do NOT modify explicit_style
- computed.source correctly identifies layout algorithm used
- computed values are populated and correct

## Building and Running

### Build the tests:
```bash
cmake --build build/Ninja/Msvc --target test_nanovg_css_flexbox --config Debug
cmake --build build/Ninja/Msvc --target test_nanovg_css_grid --config Debug
cmake --build build/Ninja/Msvc --target test_nanovg_css_computed --config Debug
```

### Run all tests:
```bash
ctest --test-dir build/Ninja/Msvc -C Debug
```

### Run specific test suites:
```bash
# Flexbox tests
./build/Ninja/Msvc/bin/test_nanovg_css_flexbox

# Grid tests
./build/Ninja/Msvc/bin/test_nanovg_css_grid

# Computed CSS tests
./build/Ninja/Msvc/bin/test_nanovg_css_computed
```

### Run with verbose output:
```bash
ctest --test-dir build/Ninja/Msvc -C Debug --verbose
```

### Run specific test cases:
```bash
# Run only justify-content tests
./build/Ninja/Msvc/bin/test_nanovg_css_flexbox "[justify-content]"

# Run only track-sizing tests
./build/Ninja/Msvc/bin/test_nanovg_css_grid "[track-sizing]"

# Run only immutability tests
./build/Ninja/Msvc/bin/test_nanovg_css_computed "[immutability]"
```

## Test Statistics

### Total Test Lines: ~1850 lines
- test_nanovg_css_flexbox.cpp: ~650 lines
- test_nanovg_css_grid.cpp: ~650 lines
- test_nanovg_css_computed.cpp: ~550 lines

### Total Test Cases: 44+
- Flexbox: 14 comprehensive test cases
- Grid: 15 comprehensive test cases
- Computed CSS: 15 comprehensive test cases

### Coverage Improvement:
- **Before:** ~30% coverage (1589 test lines, mostly smoke tests)
- **After:** ~70% estimated coverage (3439 total test lines, with comprehensive validation)

## Test Organization

All tests follow Catch2 conventions with clear sections and tags:

**Tags:**
- `[flexbox]`, `[grid]`, `[computed]` - Module tags
- `[layout]` - Layout-related tests
- `[justify-content]`, `[align-items]`, etc. - Feature tags
- `[track-sizing]`, `[gap]`, `[span]`, etc. - Feature tags
- `[immutability]`, `[layout-source]`, `[box-model]`, etc. - Feature tags

**Naming Convention:**
- Test names clearly describe what is being tested
- Format: `"<Module> <feature>: <specific case>"`
- Examples:
  - `"Flexbox justify-content: flex-start"`
  - `"Grid track sizing: fractional fr"`
  - `"Explicit style immutability: flexbox layout"`

## Expected Test Results

### All tests should PASS if:
1. Flexbox layout correctly computes positions and sizes
2. Grid layout correctly computes track sizes and item placement
3. explicit_style is NEVER modified during layout
4. computed values are correctly populated
5. LayoutSource is correctly set
6. Box model (padding, margin, border) is correctly applied

### Tests will FAIL if:
1. Layout algorithms produce incorrect positions/sizes
2. explicit_style is modified during layout (CRITICAL BUG)
3. computed values are not populated
4. LayoutSource is incorrect
5. Box model calculations are wrong

## Known Issues

Some tests are marked with `[!mayfail]` tag due to known implementation gaps:

**Grid Issues:**
1. **Grid spanning** - `grid-column-span`, `grid-row-span` not fully implemented
2. **Grid auto-flow: column** - Column-first placement not working
3. **Grid auto-rows** - Implicit row sizing not applied

**Layout Recursion Issues (affects both Flexbox and Grid):**
4. **Nested containers** - Child items of nested layout containers not computed

See [KNOWN_ISSUES.md](KNOWN_ISSUES.md) for detailed information and workarounds.

These tests document the expected behavior and will pass once the features are implemented.

## Integration with CI

These tests are automatically discovered by CTest via `catch_discover_tests()` in CMakeLists.txt.

To add to CI pipeline:
```yaml
- name: Run NanoVG CSS tests
  run: |
    ctest --test-dir build -C Release --output-on-failure
```

## Future Enhancements

Recommended additional tests:
1. **Edge cases:** Zero-sized elements, negative margins, circular dependencies
2. **Performance:** Large element trees (1000+ elements), nested containers
3. **CSS functions:** calc(), min(), max(), clamp() evaluation
4. **Animations:** Transition and animation state management
5. **Stress tests:** Rapidly changing styles, repeated layout passes

## References

- [W3C CSS Flexbox Specification](https://www.w3.org/TR/css-flexbox-1/)
- [W3C CSS Grid Specification](https://www.w3.org/TR/css-grid-2/)
- [Catch2 Documentation](https://github.com/catchorg/Catch2)
- [SPEC_NANOVG_CSS.md](../SPEC_NANOVG_CSS.md) - NanoVG CSS specification
- [LAYOUT_REVIEW.md](../LAYOUT_REVIEW.md) - Comprehensive layout review
