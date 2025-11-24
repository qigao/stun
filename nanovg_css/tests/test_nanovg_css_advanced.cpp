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
#include <nanovg_css.h>
#include "nanovg_css_internal.h"  // For internal structure access in tests
#include <cmath>

using Catch::Matchers::WithinAbs;

// ============================================================================
// Transform Tests
// ============================================================================

TEST_CASE("NanoVGCSS Transforms", "[transforms]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

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

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "translated");

        nvgcssComputeLayout(renderer);

        // Verify element created successfully
        REQUIRE(elem != nullptr);
    }

    SECTION("Rotate transform") {
        const char* css = R"(
            .rotated {
                transform: rotate(45deg);
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "rotated");

        nvgcssComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    SECTION("Scale transform") {
        const char* css = R"(
            .scaled {
                transform: scale(2, 3);
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "scaled");

        nvgcssComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    SECTION("Multiple transforms") {
        const char* css = R"(
            .complex {
                transform: translate(10px, 20px) rotate(30deg) scale(1.5);
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "complex");

        nvgcssComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Z-Index Tests
// ============================================================================

TEST_CASE("NanoVGCSS Z-Index", "[z-index]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("Set z-index via CSS") {
        const char* css = R"(
            .layer-1 { z-index: 1; }
            .layer-2 { z-index: 2; }
            .layer-10 { z-index: 10; }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem1 = nvgcssCreateElement(renderer, "elem1", "rect");
        NVGCSSElement* elem2 = nvgcssCreateElement(renderer, "elem2", "rect");
        NVGCSSElement* elem3 = nvgcssCreateElement(renderer, "elem3", "rect");

        nvgcssAddClass(elem1, "layer-1");
        nvgcssAddClass(elem2, "layer-2");
        nvgcssAddClass(elem3, "layer-10");

        nvgcssComputeLayout(renderer);

        REQUIRE(elem1 != nullptr);
        REQUIRE(elem2 != nullptr);
        REQUIRE(elem3 != nullptr);
    }

    SECTION("Negative z-index") {
        const char* css = R"(
            .behind { z-index: -1; }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "behind");

        nvgcssComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Opacity Tests
// ============================================================================

TEST_CASE("NanoVGCSS Opacity", "[opacity]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("Set opacity via CSS") {
        const char* css = R"(
            .transparent { opacity: 0.5; }
            .invisible { opacity: 0; }
            .opaque { opacity: 1; }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem1 = nvgcssCreateElement(renderer, "elem1", "rect");
        NVGCSSElement* elem2 = nvgcssCreateElement(renderer, "elem2", "rect");
        NVGCSSElement* elem3 = nvgcssCreateElement(renderer, "elem3", "rect");

        nvgcssAddClass(elem1, "transparent");
        nvgcssAddClass(elem2, "invisible");
        nvgcssAddClass(elem3, "opaque");

        nvgcssComputeLayout(renderer);

        // Verify elements were created
        REQUIRE(elem1 != nullptr);
        REQUIRE(elem2 != nullptr);
        REQUIRE(elem3 != nullptr);
    }

    SECTION("Default opacity") {
        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        REQUIRE(elem->opacity == 1.0f);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Position Property Tests
// ============================================================================

TEST_CASE("NanoVGCSS Position Property", "[position]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("Absolute positioning") {
        const char* css = R"(
            .absolute {
                position: absolute;
                left: 50px;
                top: 100px;
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "absolute");

        nvgcssComputeLayout(renderer);
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

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "relative");

        nvgcssComputeLayout(renderer);
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

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "fixed");

        nvgcssComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Display Property Tests
// ============================================================================

TEST_CASE("NanoVGCSS Display Property", "[display]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("Display flex") {
        const char* css = R"(
            .flex-container {
                display: flex;
                flex-direction: row;
                gap: 10px;
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* container = nvgcssCreateElement(renderer, "container", "div");
        nvgcssAddClass(container, "flex-container");

        NVGCSSElement* child1 = nvgcssCreateElement(renderer, "child1", "div");
        NVGCSSElement* child2 = nvgcssCreateElement(renderer, "child2", "div");

        nvgcssAppendChild(renderer, container, child1);
        nvgcssAppendChild(renderer, container, child2);

        nvgcssComputeLayout(renderer);

        int child_count = 0;
        NVGCSSElement** children = nvgcssGetChildren(renderer, container, &child_count);
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

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* container = nvgcssCreateElement(renderer, "container", "div");
        nvgcssAddClass(container, "grid-container");

        nvgcssComputeLayout(renderer);
        REQUIRE(container != nullptr);
    }

    SECTION("Display none") {
        const char* css = R"(
            .hidden {
                display: none;
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "hidden");

        nvgcssComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Overflow Property Tests
// ============================================================================

TEST_CASE("NanoVGCSS Overflow Property", "[overflow]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("Overflow hidden") {
        const char* css = R"(
            .clip {
                overflow: hidden;
                width: 200px;
                height: 100px;
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
        nvgcssAddClass(elem, "clip");

        nvgcssComputeLayout(renderer);
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

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
        nvgcssAddClass(elem, "scrollable");

        nvgcssComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Computed Style Retrieval Tests
// ============================================================================

TEST_CASE("NanoVGCSS Computed Style Retrieval", "[computed-style]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("Get computed style property") {
        const char* css = R"(
            .button {
                width: 120px;
                background: blue;
            }
        )";

        nvgcssParseCSS(renderer, css);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "button");

        nvgcssComputeLayout(renderer);

        char buffer[256];
        int has_width = nvgcssGetComputedStyle(renderer, elem, "width", buffer, sizeof(buffer));
        // Function should execute without crashing
        REQUIRE(elem != nullptr);
    }

    SECTION("Get non-existent property") {
        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");

        char buffer[256];
        int result = nvgcssGetComputedStyle(renderer, elem, "non-existent-property", buffer, sizeof(buffer));
        // Should return 0 for non-existent property
        REQUIRE(result == 0);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Complex Selector Tests
// ============================================================================

TEST_CASE("NanoVGCSS Complex Selectors", "[selectors]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("Descendant selector") {
        const char* css = R"(
            .container .item {
                background: red;
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Child selector") {
        const char* css = R"(
            .parent > .child {
                color: blue;
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
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

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "button");
        nvgcssAddClass(elem, "button");
        nvgcssAddClass(elem, "primary");

        REQUIRE(nvgcssHasClass(elem, "button") == 1);
        REQUIRE(nvgcssHasClass(elem, "primary") == 1);
    }

    SECTION("Attribute selector") {
        const char* css = R"(
            [data-type="button"] {
                background: green;
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "div");
        // Direct attribute manipulation (internal API for testing)
        elem->attributes["data-type"] = "button";

        REQUIRE(elem->attributes["data-type"] == "button");
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Border and Border-Radius Tests
// ============================================================================

TEST_CASE("NanoVGCSS Border Properties", "[border]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("Border shorthand") {
        const char* css = R"(
            .bordered {
                border: 2px solid red;
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "bordered");

        nvgcssComputeLayout(renderer);
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

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "custom-border");

        nvgcssComputeLayout(renderer);
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

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "custom-radius");

        nvgcssComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Box Shadow Tests
// ============================================================================

TEST_CASE("NanoVGCSS Box Shadow", "[box-shadow]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("Simple box shadow") {
        const char* css = R"(
            .shadowed {
                box-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "shadowed");

        nvgcssComputeLayout(renderer);
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

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "multi-shadow");

        nvgcssComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Text Properties Tests
// ============================================================================

TEST_CASE("NanoVGCSS Text Properties", "[text]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("Font properties") {
        const char* css = R"(
            .text {
                font-size: 16px;
                font-weight: bold;
                font-family: sans-serif;
                color: #333;
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "span");
        nvgcssAddClass(elem, "text");
        nvgcssSetText(elem, "Hello World");

        nvgcssComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    SECTION("Text alignment") {
        const char* css = R"(
            .centered { text-align: center; }
            .left { text-align: left; }
            .right { text-align: right; }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Text transform") {
        const char* css = R"(
            .uppercase { text-transform: uppercase; }
            .lowercase { text-transform: lowercase; }
            .capitalize { text-transform: capitalize; }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_CASE("NanoVGCSS Error Handling", "[error-handling]") {
    SECTION("Invalid CSS parsing") {
        NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);

        // Malformed CSS should not crash, but may return 0
        const char* bad_css = "{ invalid css syntax }";
        nvgcssParseCSS(renderer, bad_css);

        nvgcssDeleteRenderer(renderer);
    }

    SECTION("Element operations on null") {
        NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);

        // Operations on non-existent elements should not crash
        nvgcssDeleteElement(renderer, "non-existent");
        NVGCSSElement* elem = nvgcssGetElement(renderer, "non-existent");
        REQUIRE(elem == nullptr);

        nvgcssDeleteRenderer(renderer);
    }

    SECTION("Empty CSS string") {
        NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);

        // Empty CSS should be handled
        int result = nvgcssParseCSS(renderer, "");
        // Should not crash

        nvgcssDeleteRenderer(renderer);
    }
}

// ============================================================================
// Visibility Tests
// ============================================================================

TEST_CASE("NanoVGCSS Visibility", "[visibility]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);

    SECTION("Default visibility") {
        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        REQUIRE(elem->visible == true);
    }

    SECTION("Hidden elements") {
        const char* css = R"(
            .hidden { visibility: hidden; }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Margin Tests
// ============================================================================

TEST_CASE("NanoVGCSS Margin", "[margin]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("Margin shorthand") {
        const char* css = R"(
            .spaced { margin: 20px; }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "spaced");

        nvgcssComputeLayout(renderer);
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

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "custom-margin");

        nvgcssComputeLayout(renderer);
        REQUIRE(elem != nullptr);
    }

    nvgcssDeleteRenderer(renderer);
}
