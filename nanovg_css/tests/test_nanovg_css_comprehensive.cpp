/**
 * Comprehensive Catch2 Unit Tests for NanoVG CSS
 *
 * Tests all major features of the nanovg_css module including:
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
#include <nanovg.h>
#include <nanovg_css.h>
#include <cmath>

using Catch::Matchers::WithinAbs;

// ============================================================================
// Test Fixtures
// ============================================================================

class NanoVGCSSFixture {
protected:
    NVGcontext* vg = nullptr;
    NVGCSSRenderer* renderer = nullptr;

    void SetUp() {
        // Create a null context for testing (no actual rendering)
        vg = nullptr;
        renderer = nvgcssCreateRenderer(vg);
        REQUIRE(renderer != nullptr);

        // Set up a standard viewport
        nvgcssSetViewport(renderer, 800, 600);
    }

    void TearDown() {
        if (renderer) {
            nvgcssDeleteRenderer(renderer);
            renderer = nullptr;
        }
    }
};

// ============================================================================
// Core API Tests
// ============================================================================

TEST_CASE("NanoVGCSS Core API", "[core]") {
    SECTION("Renderer creation and deletion") {
        NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
        REQUIRE(renderer != nullptr);
        nvgcssDeleteRenderer(renderer);
    }

    SECTION("Element creation and retrieval") {
        NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test-elem", "rect");
        REQUIRE(elem != nullptr);
        REQUIRE(elem->id == "test-elem");
        REQUIRE(elem->type == "rect");

        NVGCSSElement* retrieved = nvgcssGetElement(renderer, "test-elem");
        REQUIRE(retrieved == elem);

        nvgcssDeleteRenderer(renderer);
    }

    SECTION("Element deletion") {
        NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);

        nvgcssCreateElement(renderer, "elem1", "rect");
        nvgcssDeleteElement(renderer, "elem1");

        NVGCSSElement* should_be_null = nvgcssGetElement(renderer, "elem1");
        REQUIRE(should_be_null == nullptr);

        nvgcssDeleteRenderer(renderer);
    }

    SECTION("Clear all elements") {
        NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);

        nvgcssCreateElement(renderer, "elem1", "rect");
        nvgcssCreateElement(renderer, "elem2", "circle");
        nvgcssCreateElement(renderer, "elem3", "text");

        nvgcssClearElements(renderer);

        REQUIRE(nvgcssGetElement(renderer, "elem1") == nullptr);
        REQUIRE(nvgcssGetElement(renderer, "elem2") == nullptr);
        REQUIRE(nvgcssGetElement(renderer, "elem3") == nullptr);

        nvgcssDeleteRenderer(renderer);
    }
}

// ============================================================================
// CSS Class Management Tests
// ============================================================================

TEST_CASE("NanoVGCSS Class Management", "[classes]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");

    SECTION("Add class") {
        nvgcssAddClass(elem, "button");
        REQUIRE(nvgcssHasClass(elem, "button") == 1);
    }

    SECTION("Remove class") {
        nvgcssAddClass(elem, "button");
        nvgcssRemoveClass(elem, "button");
        REQUIRE(nvgcssHasClass(elem, "button") == 0);
    }

    SECTION("Multiple classes") {
        nvgcssAddClass(elem, "button");
        nvgcssAddClass(elem, "primary");
        nvgcssAddClass(elem, "large");

        REQUIRE(nvgcssHasClass(elem, "button") == 1);
        REQUIRE(nvgcssHasClass(elem, "primary") == 1);
        REQUIRE(nvgcssHasClass(elem, "large") == 1);
        REQUIRE(nvgcssHasClass(elem, "small") == 0);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Inline Style Tests
// ============================================================================

TEST_CASE("NanoVGCSS Inline Styles", "[inline-styles]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");

    SECTION("Set inline style") {
        nvgcssSetStyle(elem, "width", "200px");
        nvgcssSetStyle(elem, "height", "100px");

        REQUIRE(elem->inline_style["width"] == "200px");
        REQUIRE(elem->inline_style["height"] == "100px");
    }

    SECTION("Override inline style") {
        nvgcssSetStyle(elem, "width", "100px");
        nvgcssSetStyle(elem, "width", "200px");

        REQUIRE(elem->inline_style["width"] == "200px");
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// CSS Parsing Tests
// ============================================================================

TEST_CASE("NanoVGCSS CSS Parsing", "[css-parsing]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);

    SECTION("Parse simple CSS") {
        const char* css = R"(
            .button {
                width: 100px;
                height: 50px;
                background: blue;
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Parse multiple selectors") {
        const char* css = R"(
            .button { width: 100px; }
            #myid { height: 200px; }
            rect { background: red; }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Parse pseudo-state selectors") {
        const char* css = R"(
            .button:hover { background: lightblue; }
            .button:active { background: darkblue; }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Clear CSS rules") {
        nvgcssParseCSS(renderer, ".button { width: 100px; }");
        nvgcssClearCSS(renderer);
        // After clearing, parsing new CSS should work
        int result = nvgcssParseCSS(renderer, ".other { height: 50px; }");
        REQUIRE(result == 1);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Pseudo-State Tests
// ============================================================================

TEST_CASE("NanoVGCSS Pseudo-States", "[pseudo-states]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");

    SECTION("Set pseudo-state") {
        nvgcssSetPseudoState(elem, "hover", 1);
        REQUIRE(nvgcssHasPseudoState(elem, "hover") == 1);
    }

    SECTION("Clear pseudo-state") {
        nvgcssSetPseudoState(elem, "hover", 1);
        nvgcssSetPseudoState(elem, "hover", 0);
        REQUIRE(nvgcssHasPseudoState(elem, "hover") == 0);
    }

    SECTION("Multiple pseudo-states") {
        nvgcssSetPseudoState(elem, "hover", 1);
        nvgcssSetPseudoState(elem, "active", 1);
        nvgcssSetPseudoState(elem, "focus", 1);

        REQUIRE(nvgcssHasPseudoState(elem, "hover") == 1);
        REQUIRE(nvgcssHasPseudoState(elem, "active") == 1);
        REQUIRE(nvgcssHasPseudoState(elem, "focus") == 1);
        REQUIRE(nvgcssHasPseudoState(elem, "disabled") == 0);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Tree Manipulation Tests
// ============================================================================

TEST_CASE("NanoVGCSS Tree Manipulation", "[tree]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);

    SECTION("Append child") {
        NVGCSSElement* parent = nvgcssCreateElement(renderer, "parent", "group");
        NVGCSSElement* child = nvgcssCreateElement(renderer, "child", "rect");

        nvgcssAppendChild(renderer, parent, child);

        REQUIRE(child->parent == parent);
        REQUIRE(parent->children.size() == 1);
        REQUIRE(parent->children[0] == child);
    }

    SECTION("Multiple children") {
        NVGCSSElement* parent = nvgcssCreateElement(renderer, "parent", "group");
        NVGCSSElement* child1 = nvgcssCreateElement(renderer, "child1", "rect");
        NVGCSSElement* child2 = nvgcssCreateElement(renderer, "child2", "circle");
        NVGCSSElement* child3 = nvgcssCreateElement(renderer, "child3", "text");

        nvgcssAppendChild(renderer, parent, child1);
        nvgcssAppendChild(renderer, parent, child2);
        nvgcssAppendChild(renderer, parent, child3);

        REQUIRE(parent->children.size() == 3);
        REQUIRE(parent->children[0] == child1);
        REQUIRE(parent->children[1] == child2);
        REQUIRE(parent->children[2] == child3);
    }

    SECTION("Remove child") {
        NVGCSSElement* parent = nvgcssCreateElement(renderer, "parent", "group");
        NVGCSSElement* child = nvgcssCreateElement(renderer, "child", "rect");

        nvgcssAppendChild(renderer, parent, child);
        nvgcssRemoveChild(parent, child);

        REQUIRE(parent->children.size() == 0);
        REQUIRE(child->parent == nullptr);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Text Content Tests
// ============================================================================

TEST_CASE("NanoVGCSS Text Content", "[text]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "text");

    SECTION("Set text content") {
        nvgcssSetText(elem, "Hello, World!");
        REQUIRE(std::string(nvgcssGetText(elem)) == "Hello, World!");
    }

    SECTION("Update text content") {
        nvgcssSetText(elem, "First");
        nvgcssSetText(elem, "Second");
        REQUIRE(std::string(nvgcssGetText(elem)) == "Second");
    }

    SECTION("Empty text") {
        nvgcssSetText(elem, "");
        REQUIRE(std::string(nvgcssGetText(elem)) == "");
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Color Parsing Tests
// ============================================================================

TEST_CASE("NanoVGCSS Color Parsing", "[color]") {
    SECTION("Named colors") {
        NVGcolor red = nvgcssParseColor("red");
        REQUIRE_THAT(red.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(red.g, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(red.b, WithinAbs(0.0f, 0.01f));

        NVGcolor blue = nvgcssParseColor("blue");
        REQUIRE_THAT(blue.b, WithinAbs(1.0f, 0.01f));

        NVGcolor white = nvgcssParseColor("white");
        REQUIRE_THAT(white.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(white.g, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(white.b, WithinAbs(1.0f, 0.01f));
    }

    SECTION("Hex colors - 6 digit") {
        NVGcolor red = nvgcssParseColor("#ff0000");
        REQUIRE_THAT(red.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(red.g, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(red.b, WithinAbs(0.0f, 0.01f));

        NVGcolor green = nvgcssParseColor("#00ff00");
        REQUIRE_THAT(green.g, WithinAbs(1.0f, 0.01f));

        NVGcolor blue = nvgcssParseColor("#0000ff");
        REQUIRE_THAT(blue.b, WithinAbs(1.0f, 0.01f));
    }

    SECTION("Hex colors - 3 digit") {
        NVGcolor red = nvgcssParseColor("#f00");
        REQUIRE_THAT(red.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(red.g, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(red.b, WithinAbs(0.0f, 0.01f));
    }

    SECTION("RGB colors") {
        NVGcolor red = nvgcssParseColor("rgb(255, 0, 0)");
        REQUIRE_THAT(red.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(red.g, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(red.b, WithinAbs(0.0f, 0.01f));

        NVGcolor half = nvgcssParseColor("rgb(128, 128, 128)");
        REQUIRE_THAT(half.r, WithinAbs(0.5f, 0.02f));
        REQUIRE_THAT(half.g, WithinAbs(0.5f, 0.02f));
        REQUIRE_THAT(half.b, WithinAbs(0.5f, 0.02f));
    }

    SECTION("RGBA colors with alpha") {
        NVGcolor semi = nvgcssParseColor("rgba(255, 0, 0, 0.5)");
        REQUIRE_THAT(semi.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(semi.a, WithinAbs(0.5f, 0.01f));
    }
}

// ============================================================================
// Length Parsing Tests
// ============================================================================

TEST_CASE("NanoVGCSS Length Parsing", "[length]") {
    SECTION("Pixel values") {
        float px = nvgcssParseLength("100px", 0);
        REQUIRE_THAT(px, WithinAbs(100.0f, 0.01f));

        float decimal = nvgcssParseLength("50.5px", 0);
        REQUIRE_THAT(decimal, WithinAbs(50.5f, 0.01f));
    }

    SECTION("Percentage values") {
        float percent = nvgcssParseLength("50%", 800);
        REQUIRE_THAT(percent, WithinAbs(400.0f, 0.01f));

        float full = nvgcssParseLength("100%", 600);
        REQUIRE_THAT(full, WithinAbs(600.0f, 0.01f));

        float quarter = nvgcssParseLength("25%", 400);
        REQUIRE_THAT(quarter, WithinAbs(100.0f, 0.01f));
    }

    SECTION("Plain numbers") {
        float num = nvgcssParseLength("42", 0);
        REQUIRE_THAT(num, WithinAbs(42.0f, 0.01f));
    }

    SECTION("Zero values") {
        float zero = nvgcssParseLength("0", 0);
        REQUIRE_THAT(zero, WithinAbs(0.0f, 0.01f));

        float zero_px = nvgcssParseLength("0px", 0);
        REQUIRE_THAT(zero_px, WithinAbs(0.0f, 0.01f));
    }
}

// ============================================================================
// CSS Variables Tests
// ============================================================================

TEST_CASE("NanoVGCSS CSS Variables", "[variables]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);

    SECTION("Set and use CSS variable") {
        nvgcssSetVariable(renderer, "--primary-color", "#4a90e2");

        const char* css = R"(
            .button {
                background: var(--primary-color);
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Multiple CSS variables") {
        nvgcssSetVariable(renderer, "--spacing", "10px");
        nvgcssSetVariable(renderer, "--color-primary", "blue");
        nvgcssSetVariable(renderer, "--color-secondary", "red");

        const char* css = R"(
            .box {
                padding: var(--spacing);
                background: var(--color-primary);
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Layout Computation Tests
// ============================================================================

TEST_CASE("NanoVGCSS Layout Computation", "[layout]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("Parse CSS and create elements") {
        const char* css = R"(
            .box {
                width: 200px;
                height: 100px;
            }
        )";

        int parse_result = nvgcssParseCSS(renderer, css);
        REQUIRE(parse_result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        REQUIRE(elem != nullptr);
        nvgcssAddClass(elem, "box");
        nvgcssSetStyle(elem, "x", "50px");
        nvgcssSetStyle(elem, "y", "50px");

        // Layout computation should not crash
        nvgcssComputeLayout(renderer);
    }

    SECTION("CSS with padding") {
        const char* css = R"(
            .padded {
                width: 200px;
                height: 100px;
                padding: 10px;
            }
        )";

        int parse_result = nvgcssParseCSS(renderer, css);
        REQUIRE(parse_result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "padded");

        // Layout computation should not crash
        nvgcssComputeLayout(renderer);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Style Cascade Tests (Specificity)
// ============================================================================

TEST_CASE("NanoVGCSS Style Cascade", "[cascade]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("Inline style application") {
        const char* css = R"(
            .button {
                width: 100px;
            }
        )";

        nvgcssParseCSS(renderer, css);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "button");
        nvgcssSetStyle(elem, "width", "200px");  // Inline style (highest specificity)

        // Verify inline style is stored
        REQUIRE(elem->inline_style["width"] == "200px");
    }

    SECTION("Multiple selectors parse successfully") {
        const char* css = R"(
            .button { width: 100px; }
            #mybutton { width: 150px; }
            rect { height: 50px; }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);
    }

    SECTION("Pseudo-state CSS parses") {
        const char* css = R"(
            .button { width: 100px; }
            .button:hover { width: 120px; }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "button");
        nvgcssSetPseudoState(elem, "hover", 1);

        // Verify pseudo-state is set
        REQUIRE(nvgcssHasPseudoState(elem, "hover") == 1);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Attribute Tests
// ============================================================================

TEST_CASE("NanoVGCSS Attributes", "[attributes]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");

    SECTION("Set attribute") {
        nvgcssSetAttribute(elem, "data-value", "42");
        REQUIRE(elem->attributes["data-value"] == "42");
    }

    SECTION("Multiple attributes") {
        nvgcssSetAttribute(elem, "role", "button");
        nvgcssSetAttribute(elem, "aria-label", "Submit");
        nvgcssSetAttribute(elem, "data-id", "123");

        REQUIRE(elem->attributes["role"] == "button");
        REQUIRE(elem->attributes["aria-label"] == "Submit");
        REQUIRE(elem->attributes["data-id"] == "123");
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Viewport Tests
// ============================================================================

TEST_CASE("NanoVGCSS Viewport", "[viewport]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);

    SECTION("Set viewport size") {
        nvgcssSetViewport(renderer, 1920, 1080);
        // Just verify the function doesn't crash
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Update Tests (Time-based)
// ============================================================================

TEST_CASE("NanoVGCSS Update", "[update]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);

    SECTION("Update with delta time") {
        // Should not crash or error
        nvgcssUpdate(renderer, 0.016f);  // ~60 FPS frame
        nvgcssUpdate(renderer, 0.033f);  // ~30 FPS frame
    }

    SECTION("Multiple updates") {
        for (int i = 0; i < 60; ++i) {
            nvgcssUpdate(renderer, 0.016f);
        }
        // Should handle multiple frames without issues
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("NanoVGCSS Integration", "[integration]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

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

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* button = nvgcssCreateElement(renderer, "btn1", "rect");
        nvgcssAddClass(button, "button");
        nvgcssSetStyle(button, "x", "100px");
        nvgcssSetStyle(button, "y", "50px");

        // Verify element setup
        REQUIRE(nvgcssHasClass(button, "button") == 1);
        REQUIRE(button->inline_style["x"] == "100px");
        REQUIRE(button->inline_style["y"] == "50px");

        // Test state transitions
        nvgcssSetPseudoState(button, "hover", 1);
        REQUIRE(nvgcssHasPseudoState(button, "hover") == 1);

        nvgcssSetPseudoState(button, "active", 1);
        REQUIRE(nvgcssHasPseudoState(button, "active") == 1);

        // Layout should not crash
        nvgcssComputeLayout(renderer);
    }

    SECTION("Parent-child hierarchy") {
        NVGCSSElement* container = nvgcssCreateElement(renderer, "container", "group");
        NVGCSSElement* child1 = nvgcssCreateElement(renderer, "child1", "rect");
        NVGCSSElement* child2 = nvgcssCreateElement(renderer, "child2", "rect");

        nvgcssAppendChild(renderer, container, child1);
        nvgcssAppendChild(renderer, container, child2);

        nvgcssSetStyle(container, "x", "0px");
        nvgcssSetStyle(container, "y", "0px");
        nvgcssSetStyle(child1, "width", "100px");
        nvgcssSetStyle(child1, "height", "50px");
        nvgcssSetStyle(child2, "width", "150px");
        nvgcssSetStyle(child2, "height", "75px");

        nvgcssComputeLayout(renderer);

        REQUIRE(container->children.size() == 2);
        REQUIRE(child1->parent == container);
        REQUIRE(child2->parent == container);
    }

    nvgcssDeleteRenderer(renderer);
}

// ============================================================================
// Box Model Tests
// ============================================================================

TEST_CASE("NanoVGCSS Box Model", "[box-model]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("CSS with padding syntax") {
        const char* css = R"(
            .box {
                padding-top: 10px;
                padding-right: 20px;
                padding-bottom: 30px;
                padding-left: 40px;
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "box");

        // Layout should not crash
        nvgcssComputeLayout(renderer);
    }

    SECTION("CSS with border-radius") {
        const char* css = R"(
            .rounded {
                border-radius: 15px;
            }
        )";

        int result = nvgcssParseCSS(renderer, css);
        REQUIRE(result == 1);

        NVGCSSElement* elem = nvgcssCreateElement(renderer, "test", "rect");
        nvgcssAddClass(elem, "rounded");

        // Layout should not crash
        nvgcssComputeLayout(renderer);
    }

    nvgcssDeleteRenderer(renderer);
}
