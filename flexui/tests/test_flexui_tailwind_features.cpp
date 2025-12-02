#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <nanovg.h>
#include <nanovg_css.h>
#include <string>

using Catch::Matchers::WithinAbs;


// Helper to get computed style as string
std::string get_style(NVGCSSRenderer* renderer, NVGCSSElement* element, const char* prop) {
    char buf[64];
    if (nvgcssGetComputedStyle(renderer, element, prop, buf, sizeof(buf))) {
        return std::string(buf);
    }
    return "";
}

TEST_CASE("FlexUI Tailwind Support", "[flexui][tailwind]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    
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

        nvgcssParseCSS(renderer, css);

        NVGCSSElement* group = nvgcssCreateElement(renderer, "group", "div");
        nvgcssAddClass(group, "button-group");

        // Test Desktop (> 600px)
        nvgcssSetViewport(renderer, 1024, 768);
        nvgcssComputeLayout(renderer);
        REQUIRE(get_style(renderer, group, "flex-direction") == "row");

        // Test Mobile (< 600px)
        nvgcssSetViewport(renderer, 400, 800);
        nvgcssComputeLayout(renderer);
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

        nvgcssParseCSS(renderer, css);

        NVGCSSElement* btn = nvgcssCreateElement(renderer, "btn", "button");
        nvgcssAddClass(btn, "btn-primary");

        // Normal State
        nvgcssComputeLayout(renderer);
        REQUIRE(get_style(renderer, btn, "background-color") == "#3b82f6");

        // Hover State
        nvgcssSetPseudoState(btn, "hover", 1);
        nvgcssComputeLayout(renderer); // Recompute to apply pseudo-state styles
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

        nvgcssParseCSS(renderer, css);

        NVGCSSElement* input = nvgcssCreateElement(renderer, "input", "input");
        nvgcssAddClass(input, "input");

        // Normal State
        nvgcssComputeLayout(renderer);
        REQUIRE(get_style(renderer, input, "border-color") == "#d1d5db"); // var(--gray-300)

        // Focus State
        nvgcssSetPseudoState(input, "focus", 1);
        nvgcssComputeLayout(renderer);
        REQUIRE(get_style(renderer, input, "border-color") == "#3b82f6"); // var(--blue-500)
        REQUIRE(get_style(renderer, input, "border-width") == "2px");
    }

    nvgcssDeleteRenderer(renderer);
}
