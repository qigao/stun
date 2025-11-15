# NanoVG CSS Test Suite

This directory contains comprehensive tests for the nanovg_css module.

## Test Types

### 1. Unit Tests (Automated)

**`test_nanovg_css_comprehensive.cpp`**
- **95 assertions** across **17 test categories**
- Tests core API functionality without requiring OpenGL context
- Covers: CSS parsing, element management, colors, lengths, tree manipulation, etc.
- **Run with**: `ctest -R test_nanovg_css_comprehensive`

**`test_nanovg_css_advanced.cpp`**
- **150+ assertions** across **19 test categories**
- Tests advanced CSS features
- Covers: transforms, z-index, opacity, positioning, display modes, overflow, borders, shadows, etc.
- **Run with**: `ctest -R test_nanovg_css_advanced`

Both use **Catch2** framework and can run in CI/CD without a display.

### 2. Visual Tests (Interactive)

**`test_visual_renderer`** - Interactive visual validation
- Creates an actual window with OpenGL rendering
- Displays 9 test cases in a grid layout:
  1. Basic box with background color
  2. Border styles
  3. Text rendering
  4. Button (normal state)
  5. Button (hover state)
  6. Opacity effects
  7. Rounded corners
  8. Box shadows
  9. Gradient backgrounds
- **Purpose**: Manual visual verification of rendering correctness
- **Run with**: `./bin/test_visual_renderer`
- **Controls**: Press ESC to exit

### 3. Snapshot/Screenshot Tests (Automated Visual Regression)

**`test_snapshot_renderer`** - Automated screenshot comparison
- Renders test cases to offscreen buffers
- Saves BMP screenshots for comparison
- **Two modes**:
  - **Generate mode**: Creates baseline screenshots in `snapshots/`
  - **Verify mode**: Compares current renders against `golden/` images
- **Purpose**: Catch visual regressions in CI/CD
- **Usage**:
  ```bash
  # Generate baseline screenshots
  ./bin/test_snapshot_renderer

  # After code changes, verify against baseline
  cp snapshots/*.bmp golden/  # First time only
  ./bin/test_snapshot_renderer --verify
  ```
- **Exit code**: 0 if all tests pass, 1 if any fail (CI/CD friendly)

## Building Tests

```bash
# Build all tests
cmake --build build --target test_nanovg_css_comprehensive
cmake --build build --target test_nanovg_css_advanced
cmake --build build --target test_visual_renderer

# Or build everything
cmake --build build
```

## Running Tests

```bash
# Run automated unit tests only
ctest -R nanovg_css

# Run specific test suite
ctest -R test_nanovg_css_comprehensive --verbose

# Run visual test (opens window)
./build/bin/test_visual_renderer
```

## Test Coverage Summary

| Category | Unit Tests | Visual Tests |
|----------|-----------|--------------|
| CSS Parsing | ✓ | - |
| Color Parsing | ✓ | ✓ |
| Length Parsing | ✓ | - |
| Element Management | ✓ | - |
| Tree Manipulation | ✓ | - |
| Pseudo-States | ✓ | ✓ |
| Transforms | ✓ | - |
| Z-Index | ✓ | - |
| Opacity | ✓ | ✓ |
| Positioning | ✓ | - |
| Display Modes | ✓ | - |
| Overflow | ✓ | - |
| Borders | ✓ | ✓ |
| Border-Radius | ✓ | ✓ |
| Box Shadows | ✓ | ✓ |
| Gradients | ✓ | ✓ |
| Text Rendering | ✓ | ✓ |
| Button States | - | ✓ |

## Adding New Tests

### Adding Unit Tests

Edit `test_nanovg_css_comprehensive.cpp` or `test_nanovg_css_advanced.cpp`:

```cpp
TEST_CASE("Your Test Name", "[tag]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);

    SECTION("Test section") {
        // Your test code
        REQUIRE(condition);
    }

    nvgcssDeleteRenderer(renderer);
}
```

### Adding Visual Tests

Edit `test_visual_renderer.cpp` in the `register_tests()` method:

```cpp
tests.push_back({
    "Test Name",
    ".css-class { property: value; }",
    [](NVGCSSRenderer* r, float x, float y, float w, float h) {
        NVGCSSElement* elem = nvgcssCreateElement(r, "id", "type");
        // Setup element...
    }
});
```

## Continuous Integration

The unit tests (`test_nanovg_css_comprehensive` and `test_nanovg_css_advanced`) can run in CI/CD pipelines without a display since they use nullptr for NVGcontext.

```yaml
# Example GitHub Actions
- name: Run NanoVG CSS Tests
  run: |
    cd build
    ctest -R nanovg_css --output-on-failure
```

## Future Test Enhancements

Potential additions:
- **Screenshot comparison tests** - Render to offscreen buffer, compare against golden images
- **Performance benchmarks** - Measure CSS parsing and layout computation speed
- **Stress tests** - Large element counts, deep nesting, complex CSS
- **Fuzzing** - Random CSS input testing for crashes
- **Memory leak tests** - Valgrind/sanitizer integration

## Dependencies

- **Unit tests**: Catch2 v3
- **Visual tests**: SDL3, OpenGL, glad, NanoVG
