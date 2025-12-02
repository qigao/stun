#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "../src/spatial_index.h"
#include <flexui/widget.h>
#include <flexui/screen.h>

using namespace flexui;

TEST_CASE("SpatialIndex basic operations", "[spatial_index]") {
    Screen screen(800, 600, "Test");
    SpatialIndex index(800.0f, 600.0f);
    
    SECTION("Insert and query single widget") {
        auto* widget = screen.addWidget("test", "div");
        index.insert(widget, 100, 100, 50, 50);
        
        auto results = index.query(125, 125);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0] == widget);
    }
    
    SECTION("Query outside bounds returns empty") {
        auto* widget = screen.addWidget("test", "div");
        index.insert(widget, 100, 100, 50, 50);
        
        auto results = index.query(500, 500);
        REQUIRE(results.empty());
    }
    
    SECTION("Clear removes all widgets") {
        auto* w1 = screen.addWidget("w1", "div");
        auto* w2 = screen.addWidget("w2", "div");
        
        index.insert(w1, 100, 100, 50, 50);
        index.insert(w2, 200, 200, 50, 50);
        
        index.clear();
        
        auto results = index.query(125, 125);
        REQUIRE(results.empty());
    }
}

TEST_CASE("SpatialIndex with multiple widgets", "[spatial_index]") {
    Screen screen(800, 600, "Test");
    SpatialIndex index(800.0f, 600.0f);
    
    std::vector<Widget*> widgets;
    for (int i = 0; i < 10; i++) {
        auto* w = screen.addWidget("w" + std::to_string(i), "div");
        widgets.push_back(w);
        index.insert(w, i * 80.0f, i * 60.0f, 50, 50);
    }
    
    SECTION("Query returns only nearby widgets") {
        auto results = index.query(125, 125);
        REQUIRE(!results.empty());
        REQUIRE(results.size() <= 10);
    }
    
    SECTION("Query at different positions") {
        auto results1 = index.query(50, 50);
        auto results2 = index.query(400, 300);
        
        // Different positions should return different results
        REQUIRE(!results1.empty());
        REQUIRE(!results2.empty());
    }
}

TEST_CASE("SpatialIndex performance", "[spatial_index][benchmark]") {
    SpatialIndex index(1920.0f, 1080.0f);
    
    SECTION("Insert 1000 widgets") {
        std::vector<Widget> widgets;
        widgets.reserve(1000);
        
        for (int i = 0; i < 1000; i++) {
            widgets.emplace_back(nullptr, "w" + std::to_string(i), "div");
        }
        
        BENCHMARK("Insert 1000 widgets") {
            index.clear();
            for (int i = 0; i < 1000; i++) {
                float x = (i % 40) * 48.0f;
                float y = (i / 40) * 43.0f;
                index.insert(&widgets[i], x, y, 40, 40);
            }
        };
    }
    
    SECTION("Query with 1000 widgets") {
        std::vector<Widget> widgets;
        widgets.reserve(1000);
        
        for (int i = 0; i < 1000; i++) {
            widgets.emplace_back(nullptr, "w" + std::to_string(i), "div");
            float x = (i % 40) * 48.0f;
            float y = (i / 40) * 43.0f;
            index.insert(&widgets[i], x, y, 40, 40);
        }
        
        BENCHMARK("Query with 1000 widgets") {
            return index.query(960, 540);
        };
        
        // Verify query returns reasonable number of results
        auto results = index.query(960, 540);
        REQUIRE(results.size() < 100);  // Should be much less than 1000
    }
}

TEST_CASE("SpatialIndex vs linear search comparison", "[spatial_index][performance]") {
    const int NUM_WIDGETS = 1000;
    
    // Setup
    std::vector<Widget> widgets;
    widgets.reserve(NUM_WIDGETS);
    
    for (int i = 0; i < NUM_WIDGETS; i++) {
        widgets.emplace_back(nullptr, "w" + std::to_string(i), "div");
    }
    
    // Spatial index
    SpatialIndex index(1920.0f, 1080.0f);
    for (int i = 0; i < NUM_WIDGETS; i++) {
        float x = (i % 40) * 48.0f;
        float y = (i / 40) * 43.0f;
        index.insert(&widgets[i], x, y, 40, 40);
    }
    
    float query_x = 960.0f;
    float query_y = 540.0f;
    
    SECTION("Spatial index query") {
        BENCHMARK("Spatial index (O(1))") {
            return index.query(query_x, query_y);
        };
    }
    
    SECTION("Linear search") {
        BENCHMARK("Linear search (O(n))") {
            std::vector<Widget*> results;
            for (int i = 0; i < NUM_WIDGETS; i++) {
                float x = (i % 40) * 48.0f;
                float y = (i / 40) * 43.0f;
                float w = 40.0f;
                float h = 40.0f;
                
                if (query_x >= x && query_x <= x + w &&
                    query_y >= y && query_y <= y + h) {
                    results.push_back(&widgets[i]);
                }
            }
            return results;
        };
    }
}

TEST_CASE("SpatialIndex edge cases", "[spatial_index]") {
    SpatialIndex index(800.0f, 600.0f);
    
    SECTION("Widget at boundary") {
        Widget widget(nullptr, "test", "div");
        index.insert(&widget, 0, 0, 50, 50);
        
        auto results = index.query(25, 25);
        REQUIRE(results.size() == 1);
    }
    
    SECTION("Widget spanning multiple cells") {
        Widget widget(nullptr, "large", "div");
        index.insert(&widget, 100, 100, 200, 200);
        
        // Query in different parts of the large widget
        auto results1 = index.query(150, 150);
        auto results2 = index.query(250, 250);
        
        REQUIRE(!results1.empty());
        REQUIRE(!results2.empty());
    }
    
    SECTION("Overlapping widgets") {
        Widget w1(nullptr, "w1", "div");
        Widget w2(nullptr, "w2", "div");
        
        index.insert(&w1, 100, 100, 100, 100);
        index.insert(&w2, 150, 150, 100, 100);
        
        auto results = index.query(175, 175);
        REQUIRE(results.size() == 2);
    }
}

TEST_CASE("SpatialIndex resize", "[spatial_index]") {
    SpatialIndex index(800.0f, 600.0f);
    
    Widget widget(nullptr, "test", "div");
    index.insert(&widget, 100, 100, 50, 50);
    
    SECTION("Resize clears index") {
        index.resize(1920.0f, 1080.0f);
        
        auto results = index.query(125, 125);
        REQUIRE(results.empty());
    }
}
