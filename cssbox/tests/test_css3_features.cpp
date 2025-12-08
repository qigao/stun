/**
 * CSS3 Features Unit Tests
 *
 * Tests for newly implemented CSS3 features:
 * - grid-template-areas
 * - letter-spacing / word-spacing
 * - ::before/::after pseudo-elements
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/catch_approx.hpp>
#include <cssbox.h>
#include "cssbox_internal.h"
#include "lexbor_css_parser.h"
#include "cssbox_conversion.h"

using Catch::Matchers::WithinAbs;
using Catch::Approx;
using namespace cssbox;
using namespace cssbox::lexbor;

// ============================================================================
// grid-template-areas Tests
// ============================================================================

TEST_CASE("grid-template-areas - Parsing", "[grid][template-areas][parser]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Simple 2x2 grid areas") {
        const char* css = R"(
            #grid {
                display: grid;
                grid-template-areas: "header header" "sidebar main";
                grid-template-columns: 200px 1fr;
                grid-template-rows: 100px 1fr;
            }
        )";

        cssboxParseCSS(renderer, css);
        cssboxElement* grid = cssboxCreateElement(renderer, "grid", "div");
        cssboxComputeLayout(renderer);

        // Verify grid_template_areas map is populated
        REQUIRE(grid->style.grid_template_areas.size() == 3);
        REQUIRE(grid->style.grid_template_areas.count("header") == 1);
        REQUIRE(grid->style.grid_template_areas.count("sidebar") == 1);
        REQUIRE(grid->style.grid_template_areas.count("main") == 1);

        // Verify header spans columns 0-1 (0-based), row 0
        auto& header = grid->style.grid_template_areas["header"];
        REQUIRE(header.row_start == 0);
        REQUIRE(header.row_end == 1);
        REQUIRE(header.col_start == 0);
        REQUIRE(header.col_end == 2);  // spans 2 columns (0-based end exclusive)

        // Verify sidebar is column 0, row 1
        auto& sidebar = grid->style.grid_template_areas["sidebar"];
        REQUIRE(sidebar.row_start == 1);
        REQUIRE(sidebar.row_end == 2);
        REQUIRE(sidebar.col_start == 0);
        REQUIRE(sidebar.col_end == 1);

        // Verify main is column 1, row 1
        auto& main_area = grid->style.grid_template_areas["main"];
        REQUIRE(main_area.row_start == 1);
        REQUIRE(main_area.row_end == 2);
        REQUIRE(main_area.col_start == 1);
        REQUIRE(main_area.col_end == 2);
    }

    SECTION("3x3 holy grail layout") {
        const char* css = R"(
            #layout {
                display: grid;
                grid-template-areas: 
                    "header header header"
                    "nav main aside"
                    "footer footer footer";
            }
        )";

        cssboxParseCSS(renderer, css);
        cssboxElement* layout = cssboxCreateElement(renderer, "layout", "div");
        cssboxComputeLayout(renderer);

        REQUIRE(layout->style.grid_template_areas.size() == 5);

        // Header spans all 3 columns (0-based)
        auto& header = layout->style.grid_template_areas["header"];
        REQUIRE(header.col_start == 0);
        REQUIRE(header.col_end == 3);

        // Footer spans all 3 columns (0-based)
        auto& footer = layout->style.grid_template_areas["footer"];
        REQUIRE(footer.col_start == 0);
        REQUIRE(footer.col_end == 3);
    }

    SECTION("grid-area property on items") {
        const char* css = R"(
            #container {
                display: grid;
                grid-template-areas: "header header" "sidebar main";
            }
            #header-item { grid-area: header; }
            #sidebar-item { grid-area: sidebar; }
            #main-item { grid-area: main; }
        )";

        cssboxParseCSS(renderer, css);
        cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
        cssboxElement* header = cssboxCreateElement(renderer, "header-item", "div");
        cssboxElement* sidebar = cssboxCreateElement(renderer, "sidebar-item", "div");
        cssboxElement* main_el = cssboxCreateElement(renderer, "main-item", "div");

        cssboxAppendChild(renderer, container, header);
        cssboxAppendChild(renderer, container, sidebar);
        cssboxAppendChild(renderer, container, main_el);

        cssboxComputeLayout(renderer);

        REQUIRE(header->style.grid_area == "header");
        REQUIRE(sidebar->style.grid_area == "sidebar");
        REQUIRE(main_el->style.grid_area == "main");
    }

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("grid-template-areas - Empty cells with dot", "[grid][template-areas][parser]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        #grid {
            display: grid;
            grid-template-areas: "header header" ". main";
        }
    )";

    cssboxParseCSS(renderer, css);
    cssboxElement* grid = cssboxCreateElement(renderer, "grid", "div");
    cssboxComputeLayout(renderer);

    // Dot cells should not create named areas
    REQUIRE(grid->style.grid_template_areas.count(".") == 0);
    REQUIRE(grid->style.grid_template_areas.size() == 2);  // header and main only

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// letter-spacing / word-spacing Tests
// ============================================================================

TEST_CASE("letter-spacing - Parsing", "[text][letter-spacing][parser]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Pixel value") {
        const char* css = R"(
            #text { letter-spacing: 2px; }
        )";

        cssboxParseCSS(renderer, css);
        cssboxElement* text = cssboxCreateElement(renderer, "text", "span");
        cssboxComputeLayout(renderer);

        REQUIRE_THAT(text->style.letter_spacing, WithinAbs(2.0f, 0.1f));
    }

    SECTION("Em value") {
        const char* css = R"(
            #text { 
                font-size: 16px;
                letter-spacing: 0.1em; 
            }
        )";

        cssboxParseCSS(renderer, css);
        cssboxElement* text = cssboxCreateElement(renderer, "text", "span");
        cssboxComputeLayout(renderer);

        // 0.1em * 16px = 1.6px
        REQUIRE_THAT(text->style.letter_spacing, WithinAbs(1.6f, 0.1f));
    }

    SECTION("Normal keyword (default)") {
        const char* css = R"(
            #text { letter-spacing: normal; }
        )";

        cssboxParseCSS(renderer, css);
        cssboxElement* text = cssboxCreateElement(renderer, "text", "span");
        cssboxComputeLayout(renderer);

        REQUIRE_THAT(text->style.letter_spacing, WithinAbs(0.0f, 0.01f));
    }

    SECTION("Negative value") {
        const char* css = R"(
            #text { letter-spacing: -1px; }
        )";

        cssboxParseCSS(renderer, css);
        cssboxElement* text = cssboxCreateElement(renderer, "text", "span");
        cssboxComputeLayout(renderer);

        REQUIRE_THAT(text->style.letter_spacing, WithinAbs(-1.0f, 0.1f));
    }

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("word-spacing - Parsing", "[text][word-spacing][parser]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Pixel value") {
        const char* css = R"(
            #text { word-spacing: 5px; }
        )";

        cssboxParseCSS(renderer, css);
        cssboxElement* text = cssboxCreateElement(renderer, "text", "span");
        cssboxComputeLayout(renderer);

        REQUIRE_THAT(text->style.word_spacing, WithinAbs(5.0f, 0.1f));
    }

    SECTION("Normal keyword") {
        const char* css = R"(
            #text { word-spacing: normal; }
        )";

        cssboxParseCSS(renderer, css);
        cssboxElement* text = cssboxCreateElement(renderer, "text", "span");
        cssboxComputeLayout(renderer);

        REQUIRE_THAT(text->style.word_spacing, WithinAbs(0.0f, 0.01f));
    }

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("letter-spacing and word-spacing together", "[text][spacing]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        #text { 
            letter-spacing: 1px;
            word-spacing: 3px;
        }
    )";

    cssboxParseCSS(renderer, css);
    cssboxElement* text = cssboxCreateElement(renderer, "text", "span");
    cssboxComputeLayout(renderer);

    REQUIRE_THAT(text->style.letter_spacing, WithinAbs(1.0f, 0.1f));
    REQUIRE_THAT(text->style.word_spacing, WithinAbs(3.0f, 0.1f));

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// ::before/::after Pseudo-element Tests
// ============================================================================

TEST_CASE("Pseudo-element selector parsing", "[pseudo-element][parser]") {
    EnhancedStyleSheet stylesheet;

    SECTION("::before selector detection") {
        const char* css = R"(
            .button::before {
                content: ">> ";
                color: red;
            }
        )";

        REQUIRE(stylesheet.parse_css(css));

        auto rules = stylesheet.get_rules();
        REQUIRE(rules.size() >= 1);

        bool found_before = false;
        for (const auto& rule : rules) {
            if (rule.pseudo_element == CSSPseudoElement::BEFORE) {
                found_before = true;
                REQUIRE(rule.selector == ".button");
                REQUIRE(rule.properties.count("content") == 1);
            }
        }
        REQUIRE(found_before);
    }

    SECTION("::after selector detection") {
        const char* css = R"(
            .link::after {
                content: " ->";
            }
        )";

        REQUIRE(stylesheet.parse_css(css));

        auto rules = stylesheet.get_rules();
        bool found_after = false;
        for (const auto& rule : rules) {
            if (rule.pseudo_element == CSSPseudoElement::AFTER) {
                found_after = true;
                REQUIRE(rule.selector == ".link");
            }
        }
        REQUIRE(found_after);
    }

    SECTION("Legacy :before/:after syntax") {
        const char* css = R"(
            .old:before { content: "old"; }
            .new::before { content: "new"; }
        )";

        REQUIRE(stylesheet.parse_css(css));

        auto rules = stylesheet.get_rules();
        int before_count = 0;
        for (const auto& rule : rules) {
            if (rule.pseudo_element == CSSPseudoElement::BEFORE) {
                before_count++;
            }
        }
        REQUIRE(before_count == 2);  // Both syntaxes should work
    }

    SECTION("Normal rule without pseudo-element") {
        const char* css = R"(
            .button { color: blue; }
        )";

        REQUIRE(stylesheet.parse_css(css));

        auto rules = stylesheet.get_rules();
        REQUIRE(rules.size() >= 1);
        for (const auto& rule : rules) {
            if (rule.selector == ".button") {
                REQUIRE(rule.pseudo_element == CSSPseudoElement::NONE);
            }
        }
    }
}

TEST_CASE("compute_pseudo_element_style", "[pseudo-element][style]") {
    EnhancedStyleSheet stylesheet;

    const char* css = R"(
        .icon::before {
            content: "[icon]";
            color: blue;
            font-size: 12px;
        }
        .icon::after {
            content: "[/icon]";
            color: gray;
        }
        .icon {
            color: black;
            font-size: 16px;
        }
    )";

    REQUIRE(stylesheet.parse_css(css));

    std::vector<std::string> classes = {"icon"};
    std::map<std::string, std::string> attributes;
    std::set<std::string> pseudo_states;

    SECTION("Get ::before style") {
        auto before_style = stylesheet.compute_pseudo_element_style(
            CSSPseudoElement::BEFORE, "test", "div", classes, attributes, pseudo_states);

        REQUIRE(before_style.count("content") == 1);
        // Raw CSS value includes quotes
        REQUIRE(before_style["content"] == "\"[icon]\"");
        REQUIRE(before_style.count("color") == 1);
        REQUIRE(before_style["color"] == "blue");
    }

    SECTION("Get ::after style") {
        auto after_style = stylesheet.compute_pseudo_element_style(
            CSSPseudoElement::AFTER, "test", "div", classes, attributes, pseudo_states);

        REQUIRE(after_style.count("content") == 1);
        // Raw CSS value includes quotes
        REQUIRE(after_style["content"] == "\"[/icon]\"");
        REQUIRE(after_style["color"] == "gray");
    }

    SECTION("No pseudo-element style for non-matching element") {
        std::vector<std::string> other_classes = {"other"};
        auto style = stylesheet.compute_pseudo_element_style(
            CSSPseudoElement::BEFORE, "test", "div", other_classes, attributes, pseudo_states);

        REQUIRE(style.empty());
    }
}

TEST_CASE("content property parsing", "[pseudo-element][content]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    SECTION("Quoted string content") {
        const char* css = R"(
            #elem::before { content: "Hello"; }
        )";

        cssboxParseCSS(renderer, css);
        cssboxElement* elem = cssboxCreateElement(renderer, "elem", "div");
        cssboxComputeLayout(renderer);

        // Get pseudo-element style
        auto before_style = renderer->stylesheet->compute_pseudo_element_style(
            CSSPseudoElement::BEFORE, elem->id, elem->type, elem->classes, 
            elem->attributes, elem->pseudo_states);

        REQUIRE(before_style.count("content") == 1);
        // Raw CSS value includes quotes (stripping happens in typed conversion)
        REQUIRE(before_style["content"] == "\"Hello\"");
    }

    SECTION("Single-quoted content") {
        const char* css = R"(
            #elem::before { content: 'World'; }
        )";

        cssboxParseCSS(renderer, css);
        cssboxElement* elem = cssboxCreateElement(renderer, "elem", "div");
        cssboxComputeLayout(renderer);

        auto before_style = renderer->stylesheet->compute_pseudo_element_style(
            CSSPseudoElement::BEFORE, elem->id, elem->type, elem->classes,
            elem->attributes, elem->pseudo_states);

        // Raw CSS value includes quotes
        REQUIRE(before_style["content"] == "'World'");
    }

    SECTION("content: none") {
        const char* css = R"(
            #elem::before { content: none; }
        )";

        cssboxParseCSS(renderer, css);
        cssboxElement* elem = cssboxCreateElement(renderer, "elem", "div");
        cssboxComputeLayout(renderer);

        auto before_style = renderer->stylesheet->compute_pseudo_element_style(
            CSSPseudoElement::BEFORE, elem->id, elem->type, elem->classes,
            elem->attributes, elem->pseudo_states);

        // content: none should still be in the style map
        REQUIRE(before_style.count("content") == 1);
        REQUIRE(before_style["content"] == "none");
    }

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Pseudo-element with complex selectors", "[pseudo-element][selectors]") {
    EnhancedStyleSheet stylesheet;

    SECTION("ID selector with ::before") {
        const char* css = R"(
            #mybutton::before { content: ">"; }
        )";

        REQUIRE(stylesheet.parse_css(css));

        std::vector<std::string> classes;
        std::map<std::string, std::string> attributes;
        std::set<std::string> pseudo_states;

        auto style = stylesheet.compute_pseudo_element_style(
            CSSPseudoElement::BEFORE, "mybutton", "div", classes, attributes, pseudo_states);

        REQUIRE(style["content"] == "\">\"");
    }

    SECTION("Type selector with ::after") {
        const char* css = R"(
            button::after { content: "!"; }
        )";

        REQUIRE(stylesheet.parse_css(css));

        std::vector<std::string> classes;
        std::map<std::string, std::string> attributes;
        std::set<std::string> pseudo_states;

        auto style = stylesheet.compute_pseudo_element_style(
            CSSPseudoElement::AFTER, "any-id", "button", classes, attributes, pseudo_states);

        REQUIRE(style["content"] == "\"!\"");
    }

    SECTION("Multiple classes with ::before") {
        const char* css = R"(
            .btn.primary::before { content: "*"; }
        )";

        REQUIRE(stylesheet.parse_css(css));

        std::vector<std::string> classes = {"btn", "primary"};
        std::map<std::string, std::string> attributes;
        std::set<std::string> pseudo_states;

        auto style = stylesheet.compute_pseudo_element_style(
            CSSPseudoElement::BEFORE, "test", "div", classes, attributes, pseudo_states);

        REQUIRE(style["content"] == "\"*\"");
    }
}

TEST_CASE("Pseudo-element specificity", "[pseudo-element][specificity]") {
    EnhancedStyleSheet stylesheet;

    const char* css = R"(
        .btn::before { content: "low"; color: red; }
        #submit::before { content: "high"; }
    )";

    REQUIRE(stylesheet.parse_css(css));

    std::vector<std::string> classes = {"btn"};
    std::map<std::string, std::string> attributes;
    std::set<std::string> pseudo_states;

    // ID selector should win over class selector
    auto style = stylesheet.compute_pseudo_element_style(
        CSSPseudoElement::BEFORE, "submit", "button", classes, attributes, pseudo_states);

    REQUIRE(style["content"] == "\"high\"");
    // color from .btn::before should still apply (not overridden)
    REQUIRE(style["color"] == "red");
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("CSS3 Features Integration", "[css3][integration]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1200, 800);

    SECTION("Dashboard layout with grid-template-areas") {
        const char* css = R"(
            #dashboard {
                display: grid;
                grid-template-areas: 
                    "header header header"
                    "nav content sidebar"
                    "footer footer footer";
                grid-template-columns: 200px 1fr 250px;
                grid-template-rows: 60px 1fr 40px;
                width: 100%;
                height: 100%;
            }
            #header { grid-area: header; }
            #nav { grid-area: nav; }
            #content { grid-area: content; }
            #sidebar { grid-area: sidebar; }
            #footer { grid-area: footer; }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* dashboard = cssboxCreateElement(renderer, "dashboard", "div");
        cssboxElement* header = cssboxCreateElement(renderer, "header", "div");
        cssboxElement* nav = cssboxCreateElement(renderer, "nav", "div");
        cssboxElement* content = cssboxCreateElement(renderer, "content", "div");
        cssboxElement* sidebar = cssboxCreateElement(renderer, "sidebar", "div");
        cssboxElement* footer = cssboxCreateElement(renderer, "footer", "div");

        cssboxAppendChild(renderer, dashboard, header);
        cssboxAppendChild(renderer, dashboard, nav);
        cssboxAppendChild(renderer, dashboard, content);
        cssboxAppendChild(renderer, dashboard, sidebar);
        cssboxAppendChild(renderer, dashboard, footer);

        cssboxComputeLayout(renderer);

        // Verify areas are defined
        REQUIRE(dashboard->style.grid_template_areas.size() == 5);

        // Verify item placements
        REQUIRE(header->style.grid_area == "header");
        REQUIRE(nav->style.grid_area == "nav");
        REQUIRE(content->style.grid_area == "content");
        REQUIRE(sidebar->style.grid_area == "sidebar");
        REQUIRE(footer->style.grid_area == "footer");
    }

    SECTION("Text with spacing properties") {
        const char* css = R"(
            .spaced-text {
                letter-spacing: 2px;
                word-spacing: 4px;
                font-size: 14px;
            }
            .tight-text {
                letter-spacing: -0.5px;
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* spaced = cssboxCreateElement(renderer, "spaced", "span");
        spaced->classes.push_back("spaced-text");
        spaced->text_content = "Hello World";

        cssboxElement* tight = cssboxCreateElement(renderer, "tight", "span");
        tight->classes.push_back("tight-text");

        cssboxComputeLayout(renderer);

        REQUIRE_THAT(spaced->style.letter_spacing, WithinAbs(2.0f, 0.1f));
        REQUIRE_THAT(spaced->style.word_spacing, WithinAbs(4.0f, 0.1f));
        REQUIRE_THAT(tight->style.letter_spacing, WithinAbs(-0.5f, 0.1f));
    }

    SECTION("Buttons with ::before/::after icons") {
        const char* css = R"(
            .btn::before {
                content: "[";
                color: gray;
            }
            .btn::after {
                content: "]";
                color: gray;
            }
            .btn {
                color: blue;
                padding: 8px 16px;
            }
        )";

        cssboxParseCSS(renderer, css);

        cssboxElement* btn = cssboxCreateElement(renderer, "btn1", "button");
        btn->classes.push_back("btn");
        btn->text_content = "Click Me";

        cssboxComputeLayout(renderer);

        // Verify main element style (blue color)
        REQUIRE((btn->style.color.b > 0));

        // Verify ::before style
        auto before_style = renderer->stylesheet->compute_pseudo_element_style(
            CSSPseudoElement::BEFORE, btn->id, btn->type, btn->classes,
            btn->attributes, btn->pseudo_states);
        REQUIRE(before_style["content"] == "\"[\"");

        // Verify ::after style
        auto after_style = renderer->stylesheet->compute_pseudo_element_style(
            CSSPseudoElement::AFTER, btn->id, btn->type, btn->classes,
            btn->attributes, btn->pseudo_states);
        REQUIRE(after_style["content"] == "\"]\"");
    }

    cssboxDeleteRenderer(renderer);
}
