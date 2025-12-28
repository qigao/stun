/*
 * Meta Editor - Select Tool
 *
 * Tool for selecting, moving, and transforming objects.
 * Double-click on path = enter point editing mode.
 */

#pragma once

#include "../tool.h"
#include "../path_edit.h"
#include "../selection_manager.h"
#include <flex.h>
#include <chrono>

namespace meta_editor {

/**
 * SelectTool - Selection and transformation tool
 *
 * Features:
 * - Click to select
 * - Drag to move
 * - Double-click path = edit points
 * - Transform handles (resize/rotate)
 */
class SelectTool : public Tool {
public:
    SelectTool();

    void activate() override;
    void deactivate() override;

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_key_down(int key, int mods) override;
    bool on_key_up(int key, int mods) override;

    void render_overlay(flex::Renderer& renderer) override;

    const char* name() const override { return "Select"; }
    const char* icon() const override { return "cursor"; }

private:
    enum class Mode {
        Select,     // Normal selection mode
        PathEdit    // Editing path points
    };

    enum class DragMode {
        None,
        Move,
        Resize,         // Resizing via handle
        MovePoint,      // Moving a path point
        MoveHandleIn,   // Moving handle_in
        MoveHandleOut   // Moving handle_out
    };

    // Mode management
    Mode mode_ = Mode::Select;
    void enter_path_edit_mode(flex::Node* path_node);
    void exit_path_edit_mode();

    // Path editing state
    flex::Node* editing_path_ = nullptr;
    PathData path_data_;
    int selected_point_ = -1;
    std::string original_path_data_;

    // Path editing helpers
    void load_path_from_node();
    void apply_path_to_node();
    int hit_test_point(const flex::Vec2& pos, float threshold = 8.0f);
    int hit_test_handle_in(const flex::Vec2& pos, float threshold = 6.0f);
    int hit_test_handle_out(const flex::Vec2& pos, float threshold = 6.0f);
    void render_path_edit_overlay(flex::Renderer& renderer);

    // Double-click detection
    std::chrono::steady_clock::time_point last_click_time_;
    flex::Vec2 last_click_pos_;
    bool is_double_click(const flex::Vec2& pos);

    // Drag state
    DragMode drag_mode_ = DragMode::None;
    HandleType active_handle_ = HandleType::None;
    flex::Vec2 drag_start_world_;
    flex::Vec2 drag_start_screen_;

    // Resize state
    flex::Bounds original_bounds_;
    bool shift_pressed_ = false;

    // Original state for undo
    std::vector<flex::Vec2> original_positions_;
    std::vector<flex::Vec2> original_sizes_;
    PathEditPoint original_point_;

    // Resize helpers
    void apply_resize(const flex::Vec2& screen_pos);
};

} // namespace meta_editor
