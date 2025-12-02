#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <nanovg.h>
#include <nanovg_css.h>
#include <string>

using Catch::Matchers::WithinAbs;

std::string get_style(NVGCSSRenderer* renderer, NVGCSSElement* element, const char* prop) {
    char buf[64];
    if (nvgcssGetComputedStyle(renderer, element, prop, buf, sizeof(buf))) {
        return std::string(buf);
    }
    return "";
}

TEST_CASE("Fluent Dashboard Layout", "[flexui][fluent][dashboard]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 1200, 800);
    
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
        
        nvgcssParseCSS(renderer, css);
        
        NVGCSSElement* row = nvgcssCreateElement(renderer, "row", "div");
        nvgcssAddClass(row, "form-row");
        
        NVGCSSElement* label = nvgcssCreateElement(renderer, "label", "label");
        nvgcssAddClass(label, "form-label");
        nvgcssAppendChild(row, label);
        
        NVGCSSElement* sw = nvgcssCreateElement(renderer, "switch", "switch");
        nvgcssAppendChild(row, sw);
        
        nvgcssComputeLayout(renderer);
        
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
        
        nvgcssParseCSS(renderer, css);
        
        NVGCSSElement* container = nvgcssCreateElement(renderer, "container", "div");
        nvgcssAddClass(container, "table-container");
        
        NVGCSSElement* progress = nvgcssCreateElement(renderer, "progress1", "progressbar");
        nvgcssAppendChild(container, progress);
        
        nvgcssComputeLayout(renderer);
        
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
        
        nvgcssParseCSS(renderer, css);
        
        NVGCSSElement* row = nvgcssCreateElement(renderer, "row", "div");
        nvgcssAddClass(row, "widget-row");
        
        NVGCSSElement* label = nvgcssCreateElement(renderer, "label", "label");
        nvgcssAddClass(label, "widget-label");
        nvgcssAppendChild(row, label);
        
        nvgcssComputeLayout(renderer);
        
        // Verify flexbox layout
        REQUIRE(get_style(renderer, row, "display") == "flex");
        REQUIRE(get_style(renderer, row, "flex-direction") == "row");
        REQUIRE(get_style(renderer, row, "gap") == "20px");
    }
    
    nvgcssDeleteRenderer(renderer);
}
