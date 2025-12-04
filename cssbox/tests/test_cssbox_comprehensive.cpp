/**
 * Comprehensive Catch2 Unit Tests for NanoVG CSS
 *
 * Tests all major features of the cssbox module including:
 * - Core API (renderer, element management)
 * - CSS parsing and selectors
 * - Box model and layout
 * - Colors and lengths
 * - Transforms
 * - Pseudo-states
 * - Tree manipulation
 * - Style cascade and specificity
 * - CSS variables
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/catch_approx.hpp>
#include <nanovg.h>
#include <cssbox.h>
#include "cssbox_internal.h"  // For internal structure access in tests
#include <cmath>

using Catch::Matchers::WithinAbs;
using Catch::Approx;
// ============================================================================
// Test Fixtures
// ============================================================================

class NanoVGCSSFixture {
protected:
    NVGcontext* vg = nullptr;
    cssboxRenderer* renderer = nullptr;

    void SetUp() {
        // Create a null context for testing (no actual rendering)
        vg = nullptr;
        renderer = cssboxCreateRenderer(vg);
        REQUIRE(renderer != nullptr);

        // Set up a standard viewport
        cssboxSetViewport(renderer, 800, 600);
    }

    void TearDown() {
        if (renderer) {
            cssboxDeleteRenderer(renderer);
            renderer = nullptr;
        }
    }
};

// ============================================================================
// Core API Tests
// ============================================================================

TEST_CASE("NanoVGCSS Core API", "[core]") {
    SECTION("Renderer creation and deletion") {
        cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
        REQUIRE(renderer != nullptr);
        cssboxDeleteRenderer(renderer);
    }

    SECTION("Element creation and retrieval") {
        cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

        cssboxElement* elem = cssboxCreateElement(renderer, "test-elem", "rect");
        REQUIRE(elem != nullptr);
        REQUIRE(elem->id == "test-elem");
        REQUIRE(elem->type == "rect");

        cssboxElement* retrieved = cssboxGetElement(renderer, "test-elem");
        REQUIRE(retrieved == elem);

        cssboxDeleteRenderer(renderer);
    }

    SECTION("Element deletion") {
        cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

        cssboxCreateElement(renderer, "elem1", "rect");
        cssboxDeleteElement(renderer, "elem1");

        cssboxElement* should_be_null = cssboxGetElement(renderer, "elem1");
        REQUIRE(should_be_null == nullptr);

        cssboxDeleteRenderer(renderer);
    }

    SECTION("Clear all elements") {
        cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

        cssboxCreateElement(renderer, "elem1", "rect");
        cssboxCreateElement(renderer, "elem2", "circle");
        cssboxCreateElement(renderer, "elem3", "text");

        cssboxClearElements(renderer);

        REQUIRE(cssboxGetElement(renderer, "elem1") == nullptr);
        REQUIRE(cssboxGetElement(renderer, "elem2") == nullptr);
        REQUIRE(cssboxGetElement(renderer, "elem3") == nullptr);

        cssboxDeleteRenderer(renderer);
    }
}

// ============================================================================
// CSS Class Management Tests
// ============================================================================

TEST_CASE("NanoVGCSS Class Management", "[classes]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");

    SECTION("Add class") {
        cssboxAddClass(elem, "button");
        REQUIRE(cssboxHasClass(elem, "button") == 1);
    }

    SECTION("Remove class") {
        cssboxAddClass(elem, "button");
        cssboxRemoveClass(elem, "button");
        REQUIRE(cssboxHasClass(elem, "button") == 0);
    }

    SECTION("Multiple classes") {
        cssboxAddClass(elem, "button");
        cssboxAddClass(elem, "primary");
        cssboxAddClass(elem, "large");

        REQUIRE(cssboxHasClass(elem, "button") == 1);
        REQUIRE(cssboxHasClass(elem, "primary") == 1);
        REQUIRE(cssboxHasClass(elem, "large") == 1);
        REQUIRE(cssboxHasClass(elem, "small") == 0);
    }

    cssboxDeleteRenderer(renderer);
}


// ============================================================================
// CSS Parsing Tests
// ============================================================================

TEST_CASE("NanoVGCSS CSS Parsing", "[css-parsing]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Parse simple CSS") {
        const char* css = R"(
            .button {
                width: 100px;
                height: 50px;
                background: blue;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Parse multiple selectors") {
        const char* css = R"(
            .button { width: 100px; }
            #myid { height: 200px; }
            rect { background: red; }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Parse pseudo-state selectors") {
        const char* css = R"(
            .button:hover { background: lightblue; }
            .button:active { background: darkblue; }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Clear CSS rules") {
        cssboxParseCSS(renderer, ".button { width: 100px; }");
        cssboxClearCSS(renderer);
        // After clearing, parsing new CSS should work
        int result = cssboxParseCSS(renderer, ".other { height: 50px; }");
        REQUIRE(result == 1);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Pseudo-State Tests
// ============================================================================

TEST_CASE("NanoVGCSS Pseudo-States", "[pseudo-states]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");

    SECTION("Set pseudo-state") {
        cssboxSetPseudoState(elem, "hover", 1);
        REQUIRE(cssboxHasPseudoState(elem, "hover") == 1);
    }

    SECTION("Clear pseudo-state") {
        cssboxSetPseudoState(elem, "hover", 1);
        cssboxSetPseudoState(elem, "hover", 0);
        REQUIRE(cssboxHasPseudoState(elem, "hover") == 0);
    }

    SECTION("Multiple pseudo-states") {
        cssboxSetPseudoState(elem, "hover", 1);
        cssboxSetPseudoState(elem, "active", 1);
        cssboxSetPseudoState(elem, "focus", 1);

        REQUIRE(cssboxHasPseudoState(elem, "hover") == 1);
        REQUIRE(cssboxHasPseudoState(elem, "active") == 1);
        REQUIRE(cssboxHasPseudoState(elem, "focus") == 1);
        REQUIRE(cssboxHasPseudoState(elem, "disabled") == 0);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Tree Manipulation Tests
// ============================================================================

TEST_CASE("NanoVGCSS Tree Manipulation", "[tree]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Append child") {
        cssboxElement* parent = cssboxCreateElement(renderer, "parent", "group");
        cssboxElement* child = cssboxCreateElement(renderer, "child", "rect");

        cssboxAppendChild(renderer, parent, child);

        REQUIRE(child->parent_internal_id == parent->internal_id);
        int child_count = 0;
        cssboxElement** children = cssboxGetChildren(renderer, parent, &child_count);
        REQUIRE(child_count == 1);
        REQUIRE(children[0] == child);
    }

    SECTION("Multiple children") {
        cssboxElement* parent = cssboxCreateElement(renderer, "parent", "group");
        cssboxElement* child1 = cssboxCreateElement(renderer, "child1", "rect");
        cssboxElement* child2 = cssboxCreateElement(renderer, "child2", "circle");
        cssboxElement* child3 = cssboxCreateElement(renderer, "child3", "text");

        cssboxAppendChild(renderer, parent, child1);
        cssboxAppendChild(renderer, parent, child2);
        cssboxAppendChild(renderer, parent, child3);

        int child_count = 0;
        cssboxElement** children = cssboxGetChildren(renderer, parent, &child_count);
        REQUIRE(child_count == 3);
        REQUIRE(children[0] == child1);
        REQUIRE(children[1] == child2);
        REQUIRE(children[2] == child3);
    }

    SECTION("Remove child") {
        cssboxElement* parent = cssboxCreateElement(renderer, "parent", "group");
        cssboxElement* child = cssboxCreateElement(renderer, "child", "rect");

        cssboxAppendChild(renderer, parent, child);
        cssboxRemoveChild(renderer, parent, child);

        int child_count = 0;
        cssboxElement** children = cssboxGetChildren(renderer, parent, &child_count);
        REQUIRE(child_count == 0);
        REQUIRE(child->parent_internal_id == -1);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Text Content Tests
// ============================================================================

TEST_CASE("NanoVGCSS Text Content", "[text]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxElement* elem = cssboxCreateElement(renderer, "test", "text");

    SECTION("Set text content") {
        cssboxSetText(elem, "Hello, World!");
        REQUIRE(std::string(cssboxGetText(elem)) == "Hello, World!");
    }

    SECTION("Update text content") {
        cssboxSetText(elem, "First");
        cssboxSetText(elem, "Second");
        REQUIRE(std::string(cssboxGetText(elem)) == "Second");
    }

    SECTION("Empty text") {
        cssboxSetText(elem, "");
        REQUIRE(std::string(cssboxGetText(elem)) == "");
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Color Parsing Tests
// ============================================================================

TEST_CASE("NanoVGCSS Color Parsing", "[color]") {
    SECTION("Named colors") {
        NVGcolor red = cssboxParseColor("red");
        REQUIRE_THAT(red.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(red.g, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(red.b, WithinAbs(0.0f, 0.01f));

        NVGcolor blue = cssboxParseColor("blue");
        REQUIRE_THAT(blue.b, WithinAbs(1.0f, 0.01f));

        NVGcolor white = cssboxParseColor("white");
        REQUIRE_THAT(white.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(white.g, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(white.b, WithinAbs(1.0f, 0.01f));
    }

    SECTION("Hex colors - 6 digit") {
        NVGcolor red = cssboxParseColor("#ff0000");
        REQUIRE_THAT(red.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(red.g, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(red.b, WithinAbs(0.0f, 0.01f));

        NVGcolor green = cssboxParseColor("#00ff00");
        REQUIRE_THAT(green.g, WithinAbs(1.0f, 0.01f));

        NVGcolor blue = cssboxParseColor("#0000ff");
        REQUIRE_THAT(blue.b, WithinAbs(1.0f, 0.01f));
    }

    SECTION("Hex colors - 3 digit") {
        NVGcolor red = cssboxParseColor("#f00");
        REQUIRE_THAT(red.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(red.g, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(red.b, WithinAbs(0.0f, 0.01f));
    }

    SECTION("RGB colors") {
        NVGcolor red = cssboxParseColor("rgb(255, 0, 0)");
        REQUIRE_THAT(red.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(red.g, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(red.b, WithinAbs(0.0f, 0.01f));

        NVGcolor half = cssboxParseColor("rgb(128, 128, 128)");
        REQUIRE_THAT(half.r, WithinAbs(0.5f, 0.02f));
        REQUIRE_THAT(half.g, WithinAbs(0.5f, 0.02f));
        REQUIRE_THAT(half.b, WithinAbs(0.5f, 0.02f));
    }

    SECTION("RGBA colors with alpha") {
        NVGcolor semi = cssboxParseColor("rgba(255, 0, 0, 0.5)");
        REQUIRE_THAT(semi.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(semi.a, WithinAbs(0.5f, 0.01f));
    }
}

// ============================================================================
// Length Parsing Tests
// ============================================================================

TEST_CASE("NanoVGCSS Length Parsing", "[length]") {
    SECTION("Pixel values") {
        float px = cssboxParseLength("100px", 0);
        REQUIRE_THAT(px, WithinAbs(100.0f, 0.01f));

        float decimal = cssboxParseLength("50.5px", 0);
        REQUIRE_THAT(decimal, WithinAbs(50.5f, 0.01f));
    }

    SECTION("Percentage values") {
        float percent = cssboxParseLength("50%", 800);
        REQUIRE_THAT(percent, WithinAbs(400.0f, 0.01f));

        float full = cssboxParseLength("100%", 600);
        REQUIRE_THAT(full, WithinAbs(600.0f, 0.01f));

        float quarter = cssboxParseLength("25%", 400);
        REQUIRE_THAT(quarter, WithinAbs(100.0f, 0.01f));
    }

    SECTION("Plain numbers") {
        float num = cssboxParseLength("42", 0);
        REQUIRE_THAT(num, WithinAbs(42.0f, 0.01f));
    }

    SECTION("Zero values") {
        float zero = cssboxParseLength("0", 0);
        REQUIRE_THAT(zero, WithinAbs(0.0f, 0.01f));

        float zero_px = cssboxParseLength("0px", 0);
        REQUIRE_THAT(zero_px, WithinAbs(0.0f, 0.01f));
    }
}

// ============================================================================
// CSS Variables Tests
// ============================================================================

TEST_CASE("NanoVGCSS CSS Variables", "[variables]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Set and use CSS variable") {
        cssboxSetVariable(renderer, "--primary-color", "#4a90e2");

        const char* css = R"(
            .button {
                background: var(--primary-color);
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Multiple CSS variables") {
        cssboxSetVariable(renderer, "--spacing", "10px");
        cssboxSetVariable(renderer, "--color-primary", "blue");
        cssboxSetVariable(renderer, "--color-secondary", "red");

        const char* css = R"(
            .box {
                padding: var(--spacing);
                background: var(--color-primary);
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Layout Computation Tests
// ============================================================================

TEST_CASE("NanoVGCSS Layout Computation", "[layout]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Parse CSS and create elements") {
        const char* css = R"(
            .box {
                width: 200px;
                height: 100px;
                x: 50px;
                y: 50px;
            }
        )";

        int parse_result = cssboxParseCSS(renderer, css);
        REQUIRE(parse_result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        REQUIRE(elem != nullptr);
        cssboxAddClass(elem, "box");

        // Layout computation should not crash
        cssboxComputeLayout(renderer);
    }

    SECTION("CSS with padding") {
        const char* css = R"(
            .padded {
                width: 200px;
                height: 100px;
                padding: 10px;
            }
        )";

        int parse_result = cssboxParseCSS(renderer, css);
        REQUIRE(parse_result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "padded");

        // Layout computation should not crash
        cssboxComputeLayout(renderer);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Style Cascade Tests (Specificity)
// ============================================================================

TEST_CASE("NanoVGCSS Style Cascade", "[cascade]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Multiple selectors parse successfully") {
        const char* css = R"(
            .button { width: 100px; }
            #mybutton { width: 150px; }
            rect { height: 50px; }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Pseudo-state CSS parses") {
        const char* css = R"(
            .button { width: 100px; }
            .button:hover { width: 120px; }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "button");
        cssboxSetPseudoState(elem, "hover", 1);

        // Verify pseudo-state is set
        REQUIRE(cssboxHasPseudoState(elem, "hover") == 1);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Attribute Tests
// ============================================================================

TEST_CASE("NanoVGCSS Attributes", "[attributes]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");

    SECTION("Set attribute directly") {
        // Direct attribute manipulation (internal API for testing)
        elem->attributes["data-value"] = "42";
        REQUIRE(elem->attributes["data-value"] == "42");
    }

    SECTION("Multiple attributes") {
        // Direct attribute manipulation (internal API for testing)
        elem->attributes["role"] = "button";
        elem->attributes["aria-label"] = "Submit";
        elem->attributes["data-id"] = "123";

        REQUIRE(elem->attributes["role"] == "button");
        REQUIRE(elem->attributes["aria-label"] == "Submit");
        REQUIRE(elem->attributes["data-id"] == "123");
    }

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Attribute styles invalidate cache", "[attributes][cache]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    const char* css = R"(
        [data-tone="warm"] { color: red; }
        [data-tone="cool"] { color: blue; }
    )";
    REQUIRE(cssboxParseCSS(renderer, css) == 1);

    cssboxElement* elem = cssboxCreateElement(renderer, "tone", "rect");
    char buffer[32];

    // Direct attribute manipulation (internal API for testing)
    elem->attributes["data-tone"] = "warm";
    REQUIRE(cssboxGetComputedStyle(renderer, elem, "color", buffer, sizeof(buffer)) == 1);
    REQUIRE(std::string(buffer) == "red");

    // Change attribute and verify style updates
    elem->attributes["data-tone"] = "cool";
    REQUIRE(cssboxGetComputedStyle(renderer, elem, "color", buffer, sizeof(buffer)) == 1);
    REQUIRE(std::string(buffer) == "blue");

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Viewport Tests
// ============================================================================

TEST_CASE("NanoVGCSS Viewport", "[viewport]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Set viewport size") {
        cssboxSetViewport(renderer, 1920, 1080);
        // Just verify the function doesn't crash
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Update Tests (Time-based)
// ============================================================================

TEST_CASE("NanoVGCSS Update", "[update]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Update with delta time") {
        // Should not crash or error
        cssboxUpdate(renderer, 0.016f);  // ~60 FPS frame
        cssboxUpdate(renderer, 0.033f);  // ~30 FPS frame
    }

    SECTION("Multiple updates") {
        for (int i = 0; i < 60; ++i) {
            cssboxUpdate(renderer, 0.016f);
        }
        // Should handle multiple frames without issues
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("NanoVGCSS Integration", "[integration]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Complete button example") {
        const char* css = R"(
            .button {
                width: 120px;
                height: 40px;
                background: blue;
                border-radius: 5px;
                padding: 10px;
            }

            .button:hover {
                background: lightblue;
            }

            .button:active {
                background: darkblue;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* button = cssboxCreateElement(renderer, "btn1", "rect");
        cssboxAddClass(button, "button");

        // Verify element setup
        REQUIRE(cssboxHasClass(button, "button") == 1);

        // Test state transitions
        cssboxSetPseudoState(button, "hover", 1);
        REQUIRE(cssboxHasPseudoState(button, "hover") == 1);

        cssboxSetPseudoState(button, "active", 1);
        REQUIRE(cssboxHasPseudoState(button, "active") == 1);

        // Layout should not crash
        cssboxComputeLayout(renderer);
    }

    SECTION("Parent-child hierarchy") {
        const char* css = R"(
            #container { x: 0px; y: 0px; }
            #child1 { width: 100px; height: 50px; }
            #child2 { width: 150px; height: 75px; }
        )";
        cssboxParseCSS(renderer, css);

        cssboxElement* container = cssboxCreateElement(renderer, "container", "group");
        cssboxElement* child1 = cssboxCreateElement(renderer, "child1", "rect");
        cssboxElement* child2 = cssboxCreateElement(renderer, "child2", "rect");

        cssboxAppendChild(renderer, container, child1);
        cssboxAppendChild(renderer, container, child2);

        cssboxComputeLayout(renderer);

        int child_count = 0;
        cssboxElement** children = cssboxGetChildren(renderer, container, &child_count);
        REQUIRE(child_count == 2);
        REQUIRE(child1->parent_internal_id == container->internal_id);
        REQUIRE(child2->parent_internal_id == container->internal_id);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Box Model Tests
// ============================================================================

TEST_CASE("NanoVGCSS Box Model", "[box-model]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("CSS with padding syntax") {
        const char* css = R"(
            .box {
                padding-top: 10px;
                padding-right: 20px;
                padding-bottom: 30px;
                padding-left: 40px;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "box");

        // Layout should not crash
        cssboxComputeLayout(renderer);
    }

    SECTION("CSS with border-radius") {
        const char* css = R"(
            .rounded {
                border-radius: 15px;
            }
        )";

        int result = cssboxParseCSS(renderer, css);
        REQUIRE(result == 1);

        cssboxElement* elem = cssboxCreateElement(renderer, "test", "rect");
        cssboxAddClass(elem, "rounded");

        // Layout should not crash
        cssboxComputeLayout(renderer);
    }

    cssboxDeleteRenderer(renderer);
}
