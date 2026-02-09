/*
 * Meta Editor - Select Tool
 *
 * Tool for selecting, moving, and transforming objects.
 * Double-click delegates to PathEditTool or TextEditTool.
 */

#pragma once

#include "../tool.h"
#include "../selection_manager.h"
#include "../snap_helper.h"
#include "../core/key_codes.h"
#include <flex.h>
#include <chrono>
#include <memory>

namespace meta_editor {

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
    void render_screen_overlay(flex::Renderer& renderer) override;

    const char* name() const override { return "Select"; }
    const char* icon() const override { return "cursor"; }

private:
    enum class DragMode { None, Move, Resize, Rotate, Marquee };

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
    std::vector<float> original_rotations_;

    // Resize/Rotate helpers
    void apply_resize(const flex::Vec2& screen_pos);
    void apply_rotate(const flex::Vec2& screen_pos);
    flex::Vec2 rotation_center_;

    // Marquee selection
    flex::Vec2 marquee_end_;
    void render_marquee(flex::Renderer& renderer);
    std::vector<flex::Node*> get_nodes_in_marquee();

    // Smart guides
    std::unique_ptr<SnapHelper> snap_helper_;
    void render_snap_guides(flex::Renderer& renderer);
};

} // namespace meta_editor
