#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <nanovg.h>
#include <cssbox.h>
#include <string>

using Catch::Matchers::WithinAbs;

std::string get_style(cssboxRenderer* renderer, cssboxElement* element, const char* prop) {
    char buf[64];
    if (cssboxGetComputedStyle(renderer, element, prop, buf, sizeof(buf))) {
        return std::string(buf);
    }
    return "";
}

TEST_CASE("Fluent Dashboard Layout", "[flexui][fluent][dashboard]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 1200, 800);
    
    SECTION("Form Row with Switch") {
        const char* css = R"(
            .form-row {
                display: flex;
                flex-direction: row;
                width: 100%;
                height: 56px;
                gap: 20px;
            }
            .form-label {
                width: 200px;
                height: 56px;
            }
            switch {
                width: 52px;
                height: 28px;
            }
        )";
        
        cssboxParseCSS(renderer, css);
        
        cssboxElement* row = cssboxCreateElement(renderer, "row", "div");
        cssboxAddClass(row, "form-row");
        
        cssboxElement* label = cssboxCreateElement(renderer, "label", "label");
        cssboxAddClass(label, "form-label");
        cssboxAppendChild(row, label);
        
        cssboxElement* sw = cssboxCreateElement(renderer, "switch", "switch");
        cssboxAppendChild(row, sw);
        
        cssboxComputeLayout(renderer);
        
        // Check row layout
        REQUIRE(get_style(renderer, row, "display") == "flex");
        REQUIRE(get_style(renderer, row, "flex-direction") == "row");
        REQUIRE(get_style(renderer, row, "width") == "100%");
        REQUIRE(get_style(renderer, row, "height") == "56px");
        
        // Check label dimensions
        REQUIRE(get_style(renderer, label, "width") == "200px");
        REQUIRE(get_style(renderer, label, "height") == "56px");
        
        // Check switch dimensions
        REQUIRE(get_style(renderer, sw, "width") == "52px");
        REQUIRE(get_style(renderer, sw, "height") == "28px");
    }
    
    SECTION("Progress Bar Full Width") {
        const char* css = R"(
            .table-container {
                display: flex;
                flex-direction: column;
                width: 100%;
            }
            #progress1 {
                width: 100%;
                height: 12px;
            }
        )";
        
        cssboxParseCSS(renderer, css);
        
        cssboxElement* container = cssboxCreateElement(renderer, "container", "div");
        cssboxAddClass(container, "table-container");
        
        cssboxElement* progress = cssboxCreateElement(renderer, "progress1", "progressbar");
        cssboxAppendChild(container, progress);
        
        cssboxComputeLayout(renderer);
        
        // Check progress bar takes full width
        REQUIRE(get_style(renderer, progress, "width") == "100%");
        REQUIRE(get_style(renderer, progress, "height") == "12px");
    }
    
    SECTION("Widget Row Layout") {
        const char* css = R"(
            .widget-row {
                display: flex;
                flex-direction: row;
                width: 100%;
                height: 60px;
                gap: 20px;
            }
            .widget-label {
                width: 120px;
                height: 60px;
            }
        )";
        
        cssboxParseCSS(renderer, css);
        
        cssboxElement* row = cssboxCreateElement(renderer, "row", "div");
        cssboxAddClass(row, "widget-row");
        
        cssboxElement* label = cssboxCreateElement(renderer, "label", "label");
        cssboxAddClass(label, "widget-label");
        cssboxAppendChild(row, label);
        
        cssboxComputeLayout(renderer);
        
        // Verify flexbox layout
        REQUIRE(get_style(renderer, row, "display") == "flex");
        REQUIRE(get_style(renderer, row, "flex-direction") == "row");
        REQUIRE(get_style(renderer, row, "gap") == "20px");
    }
    
    cssboxDeleteRenderer(renderer);
}
