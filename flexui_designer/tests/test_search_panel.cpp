/*
 * flexUI Designer - Search Panel Property Tests
 *
 * Property 4: Search Correctness and Completeness
 * For any widget list and any non-empty query string Q:
 * - (Correctness) All widgets in results contain Q (case-insensitive) in ID or text
 * - (Completeness) All widgets containing Q in ID or text are in results
 *
 * **Validates: Requirements 2.2, 2.3**
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "flexui_designer/search_panel.h"
#include "flexui_designer/designer.h"

#include <random>
#include <algorithm>
#include <cctype>

using namespace flexui_designer;

// Helper to convert string to lowercase
static std::string to_lower(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

// Helper to check if string contains substring (case-insensitive)
static bool contains_ci(const std::string& haystack, const std::string& needle) {
    return to_lower(haystack).find(to_lower(needle)) != std::string::npos;
}

// Helper to create a widget with specific ID and text
static DesignWidget make_widget(const std::string& id, const std::string& text) {
    DesignWidget w;
    w.type = WidgetType::Button;
    w.id = id;
    w.text = text;
    w.x = 0;
    w.y = 0;
    w.width = 100;
    w.height = 30;
    return w;
}

// ============================================================================
// Property 4: Search Correctness and Completeness
// ============================================================================

TEST_CASE("Property 4: Search Correctness - all results match query",
          "[search][property]") {
    std::vector<DesignWidget> widgets = {
        make_widget("button_1", "Submit"),
        make_widget("button_2", "Cancel"),
        make_widget("label_title", "Welcome"),
        make_widget("input_name", "Enter name"),
        make_widget("submit_btn", "OK")
    };
    
    SearchPanel panel;
    panel.set_widgets(&widgets);
    
    SECTION("Search for 'button' matches IDs") {
        panel.handle_text_input("button");
        auto& results = panel.results();
        
        // All results must contain "button" in ID or text
        for (const auto* w : results) {
            bool matches = contains_ci(w->id, "button") || contains_ci(w->text, "button");
            INFO("Widget ID: " << w->id << ", Text: " << w->text);
            REQUIRE(matches);
        }
    }
    
    SECTION("Search for 'submit' matches both ID and text") {
        panel.handle_text_input("submit");
        auto& results = panel.results();
        
        for (const auto* w : results) {
            bool matches = contains_ci(w->id, "submit") || contains_ci(w->text, "submit");
            INFO("Widget ID: " << w->id << ", Text: " << w->text);
            REQUIRE(matches);
        }
    }
    
    SECTION("Case-insensitive: 'BUTTON' matches 'button'") {
        panel.handle_text_input("BUTTON");
        auto& results = panel.results();
        
        for (const auto* w : results) {
            bool matches = contains_ci(w->id, "button") || contains_ci(w->text, "button");
            REQUIRE(matches);
        }
    }
}

TEST_CASE("Property 4: Search Completeness - no matching widget is missing",
          "[search][property]") {
    std::vector<DesignWidget> widgets = {
        make_widget("button_1", "Submit"),
        make_widget("button_2", "Cancel"),
        make_widget("label_title", "Welcome"),
        make_widget("input_name", "Enter name"),
        make_widget("submit_btn", "OK")
    };
    
    SearchPanel panel;
    panel.set_widgets(&widgets);
    
    SECTION("All widgets with 'button' in ID are found") {
        panel.handle_text_input("button");
        auto& results = panel.results();
        
        // Count expected matches
        int expected_count = 0;
        for (const auto& w : widgets) {
            if (contains_ci(w.id, "button") || contains_ci(w.text, "button")) {
                expected_count++;
                
                // Verify this widget is in results
                bool found = std::any_of(results.begin(), results.end(),
                    [&](const DesignWidget* r) { return r->id == w.id; });
                INFO("Expected widget: " << w.id);
                REQUIRE(found);
            }
        }
        
        REQUIRE(results.size() == expected_count);
    }
    
    SECTION("All widgets with 'name' in ID or text are found") {
        panel.handle_text_input("name");
        auto& results = panel.results();
        
        for (const auto& w : widgets) {
            if (contains_ci(w.id, "name") || contains_ci(w.text, "name")) {
                bool found = std::any_of(results.begin(), results.end(),
                    [&](const DesignWidget* r) { return r->id == w.id; });
                INFO("Expected widget: " << w.id << " with text: " << w.text);
                REQUIRE(found);
            }
        }
    }
}

// ============================================================================
// Property-based test using Catch2 GENERATE for multiple iterations
// ============================================================================

TEST_CASE("Property 4: Search Correctness and Completeness - randomized iterations",
          "[search][property][pbt]") {
    // Run 100 iterations with different random seeds
    auto seed = GENERATE(range(1, 101));
    
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> widget_count_dist(1, 20);
    std::uniform_int_distribution<int> char_dist(0, 25);
    std::uniform_int_distribution<int> len_dist(3, 10);
    
    // Generate random string
    auto random_string = [&](int len) {
        std::string s;
        for (int i = 0; i < len; ++i) {
            s += 'a' + char_dist(rng);
        }
        return s;
    };
    
    // Generate random widgets
    int widget_count = widget_count_dist(rng);
    std::vector<DesignWidget> widgets;
    for (int i = 0; i < widget_count; ++i) {
        std::string id = random_string(len_dist(rng));
        std::string text = random_string(len_dist(rng));
        widgets.push_back(make_widget(id, text));
    }
    
    // Generate random query (2-4 chars to have reasonable match probability)
    std::uniform_int_distribution<int> query_len_dist(2, 4);
    std::string query = random_string(query_len_dist(rng));
    
    SearchPanel panel;
    panel.set_widgets(&widgets);
    panel.handle_text_input(query.c_str());
    
    auto& results = panel.results();
    
    // Correctness: all results match
    for (const auto* w : results) {
        bool matches = contains_ci(w->id, query) || contains_ci(w->text, query);
        INFO("Seed: " << seed << ", Query: " << query << ", Widget ID: " << w->id << ", Text: " << w->text);
        REQUIRE(matches);
    }
    
    // Completeness: no matching widget is missing
    for (const auto& w : widgets) {
        bool should_match = contains_ci(w.id, query) || contains_ci(w.text, query);
        if (should_match) {
            bool found = std::any_of(results.begin(), results.end(),
                [&](const DesignWidget* r) { return r->id == w.id; });
            INFO("Seed: " << seed << ", Query: " << query << ", Missing widget: " << w.id);
            REQUIRE(found);
        }
    }
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_CASE("Search: empty query returns no results", "[search][edge]") {
    std::vector<DesignWidget> widgets = {
        make_widget("button_1", "Submit"),
        make_widget("label_1", "Title")
    };
    
    SearchPanel panel;
    panel.set_widgets(&widgets);
    
    // Don't input anything
    REQUIRE(panel.results().empty());
}

TEST_CASE("Search: no matches returns empty results", "[search][edge]") {
    std::vector<DesignWidget> widgets = {
        make_widget("button_1", "Submit"),
        make_widget("label_1", "Title")
    };
    
    SearchPanel panel;
    panel.set_widgets(&widgets);
    panel.handle_text_input("xyz123");
    
    REQUIRE(panel.results().empty());
}

TEST_CASE("Search: empty widget list returns no results", "[search][edge]") {
    std::vector<DesignWidget> widgets;
    
    SearchPanel panel;
    panel.set_widgets(&widgets);
    panel.handle_text_input("button");
    
    REQUIRE(panel.results().empty());
}

TEST_CASE("Search: partial match works", "[search][edge]") {
    std::vector<DesignWidget> widgets = {
        make_widget("submit_button", "Click to submit"),
        make_widget("cancel_button", "Cancel action")
    };
    
    SearchPanel panel;
    panel.set_widgets(&widgets);
    panel.handle_text_input("sub");
    
    auto& results = panel.results();
    REQUIRE(results.size() == 1);
    REQUIRE(results[0]->id == "submit_button");
}

// ============================================================================
// Keyboard navigation tests
// ============================================================================

TEST_CASE("Search: keyboard navigation bounds", "[search][navigation]") {
    std::vector<DesignWidget> widgets = {
        make_widget("button_1", "First"),
        make_widget("button_2", "Second"),
        make_widget("button_3", "Third")
    };
    
    SearchPanel panel;
    panel.set_widgets(&widgets);
    panel.handle_text_input("button");
    
    REQUIRE(panel.results().size() == 3);
    REQUIRE(panel.selected_index() == 0);
    
    SECTION("Down arrow navigates forward") {
        panel.handle_key(0x28);  // Down
        REQUIRE(panel.selected_index() == 1);
        
        panel.handle_key(0x28);  // Down
        REQUIRE(panel.selected_index() == 2);
    }
    
    SECTION("Down arrow at end stays at end") {
        panel.handle_key(0x28);  // Down to 1
        panel.handle_key(0x28);  // Down to 2
        panel.handle_key(0x28);  // Down - should stay at 2
        REQUIRE(panel.selected_index() == 2);
    }
    
    SECTION("Up arrow at start stays at start") {
        panel.handle_key(0x26);  // Up - should stay at 0
        REQUIRE(panel.selected_index() == 0);
    }
    
    SECTION("Up arrow navigates backward") {
        panel.handle_key(0x28);  // Down to 1
        panel.handle_key(0x28);  // Down to 2
        panel.handle_key(0x26);  // Up to 1
        REQUIRE(panel.selected_index() == 1);
    }
}

TEST_CASE("Search: backspace removes characters", "[search][input]") {
    std::vector<DesignWidget> widgets = {
        make_widget("button_1", "Submit"),
        make_widget("label_1", "Title")
    };
    
    SearchPanel panel;
    panel.set_widgets(&widgets);
    
    panel.handle_text_input("button");
    REQUIRE(panel.query() == "button");
    REQUIRE(panel.results().size() == 1);
    
    panel.handle_key('\b');  // Backspace
    REQUIRE(panel.query() == "butto");
    
    panel.handle_key('\b');
    panel.handle_key('\b');
    panel.handle_key('\b');
    panel.handle_key('\b');
    panel.handle_key('\b');
    REQUIRE(panel.query().empty());
    REQUIRE(panel.results().empty());
}

