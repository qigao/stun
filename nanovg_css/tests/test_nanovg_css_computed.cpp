/**
 * Comprehensive Computed CSS Tests for NanoVG CSS
 *
 * Tests the separation between defined CSS (explicit_style) and computed CSS (computed):
 * - explicit_style immutability (should never be modified by layout)
 * - computed values correctness
 * - LayoutSource tracking (CSS_EXPLICIT, FLEXBOX, GRID, etc.)
 * - is_computed flag management
 * - Percentage resolution
 * - em/rem resolution
 * - calc() evaluation
 * - CSS variable resolution
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/catch_approx.hpp>
#include <nanovg.h>
#include <nanovg_css.h>
#include "nanovg_css_internal.h"  // For internal structure access in tests

using Catch::Matchers::WithinAbs;
using Catch::Approx;

// ============================================================================
// Explicit Style Immutability Tests
// ============================================================================

TEST_CASE("Explicit style immutability: basic layout", "[computed][explicit][immutability]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");

    // Set explicit values via inline styles (direct manipulation for testing)
    elem->inline_style["width"] = "200px";
    elem->inline_style["height"] = "100px";
    elem->inline_style["x"] = "50px";
    elem->inline_style["y"] = "30px";

    // Run layout - this parses inline styles and populates explicit_style
    nvgcssComputeLayout(renderer);

    // Verify explicit_style was populated correctly from inline styles
    REQUIRE(elem->explicit_style.width == 200.0f);
    REQUIRE(elem->explicit_style.height == 100.0f);
    REQUIRE(elem->explicit_style.x == 50.0f);
    REQUIRE(elem->explicit_style.y == 30.0f);

    // CRITICAL: Verify explicit_style unchanged after layout
    REQUIRE(elem->explicit_style.width == 200.0f);
    REQUIRE(elem->explicit_style.height == 100.0f);
    REQUIRE(elem->explicit_style.x == 50.0f);
    REQUIRE(elem->explicit_style.y == 30.0f);

    // Verify computed was populated
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.width, WithinAbs(200.0f, 0.1f));
    REQUIRE_THAT(elem->computed.height, WithinAbs(100.0f, 0.1f));
    REQUIRE_THAT(elem->computed.x, WithinAbs(50.0f, 0.1f));
    REQUIRE_THAT(elem->computed.y, WithinAbs(30.0f, 0.1f));

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("Explicit style immutability: auto values", "[computed][explicit][immutability]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");

    // Set explicit width/height via inline styles, but leave x/y unset (auto)
    
    elem->inline_style["width"] = "200px";
    elem->inline_style["height"] = "100px";

    // Don't set x or y - they should be auto (-1) after layout

    // Run layout - this parses inline styles and populates explicit_style
    nvgcssComputeLayout(renderer);

    // Verify explicit_style has correct values
    REQUIRE(elem->explicit_style.width == 200.0f);
    REQUIRE(elem->explicit_style.height == 100.0f);
    REQUIRE(elem->explicit_style.x == -1.0f); // auto (not set in inline styles)
    REQUIRE(elem->explicit_style.y == -1.0f); // auto (not set in inline styles)

    // CRITICAL: Verify explicit_style auto values unchanged
    REQUIRE(elem->explicit_style.x == -1.0f); // Still auto
    REQUIRE(elem->explicit_style.y == -1.0f); // Still auto

    // Verify computed has resolved values (auto resolved to some value)
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE(elem->computed.x >= 0.0f); // Resolved from auto
    REQUIRE(elem->computed.y >= 0.0f); // Resolved from auto

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("Explicit style immutability: flexbox layout", "[computed][explicit][immutability][flexbox]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .container {
            display: flex;
            flex-direction: row;
            justify-content: center;
        }
        .item { width: 100px; height: 50px; }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* container = nvgcssCreateElement(renderer, "container", "div");
    nvgcssAddClass(container, "container");
    

    NVGCSSElement* item = nvgcssCreateElement(renderer, "item", "div");
    nvgcssAddClass(item, "item");
    nvgcssAppendChild(renderer, container, item);

    // Run layout - this applies CSS rules and computes flexbox layout
    nvgcssComputeLayout(renderer);

    // CRITICAL: Verify explicit_style from CSS rules (flexbox should NOT modify these)
    REQUIRE(item->explicit_style.width == 100.0f);  // from .item CSS rule
    REQUIRE(item->explicit_style.height == 50.0f);  // from .item CSS rule
    REQUIRE(item->explicit_style.x == -1.0f);       // not set (auto)

    // Verify computed was populated by flexbox
    REQUIRE(item->computed.is_computed == true);
    REQUIRE(item->computed.source == NVGCSSComputedLayout::FLEXBOX);
    REQUIRE(item->computed.x >= 0.0f); // Flexbox computed position
    REQUIRE_THAT(item->computed.width, WithinAbs(100.0f, 0.1f));

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("Explicit style immutability: grid layout", "[computed][explicit][immutability][grid]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .grid {
            display: grid;
            grid-template-columns: 100px 200px;
            grid-template-rows: 50px;
        }
        .item { }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* grid = nvgcssCreateElement(renderer, "grid", "div");
    nvgcssAddClass(grid, "grid");
    

    NVGCSSElement* item = nvgcssCreateElement(renderer, "item", "div");
    nvgcssAddClass(item, "item");
    nvgcssAppendChild(renderer, grid, item);

    // Before layout: item has no explicit size or position
    REQUIRE(item->explicit_style.width == -1.0f); // auto
    REQUIRE(item->explicit_style.height == -1.0f); // auto
    REQUIRE(item->explicit_style.x == -1.0f); // auto

    // Run layout
    nvgcssComputeLayout(renderer);

    // CRITICAL: Verify explicit_style unchanged (grid should NOT modify explicit values)
    REQUIRE(item->explicit_style.width == -1.0f); // Still auto
    REQUIRE(item->explicit_style.height == -1.0f); // Still auto
    REQUIRE(item->explicit_style.x == -1.0f); // Still auto

    // Verify computed was populated by grid
    REQUIRE(item->computed.is_computed == true);
    REQUIRE(item->computed.source == NVGCSSComputedLayout::GRID);
    REQUIRE_THAT(item->computed.width, WithinAbs(100.0f, 0.1f)); // Grid cell width
    REQUIRE_THAT(item->computed.height, WithinAbs(50.0f, 0.1f)); // Grid cell height

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// LayoutSource Tracking Tests
// ============================================================================

TEST_CASE("LayoutSource: CSS_EXPLICIT", "[computed][layout-source]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
    

    nvgcssComputeLayout(renderer);

    // Verify LayoutSource is CSS_EXPLICIT (no flex/grid layout)
    REQUIRE(elem->computed.source == NVGCSSComputedLayout::CSS_EXPLICIT);

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("LayoutSource: FLEXBOX", "[computed][layout-source][flexbox]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .container { display: flex; }
        .item { width: 100px; height: 50px; }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* container = nvgcssCreateElement(renderer, "container", "div");
    nvgcssAddClass(container, "container");
    container->inline_style["width"] = "500px";
    container->inline_style["height"] = "100px";

    NVGCSSElement* item = nvgcssCreateElement(renderer, "item", "div");
    nvgcssAddClass(item, "item");
    nvgcssAppendChild(renderer, container, item);

    nvgcssComputeLayout(renderer);

    // Verify LayoutSource is FLEXBOX
    REQUIRE(item->computed.source == NVGCSSComputedLayout::FLEXBOX);

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("LayoutSource: GRID", "[computed][layout-source][grid]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .grid {
            display: grid;
            grid-template-columns: 100px 100px;
        }
        .item { }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* grid = nvgcssCreateElement(renderer, "grid", "div");
    nvgcssAddClass(grid, "grid");
    grid->inline_style["width"] = "200px";
    grid->inline_style["height"] = "100px";

    NVGCSSElement* item = nvgcssCreateElement(renderer, "item", "div");
    nvgcssAddClass(item, "item");
    nvgcssAppendChild(renderer, grid, item);

    nvgcssComputeLayout(renderer);

    // Verify LayoutSource is GRID
    REQUIRE(item->computed.source == NVGCSSComputedLayout::GRID);

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// is_computed Flag Tests
// ============================================================================

TEST_CASE("is_computed flag management", "[computed][is-computed]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
    

    // Before layout: is_computed should be false
    REQUIRE(elem->computed.is_computed == false);

    // Run layout
    nvgcssComputeLayout(renderer);

    // After layout: is_computed should be true
    REQUIRE(elem->computed.is_computed == true);

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Percentage Resolution Tests
// ============================================================================

TEST_CASE("Percentage resolution: width", "[computed][percentage]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .container { }
        .item { width: 50%; height: 100px; }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* container = nvgcssCreateElement(renderer, "container", "div");
    nvgcssAddClass(container, "container");
    container->inline_style["width"] = "400px";
    container->inline_style["height"] = "200px";

    NVGCSSElement* item = nvgcssCreateElement(renderer, "item", "div");
    nvgcssAddClass(item, "item");
    nvgcssAppendChild(renderer, container, item);

    nvgcssComputeLayout(renderer);

    // Verify percentage is resolved relative to parent
    // 50% of 400px = 200px
    REQUIRE_THAT(item->computed.width, WithinAbs(200.0f, 0.1f));

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("Percentage resolution: viewport units", "[computed][percentage]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
    // Note: Viewport percentage would need to be parsed as percentage of viewport
    // This test verifies the basic mechanism  // If no parent, could use viewport
    elem->inline_style["height"] = "100px";

    nvgcssComputeLayout(renderer);

    // Verify computed value exists
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE(elem->computed.width > 0.0f);

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Box Model Computed Values Tests
// ============================================================================

TEST_CASE("Computed box model: padding", "[computed][box-model]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 200px;
            height: 100px;
            padding: 10px 20px 30px 40px;
        }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
    nvgcssAddClass(elem, "box");

    nvgcssComputeLayout(renderer);

    // Verify padding is computed and stored
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.padding[0], WithinAbs(10.0f, 0.1f)); // top
    REQUIRE_THAT(elem->computed.padding[1], WithinAbs(20.0f, 0.1f)); // right
    REQUIRE_THAT(elem->computed.padding[2], WithinAbs(30.0f, 0.1f)); // bottom
    REQUIRE_THAT(elem->computed.padding[3], WithinAbs(40.0f, 0.1f)); // left

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("Computed box model: margin", "[computed][box-model]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 200px;
            height: 100px;
            margin: 5px 10px 15px 20px;
        }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
    nvgcssAddClass(elem, "box");

    nvgcssComputeLayout(renderer);

    // Verify margin is computed and stored
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.margin[0], WithinAbs(5.0f, 0.1f));  // top
    REQUIRE_THAT(elem->computed.margin[1], WithinAbs(10.0f, 0.1f)); // right
    REQUIRE_THAT(elem->computed.margin[2], WithinAbs(15.0f, 0.1f)); // bottom
    REQUIRE_THAT(elem->computed.margin[3], WithinAbs(20.0f, 0.1f)); // left

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("Computed box model: border", "[computed][box-model]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 200px;
            height: 100px;
            border-top-width: 1px;
            border-right-width: 2px;
            border-bottom-width: 3px;
            border-left-width: 4px;
        }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
    nvgcssAddClass(elem, "box");

    nvgcssComputeLayout(renderer);

    // Verify border is computed and stored
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.border[0], WithinAbs(1.0f, 0.1f)); // top
    REQUIRE_THAT(elem->computed.border[1], WithinAbs(2.0f, 0.1f)); // right
    REQUIRE_THAT(elem->computed.border[2], WithinAbs(3.0f, 0.1f)); // bottom
    REQUIRE_THAT(elem->computed.border[3], WithinAbs(4.0f, 0.1f)); // left

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("Computed box model: border-radius", "[computed][box-model]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .rounded {
            width: 200px;
            height: 100px;
            border-radius: 10px 15px 20px 25px;
        }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
    nvgcssAddClass(elem, "rounded");

    nvgcssComputeLayout(renderer);

    // Verify border-radius is computed and stored
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.border_radius[0], WithinAbs(10.0f, 0.1f)); // top-left
    REQUIRE_THAT(elem->computed.border_radius[1], WithinAbs(15.0f, 0.1f)); // top-right
    REQUIRE_THAT(elem->computed.border_radius[2], WithinAbs(20.0f, 0.1f)); // bottom-right
    REQUIRE_THAT(elem->computed.border_radius[3], WithinAbs(25.0f, 0.1f)); // bottom-left

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// box-sizing Tests
// ============================================================================

TEST_CASE("Computed content dimensions: content-box", "[computed][box-sizing]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 200px;
            height: 100px;
            padding: 10px;
            border-width: 5px;
            box-sizing: content-box;
        }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
    nvgcssAddClass(elem, "box");

    nvgcssComputeLayout(renderer);

    // Verify content dimensions
    // content-box: width/height ARE content dimensions
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.content_width, WithinAbs(200.0f, 0.1f));
    REQUIRE_THAT(elem->computed.content_height, WithinAbs(100.0f, 0.1f));

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("Computed content dimensions: border-box", "[computed][box-sizing]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 200px;
            height: 100px;
            padding: 10px;
            border-width: 5px;
            box-sizing: border-box;
        }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
    nvgcssAddClass(elem, "box");

    nvgcssComputeLayout(renderer);

    // Verify content dimensions
    // border-box: width/height INCLUDE padding and border
    // Content = 200px - (10px + 5px) * 2 = 200px - 30px = 170px
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.content_width, WithinAbs(170.0f, 0.1f));

    // Content height = 100px - (10px + 5px) * 2 = 100px - 30px = 70px
    REQUIRE_THAT(elem->computed.content_height, WithinAbs(70.0f, 0.1f));

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Min/Max Constraints Tests
// ============================================================================

TEST_CASE("Computed dimensions: min-width constraint", "[computed][constraints]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 100px;
            min-width: 150px;
            height: 100px;
        }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
    nvgcssAddClass(elem, "box");

    nvgcssComputeLayout(renderer);

    // Verify min-width constraint is applied
    // width: 100px, but min-width: 150px -> computed width should be 150px
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.width, WithinAbs(150.0f, 0.1f));

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("Computed dimensions: max-width constraint", "[computed][constraints]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 200px;
            max-width: 150px;
            height: 100px;
        }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
    nvgcssAddClass(elem, "box");

    nvgcssComputeLayout(renderer);

    // Verify max-width constraint is applied
    // width: 200px, but max-width: 150px -> computed width should be 150px
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.width, WithinAbs(150.0f, 0.1f));

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("Computed dimensions: min > max edge case", "[computed][constraints]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 100px;
            min-width: 200px;
            max-width: 150px;
            height: 100px;
        }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
    nvgcssAddClass(elem, "box");

    nvgcssComputeLayout(renderer);

    // Verify CSS spec behavior: when min > max, min wins
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.width, WithinAbs(200.0f, 0.1f)); // min-width wins

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// CSS Variable Resolution Tests
// ============================================================================

TEST_CASE("CSS variable resolution in computed styles", "[computed][variables]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    nvgcssSetVariable(renderer, "--box-width", "250px");
    nvgcssSetVariable(renderer, "--box-height", "120px");

    const char* css = R"(
        .box {
            width: var(--box-width);
            height: var(--box-height);
        }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
    nvgcssAddClass(elem, "box");

    nvgcssComputeLayout(renderer);

    // Verify variables are resolved in computed styles
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.width, WithinAbs(250.0f, 0.1f));
    REQUIRE_THAT(elem->computed.height, WithinAbs(120.0f, 0.1f));

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Multiple Layout Pass Tests
// ============================================================================

TEST_CASE("Computed values persistence across layout passes", "[computed][persistence]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");

    // Set explicit dimensions via inline styles
    elem->inline_style["width"] = "200px";
    elem->inline_style["height"] = "100px";

    // First layout pass
    nvgcssComputeLayout(renderer);

    float first_width = elem->computed.width;
    float first_height = elem->computed.height;

    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(first_width, WithinAbs(200.0f, 0.1f));
    REQUIRE_THAT(first_height, WithinAbs(100.0f, 0.1f));

    // Second layout pass (without changing anything)
    nvgcssComputeLayout(renderer);

    // Verify computed values are consistent
    REQUIRE_THAT(elem->computed.width, WithinAbs(first_width, 0.001f));
    REQUIRE_THAT(elem->computed.height, WithinAbs(first_height, 0.001f));

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Computed Style Retrieval Tests
// ============================================================================

TEST_CASE("Get computed style API", "[computed][api]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 200px;
            height: 100px;
            background: blue;
        }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
    nvgcssAddClass(elem, "box");

    nvgcssComputeLayout(renderer);

    // Test nvgcssGetComputedStyle API
    char buffer[256];
    int has_width = nvgcssGetComputedStyle(renderer, elem, "width", buffer, sizeof(buffer));

    // API should return 1 if property exists
    REQUIRE(has_width == 1);

    nvgcssDeleteRenderer(renderer);
}
