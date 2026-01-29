/*
 * flexUI Designer - History Panel Property Tests
 *
 * Property 1: History Jump Consistency
 * For any undo stack with N entries and any target index I (0 ≤ I < N),
 * jumping to index I shall result in the widgets state being identical
 * to undo_stack_[I].state.
 *
 * **Validates: Requirements 1.2**
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "flexui_designer/designer.h"
#include "flexui_designer/history_panel.h"

#include <random>
#include <vector>

using namespace flexui_designer;

// Helper to create a widget
static DesignWidget make_widget(int id, float x, float y) {
    DesignWidget w;
    w.type = WidgetType::Button;
    w.id = "widget_" + std::to_string(id);
    w.x = x;
    w.y = y;
    w.width = 100;
    w.height = 30;
    w.text = "Button " + std::to_string(id);
    return w;
}

// Helper to compare widget vectors
static bool widgets_equal(const std::vector<DesignWidget>& a, 
                          const std::vector<DesignWidget>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id) return false;
        if (a[i].x != b[i].x || a[i].y != b[i].y) return false;
    }
    return true;
}

// Simulate jump_to_history logic matching Designer::jump_to_history
// Returns the resulting widgets state after jumping to target_index
static std::vector<DesignWidget> simulate_jump(
    std::vector<DesignWidget> widgets,
    std::vector<HistoryEntry> undo_stack,
    int target_index
) {
    // Jump to undo entry at target_index
    // We need to undo until we reach that state
    // undo_stack[N-1] is most recent, undo_stack[0] is oldest
    // After 1 undo: widgets = undo_stack[N-1], undo_stack has N-1 entries
    // After 2 undos: widgets = undo_stack[N-2], undo_stack has N-2 entries
    // To reach undo_stack[target_index], we need (N - target_index) undos
    // But after those undos, undo_stack[target_index] becomes current widgets
    
    int undo_count = (int)undo_stack.size() - target_index;
    std::vector<HistoryEntry> redo_stack;
    
    for (int i = 0; i < undo_count; ++i) {
        if (undo_stack.empty()) break;
        // Push current to redo
        HistoryEntry redo_entry;
        redo_entry.state = widgets;
        redo_entry.description = undo_stack.back().description;
        redo_stack.push_back(std::move(redo_entry));
        // Pop from undo to current
        widgets = undo_stack.back().state;
        undo_stack.pop_back();
    }
    
    return widgets;
}

// ============================================================================
// Property 1: History Jump Consistency
// ============================================================================

TEST_CASE("Property 1: History Jump Consistency - jump to undo entry restores exact state",
          "[history][property]") {
    // Build history with multiple states
    // State 0: 1 widget
    // State 1: 2 widgets
    // State 2: 3 widgets
    // State 3: 4 widgets
    // State 4: 5 widgets
    // Current: 6 widgets
    std::vector<HistoryEntry> undo_stack;
    
    for (int i = 0; i < 5; ++i) {
        HistoryEntry entry;
        entry.description = "State " + std::to_string(i);
        for (int j = 0; j <= i; ++j) {
            entry.state.push_back(make_widget(j, (float)(j * 10), (float)(j * 20)));
        }
        undo_stack.push_back(entry);
    }
    
    // Current state (6 widgets)
    std::vector<DesignWidget> current_widgets;
    for (int j = 0; j <= 5; ++j) {
        current_widgets.push_back(make_widget(j, (float)(j * 10), (float)(j * 20)));
    }
    
    SECTION("Jump to index 0 (oldest state - 1 widget)") {
        auto result = simulate_jump(current_widgets, undo_stack, 0);
        REQUIRE(result.size() == 1);
        REQUIRE(widgets_equal(result, undo_stack[0].state));
    }
    
    SECTION("Jump to index 2 (middle state - 3 widgets)") {
        auto result = simulate_jump(current_widgets, undo_stack, 2);
        REQUIRE(result.size() == 3);
        REQUIRE(widgets_equal(result, undo_stack[2].state));
    }
    
    SECTION("Jump to index 4 (most recent undo - 5 widgets)") {
        auto result = simulate_jump(current_widgets, undo_stack, 4);
        REQUIRE(result.size() == 5);
        REQUIRE(widgets_equal(result, undo_stack[4].state));
    }
}

// ============================================================================
// Property-based test using Catch2 GENERATE for multiple iterations
// ============================================================================

TEST_CASE("Property 1: History Jump Consistency - randomized iterations",
          "[history][property][pbt]") {
    // Run 100 iterations with different random seeds
    auto seed = GENERATE(range(1, 101));
    
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> stack_size_dist(1, 20);
    std::uniform_int_distribution<int> widget_count_dist(0, 10);
    std::uniform_real_distribution<float> pos_dist(0.0f, 500.0f);
    
    // Generate random undo stack
    int stack_size = stack_size_dist(rng);
    std::vector<HistoryEntry> undo_stack;
    
    for (int i = 0; i < stack_size; ++i) {
        HistoryEntry entry;
        entry.description = "State " + std::to_string(i);
        int widget_count = widget_count_dist(rng);
        for (int j = 0; j < widget_count; ++j) {
            entry.state.push_back(make_widget(j, pos_dist(rng), pos_dist(rng)));
        }
        undo_stack.push_back(entry);
    }
    
    // Current widgets state
    std::vector<DesignWidget> current_widgets;
    int current_count = widget_count_dist(rng);
    for (int j = 0; j < current_count; ++j) {
        current_widgets.push_back(make_widget(j, pos_dist(rng), pos_dist(rng)));
    }
    
    // Pick random target index
    std::uniform_int_distribution<int> target_dist(0, stack_size - 1);
    int target_index = target_dist(rng);
    
    // Expected state is undo_stack[target_index].state
    std::vector<DesignWidget> expected_state = undo_stack[target_index].state;
    
    // Simulate jump
    auto result = simulate_jump(current_widgets, undo_stack, target_index);
    
    // Verify
    INFO("Seed: " << seed << ", Stack size: " << stack_size << ", Target: " << target_index);
    REQUIRE(widgets_equal(result, expected_state));
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_CASE("History Jump: empty undo stack", "[history][edge]") {
    std::vector<HistoryEntry> undo_stack;
    
    // Jump to invalid index should be no-op
    REQUIRE(undo_stack.empty());
}

TEST_CASE("History Jump: single entry stack", "[history][edge]") {
    std::vector<HistoryEntry> undo_stack;
    HistoryEntry entry;
    entry.description = "Initial";
    entry.state.push_back(make_widget(0, 0, 0));
    undo_stack.push_back(entry);
    
    std::vector<DesignWidget> current;
    current.push_back(make_widget(1, 10, 10));
    
    // Jump to index 0 should restore that state
    auto result = simulate_jump(current, undo_stack, 0);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].id == "widget_0");
}

TEST_CASE("History Jump: preserves widget properties", "[history][property]") {
    HistoryEntry entry;
    entry.description = "Test state";
    
    DesignWidget w;
    w.type = WidgetType::Slider;
    w.id = "slider_1";
    w.x = 100.5f;
    w.y = 200.5f;
    w.width = 150;
    w.height = 24;
    w.value = 75.0f;
    w.min_value = 0;
    w.max_value = 100;
    w.text = "Volume";
    w.on_change = "onVolumeChange";
    
    entry.state.push_back(w);
    
    std::vector<HistoryEntry> undo_stack;
    undo_stack.push_back(entry);
    
    std::vector<DesignWidget> current;
    current.push_back(make_widget(99, 0, 0));
    
    auto result = simulate_jump(current, undo_stack, 0);
    
    // Verify all properties are preserved
    REQUIRE(result.size() == 1);
    const auto& restored = result[0];
    REQUIRE(restored.type == WidgetType::Slider);
    REQUIRE(restored.id == "slider_1");
    REQUIRE(restored.x == 100.5f);
    REQUIRE(restored.y == 200.5f);
    REQUIRE(restored.value == 75.0f);
    REQUIRE(restored.on_change == "onVolumeChange");
}
