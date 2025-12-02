#include <catch2/catch_test_macros.hpp>
#include "../src/spatial_index.h"

using namespace flexui;

// Simple mock widget for testing
struct MockWidget {
    std::string id;
    MockWidget(const std::string& id_) : id(id_) {}
};

TEST_CASE("SpatialIndex basic operations", "[spatial_index]") {
    SpatialIndex index(800.0f, 600.0f);
    
    SECTION("Insert and query single item") {
        MockWidget widget("test");
        index.insert((Widget*)&widget, 100, 100, 50, 50);
        
        auto results = index.query(125, 125);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0] == (Widget*)&widget);
    }
    
    SECTION("Query outside bounds returns empty") {
        MockWidget widget("test");
        index.insert((Widget*)&widget, 100, 100, 50, 50);
        
        auto results = index.query(500, 500);
        REQUIRE(results.empty());
    }
    
    SECTION("Clear removes all items") {
        MockWidget w1("w1");
        MockWidget w2("w2");
        
        index.insert((Widget*)&w1, 100, 100, 50, 50);
        index.insert((Widget*)&w2, 200, 200, 50, 50);
        
        index.clear();
        
        auto results = index.query(125, 125);
        REQUIRE(results.empty());
    }
}

TEST_CASE("SpatialIndex with multiple items", "[spatial_index]") {
    SpatialIndex index(800.0f, 600.0f);
    
    std::vector<MockWidget> widgets;
    for (int i = 0; i < 10; i++) {
        widgets.emplace_back("w" + std::to_string(i));
        // Insert at positions: (0,0), (80,60), (160,120), etc.
        index.insert((Widget*)&widgets[i], i * 80.0f, i * 60.0f, 50, 50);
    }
    
    SECTION("Query returns only nearby items") {
        // Query at (25, 25) - should hit first widget at (0,0) with size (50,50)
        auto results = index.query(25, 25);
        REQUIRE(!results.empty());
        REQUIRE(results.size() <= 10);
    }
    
    SECTION("Query at different positions") {
        // Query at (25, 25) - should hit first widget at (0,0)
        auto results1 = index.query(25, 25);
        // Query at (105, 85) - should hit second widget at (80,60)
        auto results2 = index.query(105, 85);
        
        REQUIRE(!results1.empty());
        REQUIRE(!results2.empty());
    }
}

TEST_CASE("SpatialIndex edge cases", "[spatial_index]") {
    SpatialIndex index(800.0f, 600.0f);
    
    SECTION("Item at boundary") {
        MockWidget widget("test");
        index.insert((Widget*)&widget, 0, 0, 50, 50);
        
        auto results = index.query(25, 25);
        REQUIRE(results.size() == 1);
    }
    
    SECTION("Item spanning multiple cells") {
        MockWidget widget("large");
        index.insert((Widget*)&widget, 100, 100, 200, 200);
        
        auto results1 = index.query(150, 150);
        auto results2 = index.query(250, 250);
        
        REQUIRE(!results1.empty());
        REQUIRE(!results2.empty());
    }
    
    SECTION("Overlapping items") {
        MockWidget w1("w1");
        MockWidget w2("w2");
        
        index.insert((Widget*)&w1, 100, 100, 100, 100);
        index.insert((Widget*)&w2, 150, 150, 100, 100);
        
        auto results = index.query(175, 175);
        REQUIRE(results.size() == 2);
    }
}

TEST_CASE("SpatialIndex resize", "[spatial_index]") {
    SpatialIndex index(800.0f, 600.0f);
    
    MockWidget widget("test");
    index.insert((Widget*)&widget, 100, 100, 50, 50);
    
    SECTION("Resize clears index") {
        index.resize(1920.0f, 1080.0f);
        
        auto results = index.query(125, 125);
        REQUIRE(results.empty());
    }
}

TEST_CASE("SpatialIndex performance characteristics", "[spatial_index]") {
    SpatialIndex index(1920.0f, 1080.0f);
    
    SECTION("Handle many items") {
        std::vector<MockWidget> widgets;
        widgets.reserve(1000);
        
        for (int i = 0; i < 1000; i++) {
            widgets.emplace_back("w" + std::to_string(i));
            float x = (i % 40) * 48.0f;
            float y = (i / 40) * 43.0f;
            index.insert((Widget*)&widgets[i], x, y, 40, 40);
        }
        
        // Query should return reasonable number of results
        auto results = index.query(960, 540);
        REQUIRE(results.size() < 100);  // Much less than 1000
        REQUIRE(!results.empty());
    }
}
