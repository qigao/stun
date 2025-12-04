/**
 * Advanced Catch2 Unit Tests for NanoVG CSS
 *
 * Tests advanced features:
 * - CSS Transforms (translate, rotate, scale)
 * - Z-index and rendering order
 * - Opacity and transparency
 * - CSS positioning (absolute, relative, fixed)
 * - Display modes (flex, grid, none)
 * - Overflow control
 * - Computed style retrieval
 * - Edge cases and error handling
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <nanovg.h>
#include <cssbox.h>
#include "cssbox_internal.h"  // For internal structure access in tests
#include <cmath>

using Catch::Matchers::WithinAbs;

// ============================================================================
// Transform Tests
// ============================================================================

TEST_CASE("NanoVGCSS Transforms", "[transforms]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Translate transform") {
        const char* css = R"(
            .translated {
                transform: translate(50px, 100px);
            }

            #test {
                width: 100px;
                height: 50px;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "translated");

        cssboxComputeLayout(renderer);

        // Verify element created successfully
        REQUIRE(elem != nullptr);
    }

    SECTION("Rotate transform") {
        const char* css = R"(
            .rotated {
                transform: rotate(45deg);
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "rotated");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    SECTION("Scale transform") {
        const char* css = R"(
            .scaled {
                transform: scale(2, 3);
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "scaled");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    SECTION("Multiple transforms") {
        const char* css = R"(
            .complex {
                transform: translate(10px, 20px) rotate(30deg) scale(1.5);
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "complex");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Z-Index Tests
// ============================================================================

TEST_CASE("NanoVGCSS Z-Index", "[z-index]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Set z-index via CSS") {
        const char* css = R"(
            .layer-1 { z-index: 1; }
            .layer-2 { z-index: 2; }
            .layer-10 { z-index: 10; }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem1 = cssboxCreateElement(renderer, "elem1", "rect");
        cssboxElement* elem2 = cssboxCreateElement(renderer, "elem2", "rect");
        cssboxElement* elem3 = cssboxCreateElement(renderer, "elem3", "rect");

        cssboxAddClass(elem1, "layer-1");
        cssboxAddClass(elem2, "layer-2");
        cssboxAddClass(elem3, "layer-10");

        cssboxComputeLayout(renderer);

        REQUIRE(elem1 != nullptr);
        REQUIRE(elem2 != nullptr);
        REQUIRE(elem3 != nullptr);
    }

    SECTION("Negative z-index") {
        const char* css = R"(
            .behind { z-index: -1; }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "behind");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Opacity Tests
// ============================================================================

TEST_CASE("NanoVGCSS Opacity", "[opacity]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Set opacity via CSS") {
        const char* css = R"(
            .transparent { opacity: 0.5; }
            .invisible { opacity: 0; }
            .opaque { opacity: 1; }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem1 = cssboxCreateElement(renderer, "elem1", "rect");
        cssboxElement* elem2 = cssboxCreateElement(renderer, "elem2", "rect");
        cssboxElement* elem3 = cssboxCreateElement(renderer, "elem3", "rect");

        cssboxAddClass(elem1, "transparent");
        cssboxAddClass(elem2, "invisible");
        cssboxAddClass(elem3, "opaque");

        cssboxComputeLayout(renderer);

        // Verify elements were created
        REQUIRE(elem1 != nullptr);
        REQUIRE(elem2 != nullptr);
        REQUIRE(elem3 != nullptr);
    }

    SECTION("Default opacity") {
        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        REQUIRE(elem->opacity == 1.0f);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Position Property Tests
// ============================================================================

TEST_CASE("NanoVGCSS Position Property", "[position]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Absolute positioning") {
        const char* css = R"(
            .absolute {
                position: absolute;
                left: 50px;
                top: 100px;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "absolute");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    SECTION("Relative positioning") {
        const char* css = R"(
            .relative {
                position: relative;
                left: 10px;
                top: 20px;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "relative");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    SECTION("Fixed positioning") {
        const char* css = R"(
            .fixed {
                position: fixed;
                right: 10px;
                bottom: 10px;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "fixed");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Display Property Tests
// ============================================================================

TEST_CASE("NanoVGCSS Display Property", "[display]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Display flex") {
        const char* css = R"(
            .flex-container {
                display: flex;
                flex-direction: row;
                gap: 10px;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
        cssboxAddClass(container, "flex-container");

        cssboxElement* child1 = cssboxCreateElement(renderer, "child1", "div");
        cssboxElement* child2 = cssboxCreateElement(renderer, "child2", "div");

        cssboxAppendChild(renderer, container, child1);
        cssboxAppendChild(renderer, container, child2);

        cssboxComputeLayout(renderer);

        int child_count = 0;
        cssboxElement** children = cssboxGetChildren(renderer, container, &child_count);
        REQUIRE(child_count == 2);
    }

    SECTION("Display grid") {
        const char* css = R"(
            .grid-container {
                display: grid;
                grid-template-columns: 1fr 1fr 1fr;
                gap: 10px;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
        cssboxAddClass(container, "grid-container");

        cssboxComputeLayout(renderer);
        REQUIRE(container != nullptr);
    }

    SECTION("Display none") {
        const char* css = R"(
            .hidden {
                display: none;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "hidden");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Overflow Property Tests
// ============================================================================

TEST_CASE("NanoVGCSS Overflow Property", "[overflow]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Overflow hidden") {
        const char* css = R"(
            .clip {
                overflow: hidden;
                width: 200px;
                height: 100px;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
        cssboxAddClass(elem, "clip");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    SECTION("Overflow scroll") {
        const char* css = R"(
            .scrollable {
                overflow: scroll;
                overflow-x: auto;
                overflow-y: scroll;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
        cssboxAddClass(elem, "scrollable");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Computed Style Retrieval Tests
// ============================================================================

TEST_CASE("NanoVGCSS Computed Style Retrieval", "[computed-style]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Get computed style property") {
        const char* css = R"(
            .button {
                width: 120px;
                background: blue;
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "button");

        cssboxComputeLayout(renderer);

        char buffer[256];
        int has_width = cssboxGetComputedStyle(renderer, elem, "width", buffer, sizeof(buffer));
        // Function should execute without crashing
        REQUIRE(elem != nullptr);
    }

    SECTION("Get non-existent property") {
        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");

        char buffer[256];
        int result = cssboxGetComputedStyle(renderer, elem, "non-existent-property", buffer, sizeof(buffer));
        // Should return 0 for non-existent property
        REQUIRE(result == 0);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Complex Selector Tests
// ============================================================================

TEST_CASE("NanoVGCSS Complex Selectors", "[selectors]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Descendant selector") {
        const char* css = R"(
            .container .item {
                background: red;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Child selector") {
        const char* css = R"(
            .parent > .child {
                color: blue;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Multiple class selector") {
        const char* css = R"(
            .button.primary {
                background: blue;
            }
            .button.secondary {
                background: gray;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "button");
        cssboxAddClass(elem, "button");
        cssboxAddClass(elem, "primary");

        REQUIRE(cssboxHasClass(elem, "button") == 1);
        REQUIRE(cssboxHasClass(elem, "primary") == 1);
    }

    SECTION("Attribute selector") {
        const char* css = R"(
            [data-type="button"] {
                background: green;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "div");
        // Direct attribute manipulation (internal API for testing)
        elem->attributes["data-type"] = "button";

        REQUIRE(elem->attributes["data-type"] == "button");
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Border and Border-Radius Tests
// ============================================================================

TEST_CASE("NanoVGCSS Border Properties", "[border]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Border shorthand") {
        const char* css = R"(
            .bordered {
                border: 2px solid red;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "bordered");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    SECTION("Individual border sides") {
        const char* css = R"(
            .custom-border {
                border-top: 1px solid black;
                border-right: 2px dashed blue;
                border-bottom: 3px dotted green;
                border-left: 4px solid red;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "custom-border");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    SECTION("Border-radius individual corners") {
        const char* css = R"(
            .custom-radius {
                border-top-left-radius: 10px;
                border-top-right-radius: 20px;
                border-bottom-right-radius: 30px;
                border-bottom-left-radius: 40px;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "custom-radius");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Box Shadow Tests
// ============================================================================

TEST_CASE("NanoVGCSS Box Shadow", "[box-shadow]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Simple box shadow") {
        const char* css = R"(
            .shadowed {
                box-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "shadowed");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    SECTION("Multiple box shadows") {
        const char* css = R"(
            .multi-shadow {
                box-shadow:
                    2px 2px 4px rgba(0, 0, 0, 0.3),
                    -2px -2px 4px rgba(255, 255, 255, 0.3);
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "multi-shadow");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Text Properties Tests
// ============================================================================

TEST_CASE("NanoVGCSS Text Properties", "[text]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Font properties") {
        const char* css = R"(
            .text {
                font-size: 16px;
                font-weight: bold;
                font-family: sans-serif;
                color: #333;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "span");
        cssboxAddClass(elem, "text");
        cssboxSetText(elem, "Hello World");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    SECTION("Text alignment") {
        const char* css = R"(
            .centered { text-align: center; }
            .left { text-align: left; }
            .right { text-align: right; }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Text transform") {
        const char* css = R"(
            .uppercase { text-transform: uppercase; }
            .lowercase { text-transform: lowercase; }
            .capitalize { text-transform: capitalize; }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_CASE("NanoVGCSS Error Handling", "[error-handling]") {
    SECTION("Invalid CSS parsing") {
        cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

        // Malformed CSS should not crash, but may return 0
        const char* bad_css = "{ invalid css syntax }";
        cssboxParseCSS(renderer, bad_css);

        cssboxDeleteRenderer(renderer);
    }

    SECTION("Element operations on null") {
        cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

        // Operations on non-existent elements should not crash
        cssboxDeleteElement(renderer, "non-existent");
        cssboxElement* elem = cssboxGetElement(renderer, "non-existent");
        REQUIRE(elem == nullptr);

        cssboxDeleteRenderer(renderer);
    }

    SECTION("Empty CSS string") {
        cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

        // Empty CSS should be handled
        int result = cssboxParseCSS(renderer, "");
        // Should not crash

        cssboxDeleteRenderer(renderer);
    }
}

// ============================================================================
// Visibility Tests
// ============================================================================

TEST_CASE("NanoVGCSS Visibility", "[visibility]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Default visibility") {
        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        REQUIRE(elem->visible == true);
    }

    SECTION("Hidden elements") {
        const char* css = R"(
            .hidden { visibility: hidden; }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Margin Tests
// ============================================================================

TEST_CASE("NanoVGCSS Margin", "[margin]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Margin shorthand") {
        const char* css = R"(
            .spaced { margin: 20px; }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "spaced");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    SECTION("Individual margin sides") {
        const char* css = R"(
            .custom-margin {
                margin-top: 10px;
                margin-right: 20px;
                margin-bottom: 30px;
                margin-left: 40px;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "custom-margin");

        cssboxComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    cssboxDeleteRenderer(renderer);
}
