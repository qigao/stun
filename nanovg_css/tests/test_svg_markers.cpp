#include <catch2/catch_test_macros.hpp>
#include <nanovg_css_internal.h>
#include <string>
#include <unordered_map>
#include "test_config.h"

INIT_TEST_LOGGING();

TEST_CASE("SVG Markers - Data Structure", "[svg][markers]") {
    SECTION("NVGCSSMarker creation") {
        NVGCSSMarker marker;
        marker.id = "arrow";
        marker.markerWidth = 10.0f;
        marker.markerHeight = 10.0f;
        marker.refX = 5.0f;
        marker.refY = 5.0f;
        marker.orient = "auto";
        
        REQUIRE(marker.id == "arrow");
        REQUIRE(marker.markerWidth == 10.0f);
        REQUIRE(marker.markerHeight == 10.0f);
        REQUIRE(marker.refX == 5.0f);
        REQUIRE(marker.refY == 5.0f);
        REQUIRE(marker.orient == "auto");
    }
    
    SECTION("Marker registry") {
        std::unordered_map<std::string, NVGCSSMarker> markers;
        
        NVGCSSMarker arrow;
        arrow.id = "arrow";
        arrow.markerWidth = 10.0f;
        arrow.orient = "auto";
        
        markers["arrow"] = arrow;
        
        REQUIRE(markers.find("arrow") != markers.end());
        REQUIRE(markers["arrow"].id == "arrow");
        REQUIRE(markers["arrow"].markerWidth == 10.0f);
    }
    
    SECTION("Marker with children IDs") {
        NVGCSSMarker marker;
        marker.id = "dot";
        marker.children_internal_ids.push_back(1);
        marker.children_internal_ids.push_back(2);
        
        REQUIRE(marker.children_internal_ids.size() == 2);
        REQUIRE(marker.children_internal_ids[0] == 1);
        REQUIRE(marker.children_internal_ids[1] == 2);
    }
}

TEST_CASE("SVG Markers - Orientation modes", "[svg][markers]") {
    SECTION("Auto orientation") {
        NVGCSSMarker m;
        m.orient = "auto";
        REQUIRE(m.orient == "auto");
    }
    
    SECTION("Auto-start-reverse orientation") {
        NVGCSSMarker m;
        m.orient = "auto-start-reverse";
        REQUIRE(m.orient == "auto-start-reverse");
    }
    
    SECTION("Fixed angle orientation") {
        NVGCSSMarker m;
        m.orient = "45";
        REQUIRE(m.orient == "45");
    }
    
    SECTION("Default orientation") {
        NVGCSSMarker m;
        REQUIRE(m.orient == "0");  // Default from struct definition
    }
}

TEST_CASE("SVG Markers - Marker properties", "[svg][markers]") {
    NVGCSSMarker marker;
    
    SECTION("Default values") {
        REQUIRE(marker.markerWidth == 3.0f);
        REQUIRE(marker.markerHeight == 3.0f);
        REQUIRE(marker.refX == 0.0f);
        REQUIRE(marker.refY == 0.0f);
        REQUIRE(marker.orient == "0");
    }
    
    SECTION("Custom values") {
        marker.markerWidth = 20.0f;
        marker.markerHeight = 15.0f;
        marker.refX = 10.0f;
        marker.refY = 7.5f;
        
        REQUIRE(marker.markerWidth == 20.0f);
        REQUIRE(marker.markerHeight == 15.0f);
        REQUIRE(marker.refX == 10.0f);
        REQUIRE(marker.refY == 7.5f);
    }
}

