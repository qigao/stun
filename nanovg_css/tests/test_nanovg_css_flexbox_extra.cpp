#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/catch_approx.hpp>
#include <nanovg.h>
#include <nanovg_css.h>
#include "nanovg_css_internal.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("Flexbox case-insensitive properties", "[flexbox][layout][parsing]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .container {
            display: flex;
            Flex-Direction: Column;
            justify-content: Flex-Start;
            width: 200px;
            height: 500px;
        }
        .item { width: 100px; height: 50px; }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* container = nvgcssCreateElement(renderer, "container", "div");
    nvgcssAddClass(container, "container");

    NVGCSSElement* item1 = nvgcssCreateElement(renderer, "item1", "div");
    NVGCSSElement* item2 = nvgcssCreateElement(renderer, "item2", "div");

    nvgcssAddClass(item1, "item");
    nvgcssAddClass(item2, "item");

    nvgcssAppendChild(renderer, container, item1);
    nvgcssAppendChild(renderer, container, item2);

    nvgcssComputeLayout(renderer);

    // Verify column layout (stacked vertically)
    REQUIRE_THAT(item1->computed.y, WithinAbs(0.0f, 0.1f));
    REQUIRE_THAT(item2->computed.y, WithinAbs(50.0f, 0.1f));

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("Flexbox gap with row-reverse", "[flexbox][layout][gap][reverse]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .container {
            width: 400px;
            height: 100px;
            display: flex;
            flex-direction: row-reverse;
            gap: 20px;
        }
        .item { width: 100px; height: 50px; }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* container = nvgcssCreateElement(renderer, "container", "div");
    nvgcssAddClass(container, "container");

    NVGCSSElement* item1 = nvgcssCreateElement(renderer, "item1", "div");
    NVGCSSElement* item2 = nvgcssCreateElement(renderer, "item2", "div");
    NVGCSSElement* item3 = nvgcssCreateElement(renderer, "item3", "div");

    nvgcssAddClass(item1, "item");
    nvgcssAddClass(item2, "item");
    nvgcssAddClass(item3, "item");

    nvgcssAppendChild(renderer, container, item1);
    nvgcssAppendChild(renderer, container, item2);
    nvgcssAppendChild(renderer, container, item3);

    nvgcssComputeLayout(renderer);

    // Container 400px. Items 100px. Gap 20px.
    // Item 1: right-aligned -> x = 400 - 100 = 300
    // Item 2: x = 300 - 20 - 100 = 180
    // Item 3: x = 180 - 20 - 100 = 60
    REQUIRE_THAT(item1->computed.x, WithinAbs(300.0f, 0.1f));
    REQUIRE_THAT(item2->computed.x, WithinAbs(180.0f, 0.1f));
    REQUIRE_THAT(item3->computed.x, WithinAbs(60.0f, 0.1f));

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("Nested reverse layouts", "[flexbox][layout][nested][reverse]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        .outer {
            width: 200px;
            height: 400px;
            display: flex;
            flex-direction: column-reverse;
            padding: 0px;
            margin: 0px;
        }
        .inner {
            width: 200px;
            height: 100px;
            display: flex;
            flex-direction: row-reverse;
            padding: 0px;
            margin: 0px;
        }
        .item { width: 50px; height: 50px; }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* outer = nvgcssCreateElement(renderer, "outer", "div");
    nvgcssAddClass(outer, "outer");

    NVGCSSElement* inner1 = nvgcssCreateElement(renderer, "inner1", "div");
    nvgcssAddClass(inner1, "inner");

    NVGCSSElement* item1 = nvgcssCreateElement(renderer, "item1", "div");
    nvgcssAddClass(item1, "item");

    nvgcssAppendChild(renderer, outer, inner1);
    nvgcssAppendChild(renderer, inner1, item1);

    nvgcssComputeLayout(renderer);

    // Outer is column-reverse (bottom up).
    // inner1 should be at the bottom of outer.
    // outer height 400, inner height 100.
    // inner1 y = 400 - 100 = 300.
    REQUIRE_THAT(inner1->computed.y, WithinAbs(300.0f, 0.1f));

    // Inner is row-reverse (right to left).
    // item1 should be at right of inner.
    // inner width 200, item width 50.
    // item1 x relative to inner = 200 - 50 = 150.
    // item1 absolute x = outer.x (0) + inner.x (0) + 150 = 150.
    // item1 absolute y = inner1.y (300) + 0 = 300.
    
    REQUIRE_THAT(item1->computed.x, WithinAbs(150.0f, 0.1f));
    REQUIRE_THAT(item1->computed.y, WithinAbs(300.0f, 0.1f));

    nvgcssDeleteRenderer(renderer);
}

TEST_CASE("Flexbox inside Grid Cell", "[flexbox][grid][layout]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    const char* css = R"(
        #root {
            display: grid;
            grid-template-columns: 200px;
            grid-template-rows: 200px;
            width: 200px;
            height: 200px;
        }
        #sidebar {
            grid-column: 1;
            grid-row: 1;
            display: flex;
            Flex-Direction: Column;
            gap: 10px;
            padding: 10px;
        }
        .item { width: 50px; height: 50px; }
    )";

    nvgcssParseCSS(renderer, css);

    NVGCSSElement* root = nvgcssCreateElement(renderer, "root", "div");
    
    NVGCSSElement* sidebar = nvgcssCreateElement(renderer, "sidebar", "div");
    
    NVGCSSElement* item1 = nvgcssCreateElement(renderer, "item1", "div");
    NVGCSSElement* item2 = nvgcssCreateElement(renderer, "item2", "div");

    nvgcssAddClass(item1, "item");
    nvgcssAddClass(item2, "item");

    nvgcssAppendChild(renderer, root, sidebar);
    nvgcssAppendChild(renderer, sidebar, item1);
    nvgcssAppendChild(renderer, sidebar, item2);

    nvgcssComputeLayout(renderer);

    // Sidebar should fill grid cell (200x200).
    // Padding 10px.
    // Item 1: x = 10, y = 10.
    // Item 2: x = 10, y = 10 + 50 + 10 (gap) = 70.

    REQUIRE_THAT(sidebar->computed.width, WithinAbs(200.0f, 0.1f));
    
    REQUIRE_THAT(item1->computed.x, WithinAbs(10.0f, 0.1f));
    REQUIRE_THAT(item1->computed.y, WithinAbs(10.0f, 0.1f));

    REQUIRE_THAT(item2->computed.x, WithinAbs(10.0f, 0.1f));
    REQUIRE_THAT(item2->computed.y, WithinAbs(70.0f, 0.1f));

    nvgcssDeleteRenderer(renderer);
}
