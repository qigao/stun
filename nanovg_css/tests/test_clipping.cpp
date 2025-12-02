#include <catch2/catch_test_macros.hpp>
#include <nanovg_css_internal.h>
#include <string>
#include <unordered_map>
#include "test_config.h"

INIT_TEST_LOGGING();

TEST_CASE("SVG Clipping Paths - Data Structure", "[svg][clipping]") {
    SECTION("NVGCSSClipPath creation") {
        NVGCSSClipPath clip;
        clip.id = "clip1";
        
        REQUIRE(clip.id == "clip1");
        REQUIRE(clip.children_internal_ids.empty());
    }
    
    SECTION("ClipPath registry") {
        std::unordered_map<std::string, NVGCSSClipPath> clip_paths;
        
        NVGCSSClipPath clip;
        clip.id = "circle-clip";
        
        clip_paths["circle-clip"] = clip;
        
        REQUIRE(clip_paths.find("circle-clip") != clip_paths.end());
        REQUIRE(clip_paths["circle-clip"].id == "circle-clip");
    }
    
    SECTION("ClipPath with children IDs") {
        NVGCSSClipPath clip;
        clip.id = "rect-clip";
        clip.children_internal_ids.push_back(1);
        clip.children_internal_ids.push_back(2);
        
        REQUIRE(clip.children_internal_ids.size() == 2);
        REQUIRE(clip.children_internal_ids[0] == 1);
        REQUIRE(clip.children_internal_ids[1] == 2);
    }
}

TEST_CASE("SVG Clipping Paths - Properties", "[svg][clipping]") {
    NVGCSSClipPath clip;
    
    SECTION("Default values") {
        REQUIRE(clip.id.empty());
        REQUIRE(clip.children_internal_ids.empty());
    }
    
    SECTION("Custom values") {
        clip.id = "my-clip";
        clip.children_internal_ids = {10, 20, 30};
        
        REQUIRE(clip.id == "my-clip");
        REQUIRE(clip.children_internal_ids.size() == 3);
        REQUIRE(clip.children_internal_ids[0] == 10);
        REQUIRE(clip.children_internal_ids[1] == 20);
        REQUIRE(clip.children_internal_ids[2] == 30);
    }
}

