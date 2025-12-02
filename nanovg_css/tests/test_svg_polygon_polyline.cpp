#include <catch2/catch_test_macros.hpp>
#include <nanovg.h>
#include <nanovg_css.h>
#include "nanovg_css_internal.h"

TEST_CASE("SVG Polygon and Polyline", "[svg]") {
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);

    SECTION("Create polygon element") {
        NVGCSSElement* elem = nvgcssCreateElement(renderer, "poly1", "polygon");
        REQUIRE(elem != nullptr);
        REQUIRE(elem->type == "polygon");
        
        // Set points attribute via inline style
        elem->inline_style["points"] = "100,100 200,200 100,200";
        
        // Verify attribute is set
        REQUIRE(elem->inline_style["points"] == "100,100 200,200 100,200");
    }

    SECTION("Create polyline element") {
        NVGCSSElement* elem = nvgcssCreateElement(renderer, "line1", "polyline");
        REQUIRE(elem != nullptr);
        REQUIRE(elem->type == "polyline");
        
        elem->inline_style["points"] = "10,10 20,20 30,10";
        REQUIRE(elem->inline_style["points"] == "10,10 20,20 30,10");
    }
    
    SECTION("Polygon with fill and stroke") {
        NVGCSSElement* elem = nvgcssCreateElement(renderer, "poly2", "polygon");
        elem->inline_style["points"] = "0,0 10,0 10,10 0,10";
        elem->inline_style["fill"] = "red";
        elem->inline_style["stroke"] = "black";
        elem->inline_style["stroke-width"] = "2px";
        
        REQUIRE(elem->inline_style["fill"] == "red");
        REQUIRE(elem->inline_style["stroke"] == "black");
    }

    nvgcssDeleteRenderer(renderer);
}

