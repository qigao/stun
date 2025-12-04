#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <catch2/catch_test_macros.hpp>
#include "cssbox_internal.h"
#include "cssbox.h"
#include "test_config.h"

INIT_TEST_LOGGING();

TEST_CASE("SVG Patterns - Data Structure", "[svg][patterns]") {
    SECTION("cssboxPattern creation") {
        cssboxPattern pattern;
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
        std::unordered_map<std::string, cssboxPattern> patterns;
        
        cssboxPattern pattern;
        pattern.id = "stripes";
        
        patterns["stripes"] = pattern;
        
        REQUIRE(patterns.find("stripes") != patterns.end());
        REQUIRE(patterns["stripes"].id == "stripes");
    }
}

TEST_CASE("SVG Patterns - API", "[svg][patterns]") {
    // Mock renderer
    cssboxRenderer renderer(nullptr);
    
    SECTION("cssboxCreatePattern") {
        cssboxElement* pattern_elem = cssboxCreatePattern(&renderer, "grid", 0, 0, 10, 10);
        
        REQUIRE(pattern_elem != nullptr);
        REQUIRE(pattern_elem->type == "pattern");
        REQUIRE(pattern_elem->id == "grid");
        
        // Verify registry update
        REQUIRE(renderer.patterns_.find("grid") != renderer.patterns_.end());
        REQUIRE(renderer.patterns_["grid"].width == 10);
        REQUIRE(renderer.patterns_["grid"].height == 10);
    }
    
    SECTION("Pattern child synchronization") {
        cssboxElement* pattern_elem = cssboxCreatePattern(&renderer, "dots", 0, 0, 20, 20);
        
        // Create a child element
        cssboxElement* circle = cssboxCreateElement(&renderer, "dot", "circle");
        
        // Append child to pattern
        cssboxAppendChild(&renderer, pattern_elem, circle);
        
        // Verify pattern definition updated
        auto& pattern_def = renderer.patterns_["dots"];
        REQUIRE(pattern_def.children_internal_ids.size() == 1);
        REQUIRE(pattern_def.children_internal_ids[0] == circle->internal_id);
        REQUIRE(pattern_def.needs_update == true);
    }
}
