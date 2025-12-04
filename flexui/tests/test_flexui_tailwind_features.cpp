#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <nanovg.h>
#include <cssbox.h>
#include <string>

using Catch::Matchers::WithinAbs;


// Helper to get computed style as string
std::string get_style(cssboxRenderer* renderer, cssboxElement* element, const char* prop) {
    char buf[64];
    if (cssboxGetComputedStyle(renderer, element, prop, buf, sizeof(buf))) {
        return std::string(buf);
    }
    return "";
}

TEST_CASE("FlexUI Tailwind Support", "[flexui][tailwind]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    
    // ========================================================================
    // 1. Responsive Layout (Button Gallery)
    // ========================================================================
    SECTION("Responsive Button Group Layout") {
        const char* css = R"(
            /* Default: Horizontal layout */
            .button-group {
                display: flex;
                flex-direction: row;
            }

            /* Small screens: Stack buttons vertically */
            @media (max-width: 600px) {
                .button-group {
                    flex-direction: column;
                }
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* group = cssboxCreateElement(renderer, "group", "div");
        cssboxAddClass(group, "button-group");

        // Test Desktop (> 600px)
        cssboxSetViewport(renderer, 1024, 768);
        cssboxComputeLayout(renderer);
        REQUIRE(get_style(renderer, group, "flex-direction") == "row");

        // Test Mobile (< 600px)
        cssboxSetViewport(renderer, 400, 800);
        cssboxComputeLayout(renderer);
        REQUIRE(get_style(renderer, group, "flex-direction") == "column");
    }

    // ========================================================================
    // 2. Hover Effects (Button Gallery)
    // ========================================================================
    SECTION("Button Hover States") {
        const char* css = R"(
            :root {
                --blue-500: #3b82f6;
                --blue-600: #2563eb;
            }
            .btn-primary {
                background-color: var(--blue-500);
            }
            .btn-primary:hover {
                background-color: var(--blue-600);
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* btn = cssboxCreateElement(renderer, "btn", "button");
        cssboxAddClass(btn, "btn-primary");

        // Normal State
        cssboxComputeLayout(renderer);
        REQUIRE(get_style(renderer, btn, "background-color") == "#3b82f6");

        // Hover State
        cssboxSetPseudoState(btn, "hover", 1);
        cssboxComputeLayout(renderer); // Recompute to apply pseudo-state styles
        REQUIRE(get_style(renderer, btn, "background-color") == "#2563eb");
    }

    // ========================================================================
    // 3. Form Input Styling (Form Demo)
    // ========================================================================
    SECTION("Input Focus State") {
        const char* css = R"(
            :root {
                --blue-500: #3b82f6;
                --gray-300: #d1d5db;
            }
            .input {
                border-width: 1px;
                border-style: solid;
                border-color: var(--gray-300);
            }
            .input:focus {
                border-color: var(--blue-500);
                border-width: 2px;
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* input = cssboxCreateElement(renderer, "input", "input");
        cssboxAddClass(input, "input");

        // Normal State
        cssboxComputeLayout(renderer);
        REQUIRE(get_style(renderer, input, "border-color") == "#d1d5db"); // var(--gray-300)

        // Focus State
        cssboxSetPseudoState(input, "focus", 1);
        cssboxComputeLayout(renderer);
        REQUIRE(get_style(renderer, input, "border-color") == "#3b82f6"); // var(--blue-500)
        REQUIRE(get_style(renderer, input, "border-width") == "2px");
    }

    cssboxDeleteRenderer(renderer);
}
