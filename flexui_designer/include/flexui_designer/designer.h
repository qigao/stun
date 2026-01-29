/*
 * flexUI Designer - Main Designer Class
 *
 * Visual designer for flexUI interfaces using drag & drop.
 * Built on meta_editor infrastructure.
 */

#pragma once

#include <meta_editor/core/editor.h>
#include <flexUI/box.h>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <map>

namespace flexui_designer {

// Widget types available for design
enum class WidgetType {
    Button,
    Label,
    Input,
    Checkbox,
    Switch,
    Slider,
    ProgressBar,
    Dropdown,
    Tabs,
    Card,
    Divider,
    Container
};

// Placed widget info
struct DesignWidget {
    WidgetType type;
    std::string id;
    float x = 0, y = 0;
    float width = 100, height = 30;
    std::string text;
    std::string parent_id;  // For hierarchy/grouping
    
    // Widget-specific properties
    bool checked = false;
    float value = 0;
    float min_value = 0;
    float max_value = 100;
    std::vector<std::string> options;
    
    // Design-time properties
    bool locked = false;      // Cannot move/resize when locked
    bool visible = true;      // Visibility in design view
    std::string group_id;     // Group membership
    
    // Event handlers
    std::string on_click;     // Handler function name
    std::string on_change;    // For inputs, sliders, etc.
    std::string on_focus;
    std::string on_blur;
    
    // Style properties
    uint32_t bg_color = 0x2A2A2EFF;      // RGBA
    uint32_t text_color = 0xFFFFFFFF;
    uint32_t border_color = 0x3A3A40FF;
    float border_width = 1.0f;
    float border_radius = 4.0f;
    float font_size = 12.0f;
    bool font_bold = false;
    float padding = 8.0f;
    
    bool operator==(const DesignWidget& other) const {
        return id == other.id && type == other.type &&
               x == other.x && y == other.y &&
               width == other.width && height == other.height;
    }
};

// History entry with state snapshot and description
struct HistoryEntry {
    std::vector<DesignWidget> state;
    std::string description;
};

class WidgetPalette;
class DesignCanvas;
class PropertyEditor;
class CodeGenerator;
class TemplateGenerator;
class WidgetTree;
class MenuBar;
class StatusBar;
class ShortcutPanel;
class TabBar;
class UnitManager;
class HistoryPanel;
class SearchPanel;
class ScriptEditor;

// Context menu item
struct ContextMenuItem {
    std::string label;
    std::string shortcut;
    std::function<void()> action;
    bool separator = false;
};

class Designer {
public:
    Designer(float width, float height);
    ~Designer();

    void init(flex::Renderer* renderer);
    void shutdown();

    void update(float dt);
    void render(flex::Renderer& renderer);

    bool handle_event(const meta_editor::EditorEvent& event);

    // Widget operations
    DesignWidget* create_widget(WidgetType type, float x, float y);
    void delete_widget(const std::string& id);
    void select_widget(const std::string& id);
    void toggle_select(const std::string& id);
    void select_all();
    void clear_selection();
    DesignWidget* selected_widget();
    const std::vector<std::string>& selection() const { return selection_; }
    
    // Alignment
    void align_left();
    void align_right();
    void align_top();
    void align_bottom();
    void align_center_h();
    void align_center_v();
    void distribute_h();
    void distribute_v();
    
    // Z-order
    void bring_to_front();
    void send_to_back();
    void bring_forward();
    void send_backward();
    
    // Lock/Group
    void lock_selected();
    void unlock_selected();
    void group_selected();
    void ungroup_selected();
    bool is_widget_locked(const std::string& id) const;
    
    // Arrow key movement
    void move_selected(float dx, float dy);
    
    // Preview
    void toggle_preview() { preview_mode_ = !preview_mode_; }
    bool is_preview_mode() const { return preview_mode_; }
    void render_preview(flex::Renderer& renderer);
    void copy_selected();
    void paste();
    void duplicate_selected();
    void undo();
    void redo();
    void push_undo();
    void push_undo(const std::string& description);
    void jump_to_history(int index);
    
    // Style operations
    void copy_style();
    void paste_style();
    
    // Navigation
    void select_next_widget();
    void select_prev_widget();
    
    // Inline editing
    void start_inline_edit();
    void end_inline_edit();
    bool is_inline_editing() const { return inline_editing_; }

    // Code generation
    std::string generate_code() const;
    bool save_code(const std::string& path) const;

    // Project
    bool save_project(const std::string& path) const;
    bool load_project(const std::string& path);
    void new_project();

    // Access
    WidgetPalette* palette() { return palette_.get(); }
    DesignCanvas* canvas() { return design_canvas_.get(); }
    PropertyEditor* properties() { return property_editor_.get(); }
    WidgetTree* widget_tree() { return widget_tree_.get(); }
    MenuBar* menu_bar() { return menu_bar_.get(); }
    StatusBar* status_bar() { return status_bar_.get(); }
    TabBar* tab_bar() { return tab_bar_.get(); }
    UnitManager* unit_manager() { return unit_manager_.get(); }
    HistoryPanel* history_panel() { return history_panel_.get(); }
    SearchPanel* search_panel() { return search_panel_.get(); }
    ScriptEditor* script_editor() { return script_editor_.get(); }

    const std::vector<DesignWidget>& widgets() const { return widgets_; }
    
    // History panel toggle
    void toggle_history_panel() { show_history_panel_ = !show_history_panel_; }
    bool is_history_panel_visible() const { return show_history_panel_; }
    
    // Search panel
    void show_search_panel();
    void hide_search_panel();
    bool is_search_panel_visible() const;
    
    // Script editor toggle
    void toggle_script_editor() { show_script_editor_ = !show_script_editor_; }
    bool is_script_editor_visible() const { return show_script_editor_; }
    
    // Tab/Unit operations
    void new_tab(const std::string& name = "");
    void close_tab(const std::string& id);
    void switch_tab(const std::string& id);
    const std::string& active_tab_id() const;
    
    // Widget tree toggle
    void toggle_widget_tree() { show_widget_tree_ = !show_widget_tree_; }
    bool is_widget_tree_visible() const { return show_widget_tree_; }
    
    // Context menu
    void show_context_menu(float x, float y);
    void hide_context_menu() { context_menu_visible_ = false; }
    bool is_context_menu_visible() const { return context_menu_visible_; }

private:
    float width_, height_;
    
    std::unique_ptr<WidgetPalette> palette_;
    std::unique_ptr<DesignCanvas> design_canvas_;
    std::unique_ptr<PropertyEditor> property_editor_;
    std::unique_ptr<CodeGenerator> code_gen_;
    std::unique_ptr<TemplateGenerator> template_gen_;
    std::unique_ptr<WidgetTree> widget_tree_;
    std::unique_ptr<MenuBar> menu_bar_;
    std::unique_ptr<StatusBar> status_bar_;
    std::unique_ptr<ShortcutPanel> shortcut_panel_;
    std::unique_ptr<TabBar> tab_bar_;
    std::unique_ptr<UnitManager> unit_manager_;
    std::unique_ptr<HistoryPanel> history_panel_;
    std::unique_ptr<SearchPanel> search_panel_;
    std::unique_ptr<ScriptEditor> script_editor_;
    bool show_widget_tree_ = true;
    bool show_history_panel_ = false;
    bool show_script_editor_ = true;
    std::string current_project_path_;

    std::vector<DesignWidget> widgets_;
    std::string selected_id_;
    std::vector<std::string> selection_;
    int next_widget_id_ = 1;
    
    // Tab to Unit mapping
    std::map<std::string, std::string> tab_to_unit_;  // tab_id -> unit_name
    std::string active_unit_name_;
    
    std::optional<DesignWidget> clipboard_;
    std::vector<HistoryEntry> undo_stack_;
    std::vector<HistoryEntry> redo_stack_;
    std::string pending_description_;  // For next push_undo
    
    // Style clipboard (just style properties)
    struct StyleClipboard {
        uint32_t bg_color, text_color, border_color;
        float border_width, border_radius, font_size, padding;
        bool font_bold;
        bool valid = false;
    } style_clipboard_;
    
    // Inline text editing
    bool inline_editing_ = false;
    std::string inline_edit_buffer_;
    
    // Double-click detection
    float last_click_x_ = 0, last_click_y_ = 0;
    float last_click_time_ = 0;
    
    std::vector<DesignWidget*> get_selected_widgets();
    void setup_menus();
    
    bool preview_mode_ = false;
    
    // Context menu
    bool context_menu_visible_ = false;
    float context_menu_x_ = 0, context_menu_y_ = 0;
    std::vector<ContextMenuItem> context_menu_items_;
    void render_context_menu(flex::Renderer& renderer);
    bool handle_context_menu_click(float x, float y);
};


} // namespace flexui_designer
