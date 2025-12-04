/**
 * Comprehensive Computed CSS Tests for NanoVG CSS
 *
 * Tests the separation between CSS style (element->style) and layout results (element->layout):
 * - style immutability (should never be modified by layout)
 * - layout values correctness
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
#include <cssbox.h>
#include "cssbox_internal.h"  // For internal structure access in tests

using Catch::Matchers::WithinAbs;
using Catch::Approx;

// ============================================================================
// Explicit Style Immutability Tests
// ============================================================================

TEST_CASE("Explicit style immutability: basic layout", "[computed][explicit][immutability]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");

    // Set explicit values via inline styles (direct manipulation for testing)
    elem->inline_style["width"] = "200px";
    elem->inline_style["height"] = "100px";
    elem->inline_style["x"] = "50px";
    elem->inline_style["y"] = "30px";

    // Run layout - this parses inline styles and populates style + layout
    cssboxComputeLayout(renderer);

    // NEW: Verify typed style was populated correctly
    REQUIRE_THAT(elem->style.width.resolve(800, 16, 800), WithinAbs(200.0f, 0.1f));
    REQUIRE_THAT(elem->style.height.resolve(600, 16, 600), WithinAbs(100.0f, 0.1f));
    REQUIRE_THAT(elem->style.left.resolve(800, 16, 800), WithinAbs(50.0f, 0.1f));
    REQUIRE_THAT(elem->style.top.resolve(600, 16, 600), WithinAbs(30.0f, 0.1f));

    // NEW: Verify layout was populated (source of truth for rendering)
    REQUIRE(elem->layout.source != cssbox::ResolvedLayout::Source::UNCOMPUTED);
    REQUIRE_THAT(elem->layout.width, WithinAbs(200.0f, 0.1f));
    REQUIRE_THAT(elem->layout.height, WithinAbs(100.0f, 0.1f));
    REQUIRE_THAT(elem->layout.x, WithinAbs(50.0f, 0.1f));
    REQUIRE_THAT(elem->layout.y, WithinAbs(30.0f, 0.1f));

    // DEPRECATED: Also verify computed for backward compatibility
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.width, WithinAbs(200.0f, 0.1f));
    REQUIRE_THAT(elem->computed.height, WithinAbs(100.0f, 0.1f));
    REQUIRE_THAT(elem->computed.x, WithinAbs(50.0f, 0.1f));
    REQUIRE_THAT(elem->computed.y, WithinAbs(30.0f, 0.1f));

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Explicit style immutability: auto values", "[computed][explicit][immutability]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");

    // Set explicit width/height via inline styles, but leave x/y unset (auto)
    elem->inline_style["width"] = "200px";
    elem->inline_style["height"] = "100px";
    // Don't set x or y - they should be auto after layout

    // Run layout - this parses inline styles and populates style + layout
    cssboxComputeLayout(renderer);

    // NEW: Verify typed style has correct values
    REQUIRE_THAT(elem->style.width.resolve(800, 16, 800), WithinAbs(200.0f, 0.1f));
    REQUIRE_THAT(elem->style.height.resolve(600, 16, 600), WithinAbs(100.0f, 0.1f));
    REQUIRE(elem->style.left.is_auto()); // auto (not set in inline styles)
    REQUIRE(elem->style.top.is_auto());  // auto (not set in inline styles)

    // NEW: Verify layout has resolved values
    REQUIRE(elem->layout.source != cssbox::ResolvedLayout::Source::UNCOMPUTED);
    REQUIRE(elem->layout.x >= 0.0f); // Resolved from auto
    REQUIRE(elem->layout.y >= 0.0f); // Resolved from auto

    // DEPRECATED: Verify computed for backward compatibility
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE(elem->computed.x >= 0.0f); // Resolved from auto
    REQUIRE(elem->computed.y >= 0.0f); // Resolved from auto

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Explicit style immutability: flexbox layout", "[computed][explicit][immutability][flexbox]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        .container {
            display: flex;
            flex-direction: row;
            justify-content: center;
        }
        .item { width: 100px; height: 50px; }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
    cssboxAddClass(container, "container");

    cssboxElement* item = cssboxCreateElement(renderer, "item", "div");
    cssboxAddClass(item, "item");
    cssboxAppendChild(renderer, container, item);

    // Run layout - this applies CSS rules and computes flexbox layout
    cssboxComputeLayout(renderer);

    // NEW: Verify typed style from CSS rules (flexbox should NOT modify these)
    REQUIRE_THAT(item->style.width.resolve(800, 16, 800), WithinAbs(100.0f, 0.1f));
    REQUIRE_THAT(item->style.height.resolve(600, 16, 600), WithinAbs(50.0f, 0.1f));
    REQUIRE(item->style.left.is_auto()); // not set (auto)

    // NEW: Verify layout was populated by flexbox
    REQUIRE(item->layout.source == cssbox::ResolvedLayout::Source::FLEXBOX);
    REQUIRE(item->layout.x >= 0.0f); // Flexbox computed position
    REQUIRE_THAT(item->layout.width, WithinAbs(100.0f, 0.1f));

    // DEPRECATED: Also verify computed for backward compatibility
    REQUIRE(item->computed.is_computed == true);
    REQUIRE(item->computed.source == cssboxComputedLayout::FLEXBOX);
    REQUIRE(item->computed.x >= 0.0f);
    REQUIRE_THAT(item->computed.width, WithinAbs(100.0f, 0.1f));

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Explicit style immutability: grid layout", "[computed][explicit][immutability][grid]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        .grid {
            display: grid;
            grid-template-columns: 100px 200px;
            grid-template-rows: 50px;
        }
        .item { }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* grid = cssboxCreateElement(renderer, "grid", "div");
    cssboxAddClass(grid, "grid");

    cssboxElement* item = cssboxCreateElement(renderer, "item", "div");
    cssboxAddClass(item, "item");
    cssboxAppendChild(renderer, grid, item);

    // Before layout: item has auto size (no explicit CSS set)
    REQUIRE(item->style.width.is_auto());
    REQUIRE(item->style.height.is_auto());
    REQUIRE(item->style.left.is_auto());

    // Run layout
    cssboxComputeLayout(renderer);

    // NEW: Verify typed style unchanged (grid should NOT modify style values)
    REQUIRE(item->style.width.is_auto());  // Still auto
    REQUIRE(item->style.height.is_auto()); // Still auto
    REQUIRE(item->style.left.is_auto());   // Still auto

    // NEW: Verify layout was populated by grid
    REQUIRE(item->layout.source == cssbox::ResolvedLayout::Source::GRID);
    REQUIRE_THAT(item->layout.width, WithinAbs(100.0f, 0.1f));  // Grid cell width
    REQUIRE_THAT(item->layout.height, WithinAbs(50.0f, 0.1f)); // Grid cell height

    // DEPRECATED: Also verify computed for backward compatibility
    REQUIRE(item->computed.is_computed == true);
    REQUIRE(item->computed.source == cssboxComputedLayout::GRID);
    REQUIRE_THAT(item->computed.width, WithinAbs(100.0f, 0.1f));
    REQUIRE_THAT(item->computed.height, WithinAbs(50.0f, 0.1f));

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// LayoutSource Tracking Tests
// ============================================================================

TEST_CASE("LayoutSource: CSS_EXPLICIT", "[computed][layout-source]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
    

    cssboxComputeLayout(renderer);

    // Verify LayoutSource is CSS_EXPLICIT (no flex/grid layout)
    REQUIRE(elem->computed.source == cssboxComputedLayout::CSS_EXPLICIT);

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("LayoutSource: FLEXBOX", "[computed][layout-source][flexbox]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        .container { display: flex; }
        .item { width: 100px; height: 50px; }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
    cssboxAddClass(container, "container");
    container->inline_style["width"] = "500px";
    container->inline_style["height"] = "100px";

    cssboxElement* item = cssboxCreateElement(renderer, "item", "div");
    cssboxAddClass(item, "item");
    cssboxAppendChild(renderer, container, item);

    cssboxComputeLayout(renderer);

    // Verify LayoutSource is FLEXBOX
    REQUIRE(item->computed.source == cssboxComputedLayout::FLEXBOX);

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("LayoutSource: GRID", "[computed][layout-source][grid]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        .grid {
            display: grid;
            grid-template-columns: 100px 100px;
        }
        .item { }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* grid = cssboxCreateElement(renderer, "grid", "div");
    cssboxAddClass(grid, "grid");
    grid->inline_style["width"] = "200px";
    grid->inline_style["height"] = "100px";

    cssboxElement* item = cssboxCreateElement(renderer, "item", "div");
    cssboxAddClass(item, "item");
    cssboxAppendChild(renderer, grid, item);

    cssboxComputeLayout(renderer);

    // Verify LayoutSource is GRID
    REQUIRE(item->computed.source == cssboxComputedLayout::GRID);

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// is_computed Flag Tests
// ============================================================================

TEST_CASE("is_computed flag management", "[computed][is-computed]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
    

    // Before layout: is_computed should be false
    REQUIRE(elem->computed.is_computed == false);

    // Run layout
    cssboxComputeLayout(renderer);

    // After layout: is_computed should be true
    REQUIRE(elem->computed.is_computed == true);

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Percentage Resolution Tests
// ============================================================================

TEST_CASE("Percentage resolution: width", "[computed][percentage]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        .container { }
        .item { width: 50%; height: 100px; }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
    cssboxAddClass(container, "container");
    container->inline_style["width"] = "400px";
    container->inline_style["height"] = "200px";

    cssboxElement* item = cssboxCreateElement(renderer, "item", "div");
    cssboxAddClass(item, "item");
    cssboxAppendChild(renderer, container, item);

    cssboxComputeLayout(renderer);

    // Verify percentage is resolved relative to parent
    // 50% of 400px = 200px
    REQUIRE_THAT(item->computed.width, WithinAbs(200.0f, 0.1f));

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Percentage resolution: viewport units", "[computed][percentage]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
    // Note: Viewport percentage would need to be parsed as percentage of viewport
    // This test verifies the basic mechanism  // If no parent, could use viewport
    elem->inline_style["height"] = "100px";

    cssboxComputeLayout(renderer);

    // Verify computed value exists
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE(elem->computed.width > 0.0f);

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Box Model Computed Values Tests
// ============================================================================

TEST_CASE("Computed box model: padding", "[computed][box-model]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 200px;
            height: 100px;
            padding: 10px 20px 30px 40px;
        }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
    cssboxAddClass(elem, "box");

    cssboxComputeLayout(renderer);

    // Verify padding is computed and stored
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.padding[0], WithinAbs(10.0f, 0.1f)); // top
    REQUIRE_THAT(elem->computed.padding[1], WithinAbs(20.0f, 0.1f)); // right
    REQUIRE_THAT(elem->computed.padding[2], WithinAbs(30.0f, 0.1f)); // bottom
    REQUIRE_THAT(elem->computed.padding[3], WithinAbs(40.0f, 0.1f)); // left

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Computed box model: margin", "[computed][box-model]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 200px;
            height: 100px;
            margin: 5px 10px 15px 20px;
        }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
    cssboxAddClass(elem, "box");

    cssboxComputeLayout(renderer);

    // Verify margin is computed and stored
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.margin[0], WithinAbs(5.0f, 0.1f));  // top
    REQUIRE_THAT(elem->computed.margin[1], WithinAbs(10.0f, 0.1f)); // right
    REQUIRE_THAT(elem->computed.margin[2], WithinAbs(15.0f, 0.1f)); // bottom
    REQUIRE_THAT(elem->computed.margin[3], WithinAbs(20.0f, 0.1f)); // left

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Computed box model: border", "[computed][box-model]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

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

    cssboxParseCSS(renderer, css);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
    cssboxAddClass(elem, "box");

    cssboxComputeLayout(renderer);

    // Verify border is computed and stored
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.border[0], WithinAbs(1.0f, 0.1f)); // top
    REQUIRE_THAT(elem->computed.border[1], WithinAbs(2.0f, 0.1f)); // right
    REQUIRE_THAT(elem->computed.border[2], WithinAbs(3.0f, 0.1f)); // bottom
    REQUIRE_THAT(elem->computed.border[3], WithinAbs(4.0f, 0.1f)); // left

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Computed box model: border-radius", "[computed][box-model]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        .rounded {
            width: 200px;
            height: 100px;
            border-radius: 10px 15px 20px 25px;
        }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
    cssboxAddClass(elem, "rounded");

    cssboxComputeLayout(renderer);

    // Verify border-radius is computed and stored
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.border_radius[0], WithinAbs(10.0f, 0.1f)); // top-left
    REQUIRE_THAT(elem->computed.border_radius[1], WithinAbs(15.0f, 0.1f)); // top-right
    REQUIRE_THAT(elem->computed.border_radius[2], WithinAbs(20.0f, 0.1f)); // bottom-right
    REQUIRE_THAT(elem->computed.border_radius[3], WithinAbs(25.0f, 0.1f)); // bottom-left

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// box-sizing Tests
// ============================================================================

TEST_CASE("Computed content dimensions: content-box", "[computed][box-sizing]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 200px;
            height: 100px;
            padding: 10px;
            border-width: 5px;
            box-sizing: content-box;
        }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
    cssboxAddClass(elem, "box");

    cssboxComputeLayout(renderer);

    // Verify content dimensions
    // content-box: width/height ARE content dimensions
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.content_width, WithinAbs(200.0f, 0.1f));
    REQUIRE_THAT(elem->computed.content_height, WithinAbs(100.0f, 0.1f));

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Computed content dimensions: border-box", "[computed][box-sizing]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 200px;
            height: 100px;
            padding: 10px;
            border-width: 5px;
            box-sizing: border-box;
        }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
    cssboxAddClass(elem, "box");

    cssboxComputeLayout(renderer);

    // Verify content dimensions
    // border-box: width/height INCLUDE padding and border
    // Content = 200px - (10px + 5px) * 2 = 200px - 30px = 170px
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.content_width, WithinAbs(170.0f, 0.1f));

    // Content height = 100px - (10px + 5px) * 2 = 100px - 30px = 70px
    REQUIRE_THAT(elem->computed.content_height, WithinAbs(70.0f, 0.1f));

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Min/Max Constraints Tests
// ============================================================================

TEST_CASE("Computed dimensions: min-width constraint", "[computed][constraints]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 100px;
            min-width: 150px;
            height: 100px;
        }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
    cssboxAddClass(elem, "box");

    cssboxComputeLayout(renderer);

    // Verify min-width constraint is applied
    // width: 100px, but min-width: 150px -> computed width should be 150px
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.width, WithinAbs(150.0f, 0.1f));

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Computed dimensions: max-width constraint", "[computed][constraints]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 200px;
            max-width: 150px;
            height: 100px;
        }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
    cssboxAddClass(elem, "box");

    cssboxComputeLayout(renderer);

    // Verify max-width constraint is applied
    // width: 200px, but max-width: 150px -> computed width should be 150px
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.width, WithinAbs(150.0f, 0.1f));

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Computed dimensions: min > max edge case", "[computed][constraints]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 100px;
            min-width: 200px;
            max-width: 150px;
            height: 100px;
        }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
    cssboxAddClass(elem, "box");

    cssboxComputeLayout(renderer);

    // Verify CSS spec behavior: when min > max, min wins
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.width, WithinAbs(200.0f, 0.1f)); // min-width wins

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// CSS Variable Resolution Tests
// ============================================================================

TEST_CASE("CSS variable resolution in computed styles", "[computed][variables]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    cssboxSetVariable(renderer, "--box-width", "250px");
    cssboxSetVariable(renderer, "--box-height", "120px");

    const char* css = R"(
        .box {
            width: var(--box-width);
            height: var(--box-height);
        }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
    cssboxAddClass(elem, "box");

    cssboxComputeLayout(renderer);

    // Verify variables are resolved in computed styles
    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(elem->computed.width, WithinAbs(250.0f, 0.1f));
    REQUIRE_THAT(elem->computed.height, WithinAbs(120.0f, 0.1f));

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Multiple Layout Pass Tests
// ============================================================================

TEST_CASE("Computed values persistence across layout passes", "[computed][persistence]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");

    // Set explicit dimensions via inline styles
    elem->inline_style["width"] = "200px";
    elem->inline_style["height"] = "100px";

    // First layout pass
    cssboxComputeLayout(renderer);

    float first_width = elem->computed.width;
    float first_height = elem->computed.height;

    REQUIRE(elem->computed.is_computed == true);
    REQUIRE_THAT(first_width, WithinAbs(200.0f, 0.1f));
    REQUIRE_THAT(first_height, WithinAbs(100.0f, 0.1f));

    // Second layout pass (without changing anything)
    cssboxComputeLayout(renderer);

    // Verify computed values are consistent
    REQUIRE_THAT(elem->computed.width, WithinAbs(first_width, 0.001f));
    REQUIRE_THAT(elem->computed.height, WithinAbs(first_height, 0.001f));

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Computed Style Retrieval Tests
// ============================================================================

TEST_CASE("Get computed style API", "[computed][api]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        .box {
            width: 200px;
            height: 100px;
            background: blue;
        }
    )";

    cssboxParseCSS(renderer, css);

    cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
    cssboxAddClass(elem, "box");

    cssboxComputeLayout(renderer);

    // Test cssboxGetComputedStyle API
    char buffer[256];
    int has_width = cssboxGetComputedStyle(renderer, elem, "width", buffer, sizeof(buffer));

    // API should return 1 if property exists
    REQUIRE(has_width == 1);

    cssboxDeleteRenderer(renderer);
}
