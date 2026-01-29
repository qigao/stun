/*
 * flexUI Designer - Tab Bar for multiple design units
 */

#pragma once

#include <flex/runtime/renderer.h>
#include <string>
#include <vector>
#include <functional>

namespace flexui_designer {

struct TabInfo {
    std::string id;
    std::string title;
    bool modified = false;
    bool closable = true;
};

struct TabContextMenuItem {
    std::string label;
    std::function<void()> action;
    bool separator = false;
};

class TabBar {
public:
    static constexpr float HEIGHT = 28.0f;
    static constexpr float TAB_WIDTH = 150.0f;
    static constexpr float TAB_MIN_WIDTH = 80.0f;

    void set_position(float x, float y) { x_ = x; y_ = y; }
    void set_size(float w, float h) { width_ = w; height_ = h; }

    void add_tab(const std::string& id, const std::string& title);
    void remove_tab(const std::string& id);
    void set_active(const std::string& id);
    void set_modified(const std::string& id, bool modified);
    void rename_tab(const std::string& id, const std::string& new_title);
    void move_tab(int from_index, int to_index);
    
    const std::string& active_id() const { return active_id_; }
    size_t tab_count() const { return tabs_.size(); }
    const std::vector<TabInfo>& tabs() const { return tabs_; }

    void render(flex::Renderer& renderer);
    bool handle_click(float x, float y, bool right_click = false);
    bool handle_drag(float x, float y);
    bool handle_drop(float x, float y);
    void cancel_drag();
    
    bool is_dragging() const { return dragging_; }
    bool is_context_menu_visible() const { return context_menu_visible_; }

    using SelectCallback = std::function<void(const std::string& id)>;
    using CloseCallback = std::function<void(const std::string& id)>;
    using NewTabCallback = std::function<void()>;
    using RenameCallback = std::function<void(const std::string& id)>;
    
    void set_select_callback(SelectCallback cb) { on_select_ = std::move(cb); }
    void set_close_callback(CloseCallback cb) { on_close_ = std::move(cb); }
    void set_new_tab_callback(NewTabCallback cb) { on_new_tab_ = std::move(cb); }
    void set_rename_callback(RenameCallback cb) { on_rename_ = std::move(cb); }

private:
    float x_ = 0, y_ = 0, width_ = 800, height_ = HEIGHT;
    std::vector<TabInfo> tabs_;
    std::string active_id_;
    
    SelectCallback on_select_;
    CloseCallback on_close_;
    NewTabCallback on_new_tab_;
    RenameCallback on_rename_;
    
    // Drag state
    bool dragging_ = false;
    int drag_tab_index_ = -1;
    float drag_start_x_ = 0;
    float drag_offset_x_ = 0;
    float drag_current_x_ = 0;
    
    // Context menu
    bool context_menu_visible_ = false;
    float context_menu_x_ = 0, context_menu_y_ = 0;
    int context_menu_tab_index_ = -1;
    std::vector<TabContextMenuItem> context_menu_items_;
    
    int hit_test_tab(float x, float y) const;
    bool hit_test_close(float x, float y, int tab_index) const;
    float get_tab_width() const;
    void show_context_menu(int tab_index, float x, float y);
    void render_context_menu(flex::Renderer& renderer);
    bool handle_context_menu_click(float x, float y);
};

} // namespace flexui_designer
