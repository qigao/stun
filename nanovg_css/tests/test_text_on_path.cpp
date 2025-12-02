#include <catch2/catch_test_macros.hpp>
#include "nanovg_css_internal.h"
#include "nanovg_css.h"
#include "test_config.h"

INIT_TEST_LOGGING();

TEST_CASE("SVG Text on Path", "[svg][textpath]") {
    // Mock renderer
    NVGCSSRenderer renderer(nullptr);
    
    SECTION("Setup elements") {
        // Create path
        NVGCSSElement* path = nvgcssCreateElement(&renderer, "curve1", "path");
        path->inline_style["d"] = "M 10 10 C 20 20 40 20 50 10";
        
        // Create textPath
        NVGCSSElement* textPath = nvgcssCreateElement(&renderer, "tp1", "textPath");
        textPath->attributes["href"] = "#curve1";
        textPath->text_content = "Hello Path";
        
        REQUIRE(textPath->type == "textPath");
        REQUIRE(textPath->text_content == "Hello Path");
        
        // Verify path lookup logic (manual check since paint_text_path is private)
        std::string href = textPath->attributes["href"];
        std::string path_id = href.substr(1); // Remove #
        REQUIRE(path_id == "curve1");
        
        NVGCSSElement* found_path = nvgcssGetElement(&renderer, path_id.c_str());
        REQUIRE(found_path == path);
    }
    
    SECTION("StartOffset parsing") {
        NVGCSSElement* textPath = nvgcssCreateElement(&renderer, "tp2", "textPath");
        
        // Percentage
        textPath->attributes["startOffset"] = "50%";
        float total_len = 100.0f;
        float offset = nvgcss_utils::parse_length(textPath->attributes["startOffset"], total_len);
        REQUIRE(offset == 50.0f);
        
        // Absolute
        textPath->attributes["startOffset"] = "20px";
        offset = nvgcss_utils::parse_length(textPath->attributes["startOffset"], total_len);
        REQUIRE(offset == 20.0f);
    }
}
