/*
 * Meta Editor - Path Edit Tool
 *
 * Dedicated tool for editing bezier path points and handles.
 * Activated by double-clicking a path shape in SelectTool.
 */

#pragma once

#include "../tool.h"
#include "../path_edit.h"
#include "../core/key_codes.h"
#include <flex.h>

namespace meta_editor {

class PathEditTool : public Tool {
public:
    void activate() override;
    void deactivate() override;

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_key_down(int key, int mods) override;

    void render_overlay(flex::Renderer& renderer) override;

    const char* name() const override { return "PathEdit"; }
    const char* icon() const override { return "bezier"; }

    // Entry point - called from SelectTool on double-click
    void edit_path(flex::Node* path_node);
    bool is_editing() const { return editing_path_ != nullptr; }
    flex::Node* editing_node() const { return editing_path_; }

private:
    enum class DragMode { None, MovePoint, MoveHandleIn, MoveHandleOut };

    flex::Node* editing_path_ = nullptr;
    PathData path_data_;
    int selected_point_ = -1;
    std::string original_path_data_;

    DragMode drag_mode_ = DragMode::None;
    flex::Vec2 drag_start_;
    PathEditPoint original_point_;

    void load_path_from_node();
    void apply_path_to_node();
    void commit_edit();

    int hit_test_point(const flex::Vec2& pos, float threshold = 8.0f);
    int hit_test_handle_in(const flex::Vec2& pos, float threshold = 6.0f);
    int hit_test_handle_out(const flex::Vec2& pos, float threshold = 6.0f);
};

} // namespace meta_editor
