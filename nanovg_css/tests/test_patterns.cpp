#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <catch2/catch_test_macros.hpp>
#include "nanovg_css_internal.h"
#include "nanovg_css.h"
#include "test_config.h"

INIT_TEST_LOGGING();

TEST_CASE("SVG Patterns - Data Structure", "[svg][patterns]") {
    SECTION("NVGCSSPattern creation") {
        NVGCSSPattern pattern;
        pattern.id = "dots";
        pattern.x = 0;
        pattern.y = 0;
        pattern.width = 20;
        pattern.height = 20;
        
        REQUIRE(pattern.id == "dots");
        REQUIRE(pattern.width == 20);
        REQUIRE(pattern.height == 20);
        REQUIRE(pattern.children_internal_ids.empty());
        REQUIRE(pattern.needs_update == true);
        REQUIRE(pattern.image_handle == -1);
    }
    
    SECTION("Pattern registry") {
        std::unordered_map<std::string, NVGCSSPattern> patterns;
        
        NVGCSSPattern pattern;
        pattern.id = "stripes";
        
        patterns["stripes"] = pattern;
        
        REQUIRE(patterns.find("stripes") != patterns.end());
        REQUIRE(patterns["stripes"].id == "stripes");
    }
}

TEST_CASE("SVG Patterns - API", "[svg][patterns]") {
    // Mock renderer
    NVGCSSRenderer renderer(nullptr);
    
    SECTION("nvgcssCreatePattern") {
        NVGCSSElement* pattern_elem = nvgcssCreatePattern(&renderer, "grid", 0, 0, 10, 10);
        
        REQUIRE(pattern_elem != nullptr);
        REQUIRE(pattern_elem->type == "pattern");
        REQUIRE(pattern_elem->id == "grid");
        
        // Verify registry update
        REQUIRE(renderer.patterns_.find("grid") != renderer.patterns_.end());
        REQUIRE(renderer.patterns_["grid"].width == 10);
        REQUIRE(renderer.patterns_["grid"].height == 10);
    }
    
    SECTION("Pattern child synchronization") {
        NVGCSSElement* pattern_elem = nvgcssCreatePattern(&renderer, "dots", 0, 0, 20, 20);
        
        // Create a child element
        NVGCSSElement* circle = nvgcssCreateElement(&renderer, "dot", "circle");
        
        // Append child to pattern
        nvgcssAppendChild(&renderer, pattern_elem, circle);
        
        // Verify pattern definition updated
        auto& pattern_def = renderer.patterns_["dots"];
        REQUIRE(pattern_def.children_internal_ids.size() == 1);
        REQUIRE(pattern_def.children_internal_ids[0] == circle->internal_id);
        REQUIRE(pattern_def.needs_update == true);
    }
}
