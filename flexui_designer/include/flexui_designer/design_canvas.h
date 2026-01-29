/*
 * flexUI Designer - Design Canvas
 *
 * The main design area where widgets are placed and arranged.
 */

#pragma once

#include "designer.h"
#include <meta_editor/view/panel.h>
#include <functional>

namespace flexui_designer {

enum class ResizeHandle { None, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };

class DesignCanvas : public meta_editor::Panel {
public:
    DesignCanvas();

    void render(flex::Renderer& renderer) override;
    bool handle_click(float x, float y) override;

    void set_widgets(std::vector<DesignWidget>* widgets) { widgets_ = widgets; }
    void set_selected_id(const std::string* id) { selected_id_ = id; }
    
    // Inline editing overlay
    void set_inline_edit(bool editing, const std::string* buffer) { 
        inline_editing_ = editing; 
        inline_edit_buffer_ = buffer; 
    }

    int hit_test_widget(float x, float y) const;
    ResizeHandle hit_test_handle(float x, float y) const;

    void start_widget_drag(int index, float offset_x, float offset_y);
    void start_resize(int index, ResizeHandle handle, float x, float y);
    void update_widget_drag(float x, float y);
    void update_resize(float x, float y);
    void end_widget_drag();
    void end_resize();
    
    bool is_widget_dragging() const { return widget_dragging_; }
    bool is_resizing() const { return resizing_; }

    using SelectCallback = std::function<void(int index)>;
    using MoveCallback = std::function<void(int index, float x, float y)>;
    using ResizeCallback = std::function<void(int index, float x, float y, float w, float h)>;
    
    void set_select_callback(SelectCallback cb) { on_select_ = std::move(cb); }
    void set_move_callback(MoveCallback cb) { on_move_ = std::move(cb); }
    void set_resize_callback(ResizeCallback cb) { on_resize_ = std::move(cb); }

    // Box selection
    void start_box_select(float x, float y);
    void update_box_select(float x, float y);
    void end_box_select();
    bool is_box_selecting() const { return box_selecting_; }
    
    using BoxSelectCallback = std::function<void(const std::vector<int>& indices)>;
    void set_box_select_callback(BoxSelectCallback cb) { on_box_select_ = std::move(cb); }

    // Grid settings
    void set_grid_size(float size) { grid_size_ = size; }
    float grid_size() const { return grid_size_; }
    void set_show_grid(bool show) { show_grid_ = show; }
    bool show_grid() const { return show_grid_; }
    void toggle_grid() { show_grid_ = !show_grid_; }
    void toggle_snap() { snap_enabled_ = !snap_enabled_; }
    bool snap_enabled() const { return snap_enabled_; }

    // Smart guides
    void set_show_guides(bool show) { show_guides_ = show; }
    bool show_guides() const { return show_guides_; }
    
    // Rulers
    void set_show_rulers(bool show) { show_rulers_ = show; }
    bool show_rulers() const { return show_rulers_; }
    void toggle_rulers() { show_rulers_ = !show_rulers_; }
    static constexpr float RULER_SIZE = 20.0f;

    // Zoom
    void set_zoom(float z) { zoom_ = std::max(0.25f, std::min(4.0f, z)); }
    float zoom() const { return zoom_; }
    void zoom_in() { set_zoom(zoom_ * 1.25f); }
    void zoom_out() { set_zoom(zoom_ / 1.25f); }
    void zoom_reset() { zoom_ = 1.0f; }
    void zoom_fit();
    
    // Pan
    void set_pan(float x, float y) { pan_x_ = x; pan_y_ = y; }
    float pan_x() const { return pan_x_; }
    float pan_y() const { return pan_y_; }
    void start_pan(float x, float y);
    void update_pan(float x, float y);
    void end_pan();
    bool is_panning() const { return panning_; }
    
    // Coordinate transforms
    void screen_to_canvas(float sx, float sy, float& cx, float& cy) const;
    void canvas_to_screen(float cx, float cy, float& sx, float& sy) const;

private:
    void render_widget(flex::Renderer& renderer, const DesignWidget& w, bool selected);
    void render_grid(flex::Renderer& renderer);
    void render_handles(flex::Renderer& renderer, const DesignWidget& w);

    std::vector<DesignWidget>* widgets_ = nullptr;
    const std::string* selected_id_ = nullptr;

    bool widget_dragging_ = false;
    int drag_widget_index_ = -1;
    float drag_offset_x_ = 0, drag_offset_y_ = 0;

    bool resizing_ = false;
    ResizeHandle resize_handle_ = ResizeHandle::None;
    float resize_start_x_ = 0, resize_start_y_ = 0;
    float resize_orig_x_ = 0, resize_orig_y_ = 0;
    float resize_orig_w_ = 0, resize_orig_h_ = 0;

    SelectCallback on_select_;
    MoveCallback on_move_;
    ResizeCallback on_resize_;
    BoxSelectCallback on_box_select_;

    float grid_size_ = 20.0f;
    bool show_grid_ = true;
    bool snap_enabled_ = true;
    bool show_guides_ = true;
    bool show_rulers_ = true;
    
    // Zoom and pan
    float zoom_ = 1.0f;
    float pan_x_ = 0, pan_y_ = 0;
    bool panning_ = false;
    float pan_start_x_ = 0, pan_start_y_ = 0;
    float pan_orig_x_ = 0, pan_orig_y_ = 0;
    
    // Box selection
    bool box_selecting_ = false;
    float box_start_x_ = 0, box_start_y_ = 0;
    float box_end_x_ = 0, box_end_y_ = 0;
    
    // Smart guides
    std::vector<float> guide_lines_h_;
    std::vector<float> guide_lines_v_;
    void update_guides(float wx, float wy, float ww, float wh);
    void render_guides(flex::Renderer& renderer);
    void render_box_selection(flex::Renderer& renderer);
    void render_rulers(flex::Renderer& renderer);
    void render_inline_edit(flex::Renderer& renderer, const DesignWidget& w);
    
    // Inline editing
    bool inline_editing_ = false;
    const std::string* inline_edit_buffer_ = nullptr;

    static constexpr float HANDLE_SIZE = 8.0f;
};

} // namespace flexui_designer
