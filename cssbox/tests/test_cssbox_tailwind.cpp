/**
 * Tailwind CSS Unit Tests
 *
 * Tests for Tailwind-style utility classes and CSS variables.
 * Only tests public API and observable behavior, not implementation details.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <nanovg.h>
#include <cssbox.h>
#include "cssbox_internal.h"
#include "test_config.h"
using Catch::Matchers::WithinAbs;
using namespace cssbox;

INIT_TEST_LOGGING();

// ============================================================================
// CSS Variable Resolution Tests (via public API only)
// ============================================================================

TEST_CASE("Tailwind - CSS Variable Resolution", "[tailwind][variables]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Resolve simple color variable") {
        const char* css = R"(
            :root {
                --blue-500: #3b82f6;
            }
            .bg-blue-500 {
                background-color: var(--blue-500);
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* el = cssboxCreateElement(renderer, "test", "div");
        el->classes.push_back("bg-blue-500");

        cssboxComputeLayout(renderer);

        // Verify variable was resolved correctly via computed style
        auto computed = renderer->stylesheet->compute_style(
            el->id, el->type, el->classes, el->attributes,
            el->pseudo_states, el->inline_style, {},
            el->child_index, el->total_siblings
        );

        REQUIRE(computed.count("background-color") == 1);
        REQUIRE(computed["background-color"] == "#3b82f6");
    }

    SECTION("REGRESSION: --white variable resolution") {
        // This was the original bug: --white was skipped after comments
        const char* css = R"(
            :root {
                /* Neutral Gray */
                --white: #ffffff;
                --black: #000000;
            }
            .text-white { color: var(--white); }
            .text-black { color: var(--black); }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* white_el = cssboxCreateElement(renderer, "white", "div");
        white_el->classes.push_back("text-white");

        cssboxElement* black_el = cssboxCreateElement(renderer, "black", "div");
        black_el->classes.push_back("text-black");

        cssboxComputeLayout(renderer);

        auto white_style = renderer->stylesheet->compute_style(
            white_el->id, white_el->type, white_el->classes, white_el->attributes,
            white_el->pseudo_states, white_el->inline_style, {},
            white_el->child_index, white_el->total_siblings
        );

        auto black_style = renderer->stylesheet->compute_style(
            black_el->id, black_el->type, black_el->classes, black_el->attributes,
            black_el->pseudo_states, black_el->inline_style, {},
            black_el->child_index, black_el->total_siblings
        );

        // var(--white) should resolve to #ffffff
        REQUIRE(white_style["color"] == "#ffffff");
        // var(--black) should also work
        REQUIRE(black_style["color"] == "#000000");
    }

    SECTION("Multiple variables in same element") {
        const char* css = R"(
            :root {
                --blue-500: #3b82f6;
                --white: #ffffff;
            }
            .btn-primary {
                background-color: var(--blue-500);
                color: var(--white);
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* el = cssboxCreateElement(renderer, "test", "button");
        el->classes.push_back("btn-primary");

        cssboxComputeLayout(renderer);

        auto computed = renderer->stylesheet->compute_style(
            el->id, el->type, el->classes, el->attributes,
            el->pseudo_states, el->inline_style, {},
            el->child_index, el->total_siblings
        );

        REQUIRE(computed["background-color"] == "#3b82f6");
        REQUIRE(computed["color"] == "#ffffff");
    }

    SECTION("Fallback when variable doesn't exist") {
        const char* css = R"(
            .test {
                background-color: var(--nonexistent, #ff0000);
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* el = cssboxCreateElement(renderer, "test", "div");
        el->classes.push_back("test");

        cssboxComputeLayout(renderer);

        auto computed = renderer->stylesheet->compute_style(
            el->id, el->type, el->classes, el->attributes,
            el->pseudo_states, el->inline_style, {},
            el->child_index, el->total_siblings
        );

        // Should use fallback value
        REQUIRE(computed.count("background-color") == 1);
        REQUIRE(computed["background-color"] == "#ff0000");
    }

    SECTION("Tailwind color palette variables") {
        const char* css = R"(
            :root {
                --blue-100: #dbeafe;
                --blue-200: #bfdbfe;
                --blue-500: #3b82f6;
                --blue-900: #1e3a8a;
            }
            .bg-blue-100 { background-color: var(--blue-100); }
            .bg-blue-200 { background-color: var(--blue-200); }
            .bg-blue-500 { background-color: var(--blue-500); }
            .bg-blue-900 { background-color: var(--blue-900); }
        )";

        cssboxParseCSS(renderer, css);

        auto test_color = [&](const char* class_name, const char* expected_color) {
            cssboxElement* el = cssboxCreateElement(renderer, class_name, "div");
            el->classes.push_back(class_name);
            cssboxComputeLayout(renderer);

            auto computed = renderer->stylesheet->compute_style(
                el->id, el->type, el->classes, el->attributes,
                el->pseudo_states, el->inline_style, {},
                el->child_index, el->total_siblings
            );

            REQUIRE(computed["background-color"] == expected_color);
        };

        test_color("bg-blue-100", "#dbeafe");
        test_color("bg-blue-200", "#bfdbfe");
        test_color("bg-blue-500", "#3b82f6");
        test_color("bg-blue-900", "#1e3a8a");
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Tailwind Utility Class Tests
// ============================================================================

TEST_CASE("Tailwind - Atomic Utility Classes", "[tailwind][utilities]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Width utilities (w-*)") {
        const char* css = R"(
            .w-20 { width: 80px; }
            .w-24 { width: 96px; }
            .w-32 { width: 128px; }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* el20 = cssboxCreateElement(renderer, "el20", "div");
        el20->classes.push_back("w-20");

        cssboxElement* el24 = cssboxCreateElement(renderer, "el24", "div");
        el24->classes.push_back("w-24");

        cssboxElement* el32 = cssboxCreateElement(renderer, "el32", "div");
        el32->classes.push_back("w-32");

        cssboxComputeLayout(renderer);

        REQUIRE_THAT(el20->style.width.value, WithinAbs(80.0f, 0.1f));
        REQUIRE_THAT(el24->style.width.value, WithinAbs(96.0f, 0.1f));
        REQUIRE_THAT(el32->style.width.value, WithinAbs(128.0f, 0.1f));
    }

    SECTION("Height utilities (h-*)") {
        const char* css = R"(
            .h-8 { height: 32px; }
            .h-10 { height: 40px; }
            .h-12 { height: 48px; }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* el8 = cssboxCreateElement(renderer, "el8", "div");
        el8->classes.push_back("h-8");

        cssboxElement* el10 = cssboxCreateElement(renderer, "el10", "div");
        el10->classes.push_back("h-10");

        cssboxElement* el12 = cssboxCreateElement(renderer, "el12", "div");
        el12->classes.push_back("h-12");

        cssboxComputeLayout(renderer);

        REQUIRE_THAT(el8->style.height.value, WithinAbs(32.0f, 0.1f));
        REQUIRE_THAT(el10->style.height.value, WithinAbs(40.0f, 0.1f));
        REQUIRE_THAT(el12->style.height.value, WithinAbs(48.0f, 0.1f));
    }

    SECTION("Padding utilities (px-*, py-*)") {
        const char* css = R"(
            .px-6 { padding-left: 24px; padding-right: 24px; }
            .py-2 { padding-top: 8px; padding-bottom: 8px; }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* el = cssboxCreateElement(renderer, "el", "div");
        el->classes.push_back("px-6");
        el->classes.push_back("py-2");

        cssboxComputeLayout(renderer);

        REQUIRE_THAT(el->style.padding[1].value, WithinAbs(24.0f, 0.1f)); // right
        REQUIRE_THAT(el->style.padding[3].value, WithinAbs(24.0f, 0.1f)); // left
        REQUIRE_THAT(el->style.padding[0].value, WithinAbs(8.0f, 0.1f));  // top
        REQUIRE_THAT(el->style.padding[2].value, WithinAbs(8.0f, 0.1f));  // bottom
    }

    SECTION("Border radius utilities (rounded-*)") {
        const char* css = R"(
            .rounded { border-radius: 4px; }
            .rounded-lg { border-radius: 8px; }
            .rounded-full { border-radius: 9999px; }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* el = cssboxCreateElement(renderer, "el", "div");
        el->classes.push_back("rounded-lg");

        cssboxComputeLayout(renderer);

        REQUIRE_THAT(el->style.border.radius[0], WithinAbs(8.0f, 0.1f));
    }

    SECTION("Flexbox utilities") {
        const char* css = R"(
            .flex { display: flex; }
            .flex-row { flex-direction: row; }
            .flex-col { flex-direction: column; }
            .gap-3 { gap: 12px; }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* row = cssboxCreateElement(renderer, "row", "div");
        row->classes = {"flex", "flex-row", "gap-3"};

        cssboxComputeLayout(renderer);

        REQUIRE(row->style.display == Display::FLEX);
        REQUIRE(row->style.flex_direction == FlexDirection::ROW);
        REQUIRE_THAT(row->style.gap.value, WithinAbs(12.0f, 0.1f));
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Tailwind Button Component Test (Integration)
// ============================================================================

TEST_CASE("Tailwind - Complete Button Component", "[tailwind][integration][button]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Primary button with multiple utilities") {
        const char* css = R"(
            :root {
                --blue-500: #3b82f6;
                --white: #ffffff;
            }
            .bg-blue-500 { background-color: var(--blue-500); }
            .text-white { color: var(--white); }
            .px-6 { padding-left: 24px; padding-right: 24px; }
            .py-2 { padding-top: 8px; padding-bottom: 8px; }
            .rounded { border-radius: 4px; }
            .w-24 { width: 96px; }
            .h-10 { height: 40px; }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* button = cssboxCreateElement(renderer, "btn-primary", "button");
        button->classes = {"bg-blue-500", "text-white", "px-6", "py-2", "rounded", "w-24", "h-10"};

        cssboxComputeLayout(renderer);

        auto computed = renderer->stylesheet->compute_style(
            button->id, button->type, button->classes, button->attributes,
            button->pseudo_states, button->inline_style, {},
            button->child_index, button->total_siblings
        );

        // Verify all styles applied correctly
        REQUIRE(computed["background-color"] == "#3b82f6");
        REQUIRE(computed["color"] == "#ffffff");

        REQUIRE_THAT(button->style.width.value, WithinAbs(96.0f, 0.1f));
        REQUIRE_THAT(button->style.height.value, WithinAbs(40.0f, 0.1f));

        REQUIRE_THAT(button->style.padding[1].value, WithinAbs(24.0f, 0.1f)); // right
        REQUIRE_THAT(button->style.padding[3].value, WithinAbs(24.0f, 0.1f)); // left
        REQUIRE_THAT(button->style.padding[0].value, WithinAbs(8.0f, 0.1f));  // top
        REQUIRE_THAT(button->style.padding[2].value, WithinAbs(8.0f, 0.1f));  // bottom

        REQUIRE_THAT(button->style.border.radius[0], WithinAbs(4.0f, 0.1f));
    }

    SECTION("Semantic color buttons") {
        const char* css = R"(
            :root {
                --green-500: #22c55e;
                --yellow-500: #eab308;
                --red-500: #ef4444;
                --white: #ffffff;
            }
            .bg-green-500 { background-color: var(--green-500); }
            .bg-yellow-500 { background-color: var(--yellow-500); }
            .bg-red-500 { background-color: var(--red-500); }
            .text-white { color: var(--white); }
            .w-24 { width: 96px; }
            .h-10 { height: 40px; }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* success = cssboxCreateElement(renderer, "btn-success", "button");
        success->classes = {"bg-green-500", "text-white", "w-24", "h-10"};

        cssboxElement* warning = cssboxCreateElement(renderer, "btn-warning", "button");
        warning->classes = {"bg-yellow-500", "text-white", "w-24", "h-10"};

        cssboxElement* danger = cssboxCreateElement(renderer, "btn-danger", "button");
        danger->classes = {"bg-red-500", "text-white", "w-24", "h-10"};

        cssboxComputeLayout(renderer);

        auto success_style = renderer->stylesheet->compute_style(
            success->id, success->type, success->classes, success->attributes,
            success->pseudo_states, success->inline_style, {},
            success->child_index, success->total_siblings
        );

        auto warning_style = renderer->stylesheet->compute_style(
            warning->id, warning->type, warning->classes, warning->attributes,
            warning->pseudo_states, warning->inline_style, {},
            warning->child_index, warning->total_siblings
        );

        auto danger_style = renderer->stylesheet->compute_style(
            danger->id, danger->type, danger->classes, danger->attributes,
            danger->pseudo_states, danger->inline_style, {},
            danger->child_index, danger->total_siblings
        );

        REQUIRE(success_style["background-color"] == "#22c55e");
        REQUIRE(warning_style["background-color"] == "#eab308");
        REQUIRE(danger_style["background-color"] == "#ef4444");

        // All should have white text
        REQUIRE(success_style["color"] == "#ffffff");
        REQUIRE(warning_style["color"] == "#ffffff");
        REQUIRE(danger_style["color"] == "#ffffff");

        // All should have same dimensions
        REQUIRE_THAT(success->style.width.value, WithinAbs(96.0f, 0.1f));
        REQUIRE_THAT(warning->style.width.value, WithinAbs(96.0f, 0.1f));
        REQUIRE_THAT(danger->style.width.value, WithinAbs(96.0f, 0.1f));
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Regression Tests
// ============================================================================

TEST_CASE("Tailwind - Regression: Variables after comments", "[tailwind][regression]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Multiple variables before and after comments") {
        const char* css = R"(
            :root {
                --var1: #111111;
                /* Comment 1 */
                --var2: #222222;
                --var3: #333333;
                /* Comment 2 */
                --var4: #444444;
            }
            .test1 { color: var(--var1); }
            .test2 { color: var(--var2); }
            .test3 { color: var(--var3); }
            .test4 { color: var(--var4); }
        )";

        cssboxParseCSS(renderer, css);

        auto test_var = [&](const char* class_name, const char* expected_color) {
            cssboxElement* el = cssboxCreateElement(renderer, class_name, "div");
            el->classes.push_back(class_name);
            cssboxComputeLayout(renderer);

            auto computed = renderer->stylesheet->compute_style(
                el->id, el->type, el->classes, el->attributes,
                el->pseudo_states, el->inline_style, {},
                el->child_index, el->total_siblings
            );

            REQUIRE(computed["color"] == expected_color);
        };

        // All variables should resolve correctly regardless of comment position
        test_var("test1", "#111111");
        test_var("test2", "#222222");
        test_var("test3", "#333333");
        test_var("test4", "#444444");
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Tailwind Pseudo-class Variants (hover, focus, active)
// ============================================================================

TEST_CASE("Tailwind - Pseudo-class Variants", "[tailwind][pseudo-class]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Hover state styling") {
        const char* css = R"(
            :root {
                --blue-500: #3b82f6;
                --blue-600: #2563eb;
            }
            .bg-blue-500 { background-color: var(--blue-500); }
            .bg-blue-500:hover { background-color: var(--blue-600); }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* button = cssboxCreateElement(renderer, "btn", "button");
        button->classes.push_back("bg-blue-500");

        cssboxComputeLayout(renderer);

        // Normal state
        auto normal_style = renderer->stylesheet->compute_style(
            button->id, button->type, button->classes, button->attributes,
            button->pseudo_states, button->inline_style, {},
            button->child_index, button->total_siblings
        );
        REQUIRE(normal_style["background-color"] == "#3b82f6");

        // Hover state
        button->pseudo_states.insert("hover");
        auto hover_style = renderer->stylesheet->compute_style(
            button->id, button->type, button->classes, button->attributes,
            button->pseudo_states, button->inline_style, {},
            button->child_index, button->total_siblings
        );
        REQUIRE(hover_style["background-color"] == "#2563eb");
    }

    SECTION("Focus state with ring") {
        const char* css = R"(
            :root {
                --blue-500: #3b82f6;
            }
            .focus\:ring-2:focus {
                box-shadow: 0 0 0 2px var(--blue-500);
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* input = cssboxCreateElement(renderer, "input", "input");
        input->classes.push_back("focus:ring-2");

        cssboxComputeLayout(renderer);

        // Normal state - no box-shadow
        auto normal_style = renderer->stylesheet->compute_style(
            input->id, input->type, input->classes, input->attributes,
            input->pseudo_states, input->inline_style, {},
            input->child_index, input->total_siblings
        );
        REQUIRE(normal_style.count("box-shadow") == 0);

        // Focus state - has box-shadow
        input->pseudo_states.insert("focus");
        auto focus_style = renderer->stylesheet->compute_style(
            input->id, input->type, input->classes, input->attributes,
            input->pseudo_states, input->inline_style, {},
            input->child_index, input->total_siblings
        );
        REQUIRE(focus_style.count("box-shadow") == 1);
        REQUIRE(focus_style["box-shadow"].find("#3b82f6") != std::string::npos);
    }

    SECTION("Active state (button press)") {
        const char* css = R"(
            .scale-100 { transform: scale(1); }
            .scale-100:active { transform: scale(0.95); }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* button = cssboxCreateElement(renderer, "btn", "button");
        button->classes.push_back("scale-100");

        cssboxComputeLayout(renderer);

        auto normal_style = renderer->stylesheet->compute_style(
            button->id, button->type, button->classes, button->attributes,
            button->pseudo_states, button->inline_style, {},
            button->child_index, button->total_siblings
        );
        REQUIRE(normal_style["transform"] == "scale(1)");

        button->pseudo_states.insert("active");
        auto active_style = renderer->stylesheet->compute_style(
            button->id, button->type, button->classes, button->attributes,
            button->pseudo_states, button->inline_style, {},
            button->child_index, button->total_siblings
        );
        REQUIRE(active_style["transform"] == "scale(0.95)");
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Tailwind Responsive Breakpoints (sm, md, lg, xl)
// ============================================================================

TEST_CASE("Tailwind - Responsive Breakpoints", "[tailwind][responsive]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Mobile-first responsive width") {
        const char* css = R"(
            .w-full { width: 100%; }
            @media (min-width: 640px) {
                .sm\:w-1\/2 { width: 50%; }
            }
            @media (min-width: 768px) {
                .md\:w-1\/3 { width: 33.333333%; }
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* box = cssboxCreateElement(renderer, "box", "div");
        box->classes = {"w-full", "sm:w-1/2", "md:w-1/3"};

        // Simulate mobile viewport (320px)
        renderer->viewport_width = 320;
        cssboxComputeLayout(renderer);
        REQUIRE_THAT(box->style.width.value, WithinAbs(100.0f, 0.1f));

        // Simulate tablet viewport (768px)
        renderer->viewport_width = 768;
        cssboxComputeLayout(renderer);
        REQUIRE_THAT(box->style.width.value, WithinAbs(33.333333f, 0.1f));

        // Simulate desktop viewport (1024px)
        renderer->viewport_width = 1024;
        cssboxComputeLayout(renderer);
        REQUIRE_THAT(box->style.width.value, WithinAbs(33.333333f, 0.1f));
    }

    SECTION("Responsive flexbox direction") {
        const char* css = R"(
            .flex-col { flex-direction: column; }
            @media (min-width: 768px) {
                .md\:flex-row { flex-direction: row; }
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
        container->classes = {"flex-col", "md:flex-row"};

        // Mobile: column layout
        renderer->viewport_width = 375;
        cssboxComputeLayout(renderer);
        REQUIRE(container->style.flex_direction == FlexDirection::COLUMN);

        // Desktop: row layout
        renderer->viewport_width = 1024;
        cssboxComputeLayout(renderer);
        REQUIRE(container->style.flex_direction == FlexDirection::ROW);
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Tailwind State Combinations (multiple pseudo-classes)
// ============================================================================

TEST_CASE("Tailwind - State Combinations", "[tailwind][combinations]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Hover + Focus simultaneously") {
        const char* css = R"(
            :root {
                --blue-500: #3b82f6;
                --blue-600: #2563eb;
            }
            .btn {
                background-color: var(--blue-500);
            }
            .btn:hover {
                background-color: var(--blue-600);
            }
            .btn:focus {
                box-shadow: 0 0 0 3px rgba(59, 130, 246, 0.5);
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* button = cssboxCreateElement(renderer, "btn", "button");
        button->classes.push_back("btn");

        cssboxComputeLayout(renderer);

        // Hover + Focus both active
        button->pseudo_states.insert("hover");
        button->pseudo_states.insert("focus");

        auto combined_style = renderer->stylesheet->compute_style(
            button->id, button->type, button->classes, button->attributes,
            button->pseudo_states, button->inline_style, {},
            button->child_index, button->total_siblings
        );

        // Should have both hover background AND focus ring
        REQUIRE(combined_style["background-color"] == "#2563eb");
        REQUIRE(combined_style.count("box-shadow") == 1);
    }

    SECTION("Disabled state overrides hover") {
        const char* css = R"(
            :root {
                --blue-500: #3b82f6;
                --gray-300: #d1d5db;
            }
            .btn { background-color: var(--blue-500); }
            .btn:hover { background-color: #2563eb; }
            .btn:disabled {
                background-color: var(--gray-300);
                cursor: not-allowed;
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* button = cssboxCreateElement(renderer, "btn", "button");
        button->classes.push_back("btn");

        cssboxComputeLayout(renderer);

        // Disabled + Hover (disabled should win)
        button->pseudo_states.insert("disabled");
        button->pseudo_states.insert("hover");

        auto style = renderer->stylesheet->compute_style(
            button->id, button->type, button->classes, button->attributes,
            button->pseudo_states, button->inline_style, {},
            button->child_index, button->total_siblings
        );

        // Disabled style should override hover
        REQUIRE(style["background-color"] == "#d1d5db");
        REQUIRE(style["cursor"] == "not-allowed");
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Tailwind Real-world Components
// ============================================================================

TEST_CASE("Tailwind - Real-world Components", "[tailwind][integration][real-world]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Complete Card Component") {
        const char* css = R"(
            :root {
                --white: #ffffff;
                --gray-100: #f3f4f6;
                --gray-800: #1f2937;
                --blue-500: #3b82f6;
            }
            .bg-white { background-color: var(--white); }
            .text-gray-800 { color: var(--gray-800); }
            .text-blue-500 { color: var(--blue-500); }
            .p-6 { padding: 24px; }
            .mb-4 { margin-bottom: 16px; }
            .rounded-lg { border-radius: 8px; }
            .shadow-lg { box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1); }
        )";

        cssboxParseCSS(renderer, css);

        // Card container
        cssboxElement* card = cssboxCreateElement(renderer, "card", "div");
        card->classes = {"bg-white", "p-6", "rounded-lg", "shadow-lg"};

        // Card title
        cssboxElement* title = cssboxCreateElement(renderer, "title", "h2");
        title->classes = {"text-gray-800", "mb-4"};

        // Card link
        cssboxElement* link = cssboxCreateElement(renderer, "link", "a");
        link->classes = {"text-blue-500"};

        cssboxComputeLayout(renderer);

        auto card_style = renderer->stylesheet->compute_style(
            card->id, card->type, card->classes, card->attributes,
            card->pseudo_states, card->inline_style, {},
            card->child_index, card->total_siblings
        );

        // Verify card styling
        REQUIRE(card_style["background-color"] == "#ffffff");
        REQUIRE(card_style["padding"] == "24px");
        REQUIRE(card_style["border-radius"] == "8px");
        REQUIRE(card_style.count("box-shadow") == 1);

        REQUIRE_THAT(card->style.padding[0].value, WithinAbs(24.0f, 0.1f));
        REQUIRE_THAT(card->style.border.radius[0], WithinAbs(8.0f, 0.1f));
    }

    SECTION("Complete Form Input Component") {
        const char* css = R"(
            :root {
                --gray-300: #d1d5db;
                --blue-500: #3b82f6;
            }
            .w-full { width: 100%; }
            .px-4 { padding-left: 16px; padding-right: 16px; }
            .py-2 { padding-top: 8px; padding-bottom: 8px; }
            .border { border-width: 1px; }
            .border-gray-300 { border-color: var(--gray-300); }
            .rounded { border-radius: 4px; }
            .focus\:border-blue-500:focus { border-color: var(--blue-500); }
            .focus\:outline-none:focus { outline: none; }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* input = cssboxCreateElement(renderer, "email", "input");
        input->classes = {"w-full", "px-4", "py-2", "border", "border-gray-300",
                          "rounded", "focus:border-blue-500", "focus:outline-none"};

        cssboxComputeLayout(renderer);

        // Normal state
        auto normal_style = renderer->stylesheet->compute_style(
            input->id, input->type, input->classes, input->attributes,
            input->pseudo_states, input->inline_style, {},
            input->child_index, input->total_siblings
        );
        REQUIRE(normal_style["border-color"] == "#d1d5db");

        // Focus state
        input->pseudo_states.insert("focus");
        auto focus_style = renderer->stylesheet->compute_style(
            input->id, input->type, input->classes, input->attributes,
            input->pseudo_states, input->inline_style, {},
            input->child_index, input->total_siblings
        );
        REQUIRE(focus_style["border-color"] == "#3b82f6");
        REQUIRE(focus_style["outline"] == "none");
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Edge Cases and Class Conflicts
// ============================================================================

TEST_CASE("Tailwind - Edge Cases", "[tailwind][edge-cases]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Class override priority (last class wins)") {
        const char* css = R"(
            .w-20 { width: 80px; }
            .w-40 { width: 160px; }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* box = cssboxCreateElement(renderer, "box", "div");
        box->classes = {"w-20", "w-40"}; // w-40 should win

        cssboxComputeLayout(renderer);

        // Last specified class should take precedence
        REQUIRE_THAT(box->style.width.value, WithinAbs(160.0f, 0.1f));
    }

    SECTION("Empty class list") {
        cssboxElement* box = cssboxCreateElement(renderer, "box", "div");
        box->classes = {}; // No classes

        cssboxComputeLayout(renderer);

        // Should not crash, element gets default styling
        REQUIRE(box->style.display == Display::BLOCK);
    }

    SECTION("Duplicate classes (should be deduplicated)") {
        const char* css = R"(
            .px-4 { padding-left: 16px; padding-right: 16px; }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* box = cssboxCreateElement(renderer, "box", "div");
        box->classes = {"px-4", "px-4", "px-4"}; // Duplicates

        cssboxComputeLayout(renderer);

        // Should apply px-4 once, not triple the padding
        REQUIRE_THAT(box->style.padding[1].value, WithinAbs(16.0f, 0.1f));
        REQUIRE_THAT(box->style.padding[3].value, WithinAbs(16.0f, 0.1f));
    }

    SECTION("Non-existent CSS variables (fallback)") {
        const char* css = R"(
            .test {
                color: var(--non-existent, #ff0000);
                background: var(--also-missing, var(--still-missing, #00ff00));
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* box = cssboxCreateElement(renderer, "box", "div");
        box->classes.push_back("test");

        cssboxComputeLayout(renderer);

        auto style = renderer->stylesheet->compute_style(
            box->id, box->type, box->classes, box->attributes,
            box->pseudo_states, box->inline_style, {},
            box->child_index, box->total_siblings
        );

        // Should use fallback values
        REQUIRE(style["color"] == "#ff0000");
        REQUIRE(style["background"] == "#00ff00");
    }

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Performance and Stress Tests
// ============================================================================

TEST_CASE("Tailwind - Performance Stress Test", "[tailwind][performance]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);

    SECTION("Many elements with many classes") {
        const char* css = R"(
            :root {
                --blue-500: #3b82f6;
                --white: #ffffff;
            }
            .flex { display: flex; }
            .gap-4 { gap: 16px; }
            .p-6 { padding: 24px; }
            .bg-blue-500 { background-color: var(--blue-500); }
            .text-white { color: var(--white); }
            .rounded-lg { border-radius: 8px; }
            .shadow { box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
        )";

        cssboxParseCSS(renderer, css);

        // Create 100 elements, each with 7 classes
        std::vector<cssboxElement*> elements;
        for (int i = 0; i < 100; i++) {
            std::string id = "el_" + std::to_string(i);
            cssboxElement* el = cssboxCreateElement(renderer, id.c_str(), "div");
            el->classes = {"flex", "gap-4", "p-6", "bg-blue-500",
                          "text-white", "rounded-lg", "shadow"};
            elements.push_back(el);
        }

        // Should not crash or timeout
        cssboxComputeLayout(renderer);

        // Verify first and last elements are styled correctly
        REQUIRE(elements[0]->style.display == Display::FLEX);
        REQUIRE_THAT(elements[0]->style.gap.value, WithinAbs(16.0f, 0.1f));
        REQUIRE_THAT(elements[99]->style.padding[0].value, WithinAbs(24.0f, 0.1f));
    }

    SECTION("Deep CSS variable resolution chain") {
        const char* css = R"(
            :root {
                --color-1: var(--color-2);
                --color-2: var(--color-3);
                --color-3: var(--color-4);
                --color-4: var(--color-5);
                --color-5: #3b82f6;
            }
            .test { color: var(--color-1); }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* el = cssboxCreateElement(renderer, "el", "div");
        el->classes.push_back("test");

        cssboxComputeLayout(renderer);

        auto style = renderer->stylesheet->compute_style(
            el->id, el->type, el->classes, el->attributes,
            el->pseudo_states, el->inline_style, {},
            el->child_index, el->total_siblings
        );

        // Should resolve through the entire chain
        REQUIRE(style["color"] == "#3b82f6");
    }

    cssboxDeleteRenderer(renderer);
}
