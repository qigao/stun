/*
 * flexUI Designer - Main Implementation
 */

#include "flexui_designer/designer.h"
#include "flexui_designer/widget_palette.h"
#include "flexui_designer/design_canvas.h"
#include "flexui_designer/property_editor.h"
#include "flexui_designer/code_generator.h"
#include "flexui_designer/template_generator.h"
#include "flexui_designer/widget_tree.h"
#include "flexui_designer/menu_bar.h"
#include "flexui_designer/file_dialog.h"
#include "flexui_designer/status_bar.h"
#include "flexui_designer/shortcut_panel.h"
#include "flexui_designer/tab_bar.h"
#include "flexui_designer/design_unit.h"
#include "flexui_designer/history_panel.h"
#include "flexui_designer/search_panel.h"
#include "flexui_designer/script_editor.h"
#include <cmath>
#include <ctime>
#include <fstream>
#include <algorithm>

namespace flexui_designer {

static const char* widget_type_name(WidgetType type) {
    switch (type) {
        case WidgetType::Button: return "Button";
        case WidgetType::Label: return "Label";
        case WidgetType::Input: return "Input";
        case WidgetType::Checkbox: return "Checkbox";
        case WidgetType::Switch: return "Switch";
        case WidgetType::Slider: return "Slider";
        case WidgetType::ProgressBar: return "ProgressBar";
        case WidgetType::Dropdown: return "Dropdown";
        case WidgetType::Tabs: return "Tabs";
        case WidgetType::Card: return "Card";
        case WidgetType::Divider: return "Divider";
        case WidgetType::Container: return "Container";
        default: return "Widget";
    }
}

Designer::Designer(float width, float height)
    : width_(width), height_(height) {}

Designer::~Designer() = default;

void Designer::init(flex::Renderer* renderer) {
    FileDialog::init();
    
    palette_ = std::make_unique<WidgetPalette>();
    palette_->set_position(0, 0);
    palette_->set_size(200, height_);

    design_canvas_ = std::make_unique<DesignCanvas>();
    design_canvas_->set_position(200, 0);
    design_canvas_->set_size(width_ - 450, height_);
    design_canvas_->set_widgets(&widgets_);
    design_canvas_->set_selected_id(&selected_id_);
    design_canvas_->set_inline_edit(false, &inline_edit_buffer_);

    property_editor_ = std::make_unique<PropertyEditor>();
    property_editor_->set_position(width_ - 250, 0);
    property_editor_->set_size(250, height_);

    code_gen_ = std::make_unique<CodeGenerator>();
    template_gen_ = std::make_unique<TemplateGenerator>();

    widget_tree_ = std::make_unique<WidgetTree>();
    widget_tree_->set_position(0, height_ / 2);
    widget_tree_->set_size(200, height_ / 2);
    widget_tree_->set_widgets(&widgets_);
    widget_tree_->set_selected_id(&selected_id_);
    widget_tree_->set_select_callback([this](const std::string& id) {
        select_widget(id);
    });

    // Menu bar
    menu_bar_ = std::make_unique<MenuBar>();
    menu_bar_->set_position(0, 0);
    menu_bar_->set_size(width_, MenuBar::HEIGHT);
    setup_menus();

    // Status bar
    status_bar_ = std::make_unique<StatusBar>();
    status_bar_->set_designer(this);
    status_bar_->set_position(0, height_ - StatusBar::HEIGHT);
    status_bar_->set_size(width_, StatusBar::HEIGHT);

    // Shortcut panel
    shortcut_panel_ = std::make_unique<ShortcutPanel>();
    shortcut_panel_->set_position(0, 0, width_, height_);

    // Tab bar for multi-unit support
    tab_bar_ = std::make_unique<TabBar>();
    tab_bar_->set_position(0, MenuBar::HEIGHT);
    tab_bar_->set_size(width_, TabBar::HEIGHT);
    tab_bar_->set_select_callback([this](const std::string& id) { switch_tab(id); });
    tab_bar_->set_close_callback([this](const std::string& id) { close_tab(id); });
    tab_bar_->set_new_tab_callback([this]() { new_tab(); });
    tab_bar_->set_rename_callback([this](const std::string& id) {
        // TODO: Show rename dialog - for now just append " (renamed)"
        auto it = tab_to_unit_.find(id);
        if (it != tab_to_unit_.end()) {
            std::string new_name = it->second + " (2)";
            unit_manager_->rename_unit(it->second, new_name);
            it->second = new_name;
            tab_bar_->rename_tab(id, new_name);
        }
    });

    // Unit manager
    unit_manager_ = std::make_unique<UnitManager>();

    // History panel
    history_panel_ = std::make_unique<HistoryPanel>();
    history_panel_->set_position(width_ - 450, MenuBar::HEIGHT + TabBar::HEIGHT);
    history_panel_->set_size(200, 300);
    history_panel_->set_stacks(&undo_stack_, &redo_stack_);
    history_panel_->set_jump_callback([this](int index) { jump_to_history(index); });
    history_panel_->set_visible(false);

    // Search panel
    search_panel_ = std::make_unique<SearchPanel>();
    search_panel_->set_position(width_ / 2 - 150, MenuBar::HEIGHT + TabBar::HEIGHT + 50);
    search_panel_->set_size(300, 350);
    search_panel_->set_widgets(&widgets_);
    search_panel_->set_select_callback([this](const std::string& id) {
        select_widget(id);
    });
    search_panel_->set_visible(false);

    // Script editor panel
    script_editor_ = std::make_unique<ScriptEditor>();
    script_editor_->set_position(width_ - 250, MenuBar::HEIGHT + TabBar::HEIGHT + 300);
    script_editor_->set_size(250, 280);
    script_editor_->set_change_callback([this]() {
        push_undo("Edit script");
        tab_bar_->set_modified(tab_bar_->active_id(), true);
    });
    script_editor_->set_visible(true);

    // Adjust positions for menu bar, tab bar, and status bar
    float menu_h = MenuBar::HEIGHT;
    float tab_h = TabBar::HEIGHT;
    float status_h = StatusBar::HEIGHT;
    float content_h = height_ - menu_h - tab_h - status_h;
    palette_->set_position(0, menu_h + tab_h);
    palette_->set_size(200, content_h);
    design_canvas_->set_position(200, menu_h + tab_h);
    design_canvas_->set_size(width_ - 450, content_h);
    property_editor_->set_position(width_ - 250, menu_h + tab_h);
    property_editor_->set_size(250, content_h);
    widget_tree_->set_position(0, menu_h + tab_h + content_h / 2);
    widget_tree_->set_size(200, content_h / 2);

    // Create initial tab
    new_tab("Untitled");

    // Setup callbacks
    palette_->set_drop_callback([this](WidgetType type, float x, float y) {
        float canvas_x = x - design_canvas_->x();
        float canvas_y = y - design_canvas_->y();
        if (canvas_x >= 0 && canvas_y >= 0 && 
            canvas_x < design_canvas_->width() && 
            canvas_y < design_canvas_->height()) {
            push_undo(std::string("Create ") + widget_type_name(type));
            auto* w = create_widget(type, canvas_x, canvas_y);
            if (w) {
                w->x -= w->width / 2;
                w->y -= w->height / 2;
                w->x = std::round(w->x / 20.0f) * 20.0f;
                w->y = std::round(w->y / 20.0f) * 20.0f;
                select_widget(w->id);
            }
        }
    });

    design_canvas_->set_select_callback([this](int index) {
        if (index >= 0 && index < (int)widgets_.size()) {
            select_widget(widgets_[index].id);
        } else {
            selected_id_.clear();
            property_editor_->set_widget(nullptr);
        }
    });

    design_canvas_->set_move_callback([this](int index, float x, float y) {
        if (index >= 0 && index < (int)widgets_.size()) {
            widgets_[index].x = x;
            widgets_[index].y = y;
        }
    });

    design_canvas_->set_resize_callback([this](int index, float x, float y, float w, float h) {
        if (index >= 0 && index < (int)widgets_.size()) {
            widgets_[index].x = x;
            widgets_[index].y = y;
            widgets_[index].width = w;
            widgets_[index].height = h;
        }
    });

    design_canvas_->set_box_select_callback([this](const std::vector<int>& indices) {
        selection_.clear();
        for (int idx : indices) {
            if (idx >= 0 && idx < (int)widgets_.size()) {
                selection_.push_back(widgets_[idx].id);
            }
        }
        selected_id_ = selection_.empty() ? "" : selection_.back();
        
        // Use batch mode for multiple selection
        if (selection_.size() > 1) {
            property_editor_->set_widgets(get_selected_widgets());
        } else {
            property_editor_->set_widget(selected_widget());
        }
    });
}

void Designer::shutdown() {
    palette_.reset();
    design_canvas_.reset();
    property_editor_.reset();
    code_gen_.reset();
    template_gen_.reset();
    widget_tree_.reset();
    menu_bar_.reset();
    status_bar_.reset();
    shortcut_panel_.reset();
    tab_bar_.reset();
    unit_manager_.reset();
    history_panel_.reset();
    search_panel_.reset();
    script_editor_.reset();
    widgets_.clear();
    FileDialog::shutdown();
}

void Designer::setup_menus() {
    // File menu
    menu_bar_->add_menu("File", {
        {"New Project", "Ctrl+N", [this]() { new_project(); }},
        {"New Tab", "Ctrl+T", [this]() { new_tab(); }},
        {"Open...", "Ctrl+O", [this]() { 
            auto path = FileDialog::open_file(nullptr, nullptr, "FlexUI Project", "fuid");
            if (path) load_project(*path);
        }},
        {"Save Project", "Ctrl+Shift+S", [this]() { 
            if (current_project_path_.empty()) {
                auto path = FileDialog::save_file(nullptr, nullptr, "project.fuid", "FlexUI Project", "fuid");
                if (path) { 
                    current_project_path_ = *path; 
                    save_project(*path); 
                    tab_bar_->set_modified(tab_bar_->active_id(), false);
                }
            } else {
                save_project(current_project_path_);
                tab_bar_->set_modified(tab_bar_->active_id(), false);
            }
        }},
        {"Save As...", "", [this]() {
            auto path = FileDialog::save_file(nullptr, nullptr, "project.fuid", "FlexUI Project", "fuid");
            if (path) { 
                current_project_path_ = *path; 
                save_project(*path); 
                tab_bar_->set_modified(tab_bar_->active_id(), false);
            }
        }},
        {"", "", nullptr, true},
        {"Export C++...", "Ctrl+E", [this]() { 
            auto path = FileDialog::save_file(nullptr, nullptr, "generated_ui.cpp", "C++ Source", "cpp");
            if (path) template_gen_->save(*path, template_gen_->generate(widgets_, "cpp"));
        }},
        {"Export JSON...", "", [this]() {
            auto path = FileDialog::save_file(nullptr, nullptr, "ui.json", "JSON", "json");
            if (path) template_gen_->save(*path, template_gen_->generate(widgets_, "json"));
        }},
        {"Export XML...", "", [this]() {
            auto path = FileDialog::save_file(nullptr, nullptr, "ui.xml", "XML", "xml");
            if (path) template_gen_->save(*path, template_gen_->generate(widgets_, "xml"));
        }},
        {"", "", nullptr, true},
        {"Exit", "Alt+F4", nullptr}
    });

    // Edit menu
    menu_bar_->add_menu("Edit", {
        {"Undo", "Ctrl+Z", [this]() { undo(); }},
        {"Redo", "Ctrl+Y", [this]() { redo(); }},
        {"", "", nullptr, true},
        {"Cut", "Ctrl+X", [this]() { copy_selected(); delete_widget(selected_id_); }},
        {"Copy", "Ctrl+C", [this]() { copy_selected(); }},
        {"Paste", "Ctrl+V", [this]() { paste(); }},
        {"Duplicate", "Ctrl+D", [this]() { duplicate_selected(); }},
        {"", "", nullptr, true},
        {"Delete", "Del", [this]() { if (!selected_id_.empty()) delete_widget(selected_id_); }},
        {"Select All", "Ctrl+A", [this]() { select_all(); }}
    });

    // View menu
    menu_bar_->add_menu("View", {
        {"Zoom In", "Ctrl++", [this]() { design_canvas_->zoom_in(); }},
        {"Zoom Out", "Ctrl+-", [this]() { design_canvas_->zoom_out(); }},
        {"Zoom 100%", "Ctrl+0", [this]() { design_canvas_->zoom_reset(); }},
        {"Zoom to Fit", "Ctrl+1", [this]() { design_canvas_->zoom_fit(); }},
        {"", "", nullptr, true},
        {"Show Grid", "G", [this]() { design_canvas_->toggle_grid(); }, false, design_canvas_->show_grid(), true},
        {"Snap to Grid", "S", [this]() { design_canvas_->toggle_snap(); }, false, design_canvas_->snap_enabled(), true},
        {"Show Rulers", "R", [this]() { design_canvas_->toggle_rulers(); }, false, design_canvas_->show_rulers(), true},
        {"Show Guides", "", [this]() { design_canvas_->set_show_guides(!design_canvas_->show_guides()); }, false, design_canvas_->show_guides(), true},
        {"", "", nullptr, true},
        {"Widget Tree", "T", [this]() { toggle_widget_tree(); }, false, show_widget_tree_, true},
        {"History Panel", "H", [this]() { toggle_history_panel(); }, false, show_history_panel_, true},
        {"", "", nullptr, true},
        {"Preview Mode", "P", [this]() { toggle_preview(); }, false, preview_mode_, true}
    });

    // Arrange menu
    menu_bar_->add_menu("Arrange", {
        {"Bring to Front", "", [this]() { bring_to_front(); }},
        {"Send to Back", "", [this]() { send_to_back(); }},
        {"Bring Forward", "", [this]() { bring_forward(); }},
        {"Send Backward", "", [this]() { send_backward(); }},
        {"", "", nullptr, true},
        {"Lock", "Ctrl+L", [this]() { lock_selected(); }},
        {"Unlock", "Ctrl+Shift+L", [this]() { unlock_selected(); }},
        {"", "", nullptr, true},
        {"Group", "Ctrl+G", [this]() { group_selected(); }},
        {"Ungroup", "Ctrl+Shift+G", [this]() { ungroup_selected(); }},
        {"", "", nullptr, true},
        {"Align Left", "", [this]() { align_left(); }},
        {"Align Right", "", [this]() { align_right(); }},
        {"Align Top", "", [this]() { align_top(); }},
        {"Align Bottom", "", [this]() { align_bottom(); }},
        {"", "", nullptr, true},
        {"Center Horizontal", "", [this]() { align_center_h(); }},
        {"Center Vertical", "", [this]() { align_center_v(); }},
        {"", "", nullptr, true},
        {"Distribute Horizontal", "", [this]() { distribute_h(); }},
        {"Distribute Vertical", "", [this]() { distribute_v(); }}
    });
}

void Designer::update(float dt) {
    if (status_bar_) status_bar_->update(dt);
}

void Designer::render(flex::Renderer& renderer) {
    if (preview_mode_) {
        render_preview(renderer);
        return;
    }
    
    // Render panels first (bottom layer)
    palette_->render(renderer);
    if (show_widget_tree_) widget_tree_->render(renderer);
    design_canvas_->render(renderer);
    property_editor_->render(renderer);
    palette_->render_drag_preview(renderer);
    
    // History panel (floating)
    if (show_history_panel_) {
        history_panel_->render(renderer);
    }
    
    // Script editor panel
    if (show_script_editor_) {
        script_editor_->render(renderer);
    }
    
    // Search panel (floating, on top)
    if (search_panel_->is_visible()) {
        search_panel_->render(renderer);
    }
    
    if (context_menu_visible_) {
        render_context_menu(renderer);
    }
    
    // Status bar
    status_bar_->render(renderer);
    
    // Tab bar
    tab_bar_->render(renderer);
    
    // Menu bar always on top
    menu_bar_->render(renderer);
    
    // Shortcut panel on very top
    shortcut_panel_->render(renderer);
}

void Designer::render_preview(flex::Renderer& renderer) {
    // Dark background
    renderer.draw_rect(0, 0, width_, height_, 0, 
        flex::Paint::solid(flex::Color{0.08f, 0.08f, 0.1f, 1}), flex::Paint::none(), 0);
    
    // Render widgets without selection/handles
    for (const auto& w : widgets_) {
        float wx = 200 + w.x, wy = w.y;  // Offset from palette area
        flex::Paint fill, stroke;
        flex::Color text_col{1, 1, 1, 1};

        switch (w.type) {
            case WidgetType::Button:
                fill = flex::Paint::solid(flex::Color{0.25f, 0.5f, 0.9f, 1});
                renderer.draw_rect(wx, wy, w.width, w.height, 4, fill, flex::Paint::none(), 0);
                renderer.draw_text(w.text, wx + w.width/2 - w.text.length()*3, wy + w.height/2 + 5, "sans", 12, false, text_col);
                break;
            case WidgetType::Label:
                renderer.draw_text(w.text, wx, wy + 14, "sans", 12, false, {0.9f, 0.9f, 0.9f, 1});
                break;
            case WidgetType::Input:
                fill = flex::Paint::solid(flex::Color{0.15f, 0.15f, 0.18f, 1});
                stroke = flex::Paint::solid(flex::Color{0.35f, 0.35f, 0.4f, 1});
                renderer.draw_rect(wx, wy, w.width, w.height, 4, fill, stroke, 1);
                renderer.draw_text(w.text, wx + 8, wy + w.height/2 + 5, "sans", 12, false, {0.5f, 0.5f, 0.5f, 1});
                break;
            case WidgetType::Checkbox:
                fill = flex::Paint::solid(flex::Color{0.15f, 0.15f, 0.18f, 1});
                stroke = flex::Paint::solid(flex::Color{0.35f, 0.35f, 0.4f, 1});
                renderer.draw_rect(wx, wy, 18, 18, 3, fill, stroke, 1);
                renderer.draw_text(w.text, wx + 26, wy + 14, "sans", 12, false, {0.9f, 0.9f, 0.9f, 1});
                break;
            case WidgetType::Switch: {
                bool on = w.checked;
                flex::Color bg = on ? flex::Color{0.25f, 0.5f, 0.9f, 1} : flex::Color{0.3f, 0.3f, 0.35f, 1};
                renderer.draw_rect(wx, wy, 44, 24, 12, flex::Paint::solid(bg), flex::Paint::none(), 0);
                float knob_x = on ? wx + 22 : wx + 2;
                renderer.draw_rect(knob_x, wy + 2, 20, 20, 10, flex::Paint::solid(flex::Color{1,1,1,1}), flex::Paint::none(), 0);
                break;
            }
            case WidgetType::Slider: {
                fill = flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.28f, 1});
                renderer.draw_rect(wx, wy + 8, w.width, 8, 4, fill, flex::Paint::none(), 0);
                float pct = (w.value - w.min_value) / (w.max_value - w.min_value);
                renderer.draw_rect(wx, wy + 8, w.width * pct, 8, 4, flex::Paint::solid(flex::Color{0.25f, 0.5f, 0.9f, 1}), flex::Paint::none(), 0);
                renderer.draw_rect(wx + w.width * pct - 8, wy + 4, 16, 16, 8, flex::Paint::solid(flex::Color{1,1,1,1}), flex::Paint::none(), 0);
                break;
            }
            case WidgetType::ProgressBar: {
                fill = flex::Paint::solid(flex::Color{0.18f, 0.18f, 0.2f, 1});
                renderer.draw_rect(wx, wy, w.width, w.height, 4, fill, flex::Paint::none(), 0);
                float pct = (w.value - w.min_value) / (w.max_value - w.min_value);
                renderer.draw_rect(wx, wy, w.width * pct, w.height, 4, flex::Paint::solid(flex::Color{0.2f, 0.7f, 0.4f, 1}), flex::Paint::none(), 0);
                break;
            }
            case WidgetType::Dropdown:
                fill = flex::Paint::solid(flex::Color{0.15f, 0.15f, 0.18f, 1});
                stroke = flex::Paint::solid(flex::Color{0.35f, 0.35f, 0.4f, 1});
                renderer.draw_rect(wx, wy, w.width, w.height, 4, fill, stroke, 1);
                renderer.draw_text(w.text, wx + 10, wy + w.height/2 + 5, "sans", 12, false, {0.9f, 0.9f, 0.9f, 1});
                renderer.draw_text("v", wx + w.width - 20, wy + w.height/2 + 5, "sans", 10, false, {0.6f, 0.6f, 0.6f, 1});
                break;
            case WidgetType::Tabs: {
                float tab_w = w.width / std::max(1, (int)w.options.size());
                for (size_t i = 0; i < w.options.size(); ++i) {
                    flex::Color c = (i == 0) ? flex::Color{0.25f, 0.5f, 0.9f, 1} : flex::Color{0.18f, 0.18f, 0.2f, 1};
                    renderer.draw_rect(wx + i * tab_w, wy, tab_w - 2, w.height, 4, flex::Paint::solid(c), flex::Paint::none(), 0);
                    renderer.draw_text(w.options[i], wx + i * tab_w + 10, wy + w.height/2 + 5, "sans", 11, false, text_col);
                }
                break;
            }
            case WidgetType::Card:
                fill = flex::Paint::solid(flex::Color{0.14f, 0.14f, 0.16f, 1});
                stroke = flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.28f, 1});
                renderer.draw_rect(wx, wy, w.width, w.height, 8, fill, stroke, 1);
                renderer.draw_text(w.text, wx + 16, wy + 28, "sans", 14, true, text_col);
                break;
            case WidgetType::Divider:
                renderer.draw_rect(wx, wy, w.width, 1, 0, flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.35f, 1}), flex::Paint::none(), 0);
                break;
            case WidgetType::Container:
                stroke = flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.35f, 0.3f});
                renderer.draw_rect(wx, wy, w.width, w.height, 4, flex::Paint::none(), stroke, 1);
                break;
        }
    }

    // Preview mode indicator
    flex::Color indicator{0.9f, 0.7f, 0.2f, 1};
    renderer.draw_text("PREVIEW MODE - Press P to exit", 10, 30, "sans", 14, true, indicator);
}

bool Designer::handle_event(const meta_editor::EditorEvent& event) {
    // Handle search panel first if visible
    if (search_panel_->is_visible()) {
        if (event.type == meta_editor::EditorEvent::Type::PointerDown) {
            if (search_panel_->contains(event.x, event.y)) {
                search_panel_->handle_click(event.x, event.y);
                return true;
            } else {
                // Click outside closes search panel
                search_panel_->hide();
                return true;
            }
        }
        if (event.type == meta_editor::EditorEvent::Type::KeyDown) {
            if (search_panel_->handle_key(event.key)) {
                return true;
            }
        }
        if (event.type == meta_editor::EditorEvent::Type::TextInput) {
            if (search_panel_->handle_text_input(event.text)) {
                return true;
            }
        }
        return false;
    }

    // Handle shortcut panel first
    if (shortcut_panel_->visible()) {
        if (event.type == meta_editor::EditorEvent::Type::PointerDown) {
            shortcut_panel_->handle_click(event.x, event.y);
            return true;
        }
        if (event.type == meta_editor::EditorEvent::Type::KeyDown) {
            if (event.key == 0x1B) { // Esc
                shortcut_panel_->hide();
                return true;
            }
        }
        return false;
    }

    // Handle menu bar first
    if (menu_bar_->is_menu_open()) {
        if (event.type == meta_editor::EditorEvent::Type::PointerDown) {
            menu_bar_->handle_click(event.x, event.y);
            return true;
        }
        return false;
    }
    
    // Handle context menu first if visible
    if (context_menu_visible_) {
        if (event.type == meta_editor::EditorEvent::Type::PointerDown) {
            handle_context_menu_click(event.x, event.y);
            return true;
        }
        return false;
    }

    if (event.type == meta_editor::EditorEvent::Type::PointerDown) {
        // Check menu bar
        if (event.y < MenuBar::HEIGHT) {
            return menu_bar_->handle_click(event.x, event.y);
        }
        
        // Check tab bar
        float tab_top = MenuBar::HEIGHT;
        float tab_bottom = tab_top + TabBar::HEIGHT;
        if (event.y >= tab_top && event.y < tab_bottom) {
            bool right_click = (event.button == meta_editor::MouseButton::Right);
            return tab_bar_->handle_click(event.x, event.y, right_click);
        }
        
        // Right click - show context menu
        if (event.button == meta_editor::MouseButton::Right) {
            if (design_canvas_->contains(event.x, event.y)) {
                float ruler_offset = design_canvas_->show_rulers() ? DesignCanvas::RULER_SIZE : 0;
                float local_x = event.x - design_canvas_->x() - ruler_offset;
                float local_y = event.y - design_canvas_->y() - ruler_offset;
                int idx = design_canvas_->hit_test_widget(local_x, local_y);
                if (idx >= 0) select_widget(widgets_[idx].id);
                show_context_menu(event.x, event.y);
                return true;
            }
        }
        
        // Middle click - start panning
        if (event.button == meta_editor::MouseButton::Middle) {
            if (design_canvas_->contains(event.x, event.y)) {
                design_canvas_->start_pan(event.x, event.y);
                return true;
            }
        }
        
        // Check palette
        if (palette_->contains(event.x, event.y)) {
            return palette_->handle_click(event.x, event.y);
        }
        // Check canvas
        if (design_canvas_->contains(event.x, event.y)) {
            float ruler_offset = design_canvas_->show_rulers() ? DesignCanvas::RULER_SIZE : 0;
            float local_x = event.x - design_canvas_->x() - ruler_offset;
            float local_y = event.y - design_canvas_->y() - ruler_offset;
            
            // Double-click detection
            float now = (float)clock() / CLOCKS_PER_SEC;
            bool is_double_click = (now - last_click_time_ < 0.4f) &&
                std::abs(event.x - last_click_x_) < 5 &&
                std::abs(event.y - last_click_y_) < 5;
            last_click_x_ = event.x;
            last_click_y_ = event.y;
            last_click_time_ = now;
            
            // Check resize handles first
            auto handle = design_canvas_->hit_test_handle(local_x, local_y);
            if (handle != ResizeHandle::None) {
                int idx = -1;
                for (int i = 0; i < (int)widgets_.size(); ++i) {
                    if (widgets_[i].id == selected_id_) { idx = i; break; }
                }
                if (idx >= 0) {
                    design_canvas_->start_resize(idx, handle, local_x, local_y);
                    return true;
                }
            }
            
            int idx = design_canvas_->hit_test_widget(local_x, local_y);
            if (idx >= 0) {
                select_widget(widgets_[idx].id);
                
                // Double-click to edit text
                if (is_double_click) {
                    start_inline_edit();
                    return true;
                }
                
                design_canvas_->start_widget_drag(idx, 
                    local_x - widgets_[idx].x, 
                    local_y - widgets_[idx].y);
            } else {
                // Start box selection on empty area
                design_canvas_->start_box_select(local_x, local_y);
                selected_id_.clear();
                property_editor_->set_widget(nullptr);
            }
            return true;
        }
        // Check properties
        if (property_editor_->contains(event.x, event.y)) {
            return property_editor_->handle_click(event.x, event.y);
        }
        // Check widget tree
        if (show_widget_tree_ && widget_tree_->contains(event.x, event.y)) {
            return widget_tree_->handle_click(event.x, event.y);
        }
        // Check history panel
        if (show_history_panel_ && history_panel_->contains(event.x, event.y)) {
            return history_panel_->handle_click(event.x, event.y);
        }
        // Check script editor
        if (show_script_editor_ && script_editor_->contains(event.x, event.y)) {
            return script_editor_->handle_click(event.x, event.y);
        }
    }

    if (event.type == meta_editor::EditorEvent::Type::PointerMove) {
        float ruler_offset = design_canvas_->show_rulers() ? DesignCanvas::RULER_SIZE : 0;
        if (tab_bar_->is_dragging()) {
            tab_bar_->handle_drag(event.x, event.y);
            return true;
        }
        if (palette_->is_dragging()) {
            palette_->update_drag(event.x, event.y);
            return true;
        }
        if (design_canvas_->is_panning()) {
            design_canvas_->update_pan(event.x, event.y);
            return true;
        }
        if (design_canvas_->is_box_selecting()) {
            float local_x = event.x - design_canvas_->x() - ruler_offset;
            float local_y = event.y - design_canvas_->y() - ruler_offset;
            design_canvas_->update_box_select(local_x, local_y);
            return true;
        }
        if (design_canvas_->is_resizing()) {
            float local_x = event.x - design_canvas_->x() - ruler_offset;
            float local_y = event.y - design_canvas_->y() - ruler_offset;
            design_canvas_->update_resize(local_x, local_y);
            return true;
        }
        if (design_canvas_->is_widget_dragging()) {
            float local_x = event.x - design_canvas_->x() - ruler_offset;
            float local_y = event.y - design_canvas_->y() - ruler_offset;
            design_canvas_->update_widget_drag(local_x, local_y);
            return true;
        }
    }

    if (event.type == meta_editor::EditorEvent::Type::PointerUp) {
        if (tab_bar_->is_dragging()) {
            tab_bar_->handle_drop(event.x, event.y);
            return true;
        }
        if (palette_->is_dragging()) {
            palette_->end_drag(event.x, event.y);
            return true;
        }
        if (design_canvas_->is_box_selecting()) {
            design_canvas_->end_box_select();
            return true;
        }
        if (design_canvas_->is_panning()) {
            design_canvas_->end_pan();
            return true;
        }
        if (design_canvas_->is_resizing()) {
            design_canvas_->end_resize();
            return true;
        }
        if (design_canvas_->is_widget_dragging()) {
            design_canvas_->end_widget_drag();
            return true;
        }
    }

    // Scroll wheel for zoom
    if (event.type == meta_editor::EditorEvent::Type::Wheel) {
        if (design_canvas_->contains(event.x, event.y)) {
            if (event.wheel_y > 0) {
                design_canvas_->zoom_in();
            } else if (event.wheel_y < 0) {
                design_canvas_->zoom_out();
            }
            return true;
        }
    }

    if (event.type == meta_editor::EditorEvent::Type::KeyDown) {
        bool ctrl = event.has_ctrl();
        
        // Arrow keys for movement
        float step = ctrl ? 1.0f : design_canvas_->grid_size();
        if (event.key == 0x25) { move_selected(-step, 0); return true; } // Left
        if (event.key == 0x27) { move_selected(step, 0); return true; }  // Right
        if (event.key == 0x26) { move_selected(0, -step); return true; } // Up
        if (event.key == 0x28) { move_selected(0, step); return true; }  // Down
        
        // Delete
        if ((event.key == 127 || event.key == 0x2E) && !selected_id_.empty()) {
            delete_widget(selected_id_);
            return true;
        }
        // Ctrl+C: Copy
        if (ctrl && (event.key == 'c' || event.key == 'C')) {
            copy_selected();
            return true;
        }
        // Ctrl+V: Paste
        if (ctrl && (event.key == 'v' || event.key == 'V')) {
            paste();
            return true;
        }
        // Ctrl+D: Duplicate
        if (ctrl && (event.key == 'd' || event.key == 'D')) {
            duplicate_selected();
            return true;
        }
        // Ctrl+Z: Undo
        if (ctrl && (event.key == 'z' || event.key == 'Z')) {
            undo();
            return true;
        }
        // Ctrl+Y: Redo
        if (ctrl && (event.key == 'y' || event.key == 'Y')) {
            redo();
            return true;
        }
        // Ctrl+A: Select all
        if (ctrl && (event.key == 'a' || event.key == 'A')) {
            select_all();
            return true;
        }
        // Ctrl+F: Open search panel
        if (ctrl && (event.key == 'f' || event.key == 'F')) {
            show_search_panel();
            return true;
        }
        // G: Toggle grid
        if (event.key == 'g' || event.key == 'G') {
            design_canvas_->toggle_grid();
            return true;
        }
        // S: Toggle snap (without ctrl)
        if (!ctrl && (event.key == 's' || event.key == 'S')) {
            design_canvas_->toggle_snap();
            return true;
        }
        // R: Toggle rulers
        if (event.key == 'r' || event.key == 'R') {
            design_canvas_->toggle_rulers();
            return true;
        }
        // T: Toggle widget tree
        if (event.key == 't' || event.key == 'T') {
            toggle_widget_tree();
            return true;
        }
        // H: Toggle history panel
        if (event.key == 'h' || event.key == 'H') {
            toggle_history_panel();
            return true;
        }
        // Zoom shortcuts
        if (ctrl && (event.key == '=' || event.key == '+' || event.key == 0xBB)) {
            design_canvas_->zoom_in();
            return true;
        }
        if (ctrl && (event.key == '-' || event.key == 0xBD)) {
            design_canvas_->zoom_out();
            return true;
        }
        if (ctrl && (event.key == '0' || event.key == 0x30)) {
            design_canvas_->zoom_reset();
            return true;
        }
        if (ctrl && (event.key == '1' || event.key == 0x31)) {
            design_canvas_->zoom_fit();
            return true;
        }
        // ? or F1: Show shortcuts
        if (event.key == '?' || event.key == '/' || event.key == 0x70) { // 0x70 = F1
            shortcut_panel_->toggle();
            return true;
        }
        // P: Preview mode
        if (event.key == 'p' || event.key == 'P') {
            toggle_preview();
            return true;
        }
        // Ctrl+L: Lock
        if (ctrl && (event.key == 'l' || event.key == 'L')) {
            if (event.has_shift()) unlock_selected();
            else lock_selected();
            return true;
        }
        // Ctrl+G: Group
        if (ctrl && (event.key == 'g' || event.key == 'G')) {
            if (event.has_shift()) ungroup_selected();
            else group_selected();
            return true;
        }
        // Ctrl+T: New Tab
        if (ctrl && (event.key == 't' || event.key == 'T')) {
            new_tab();
            return true;
        }
        // Ctrl+W: Close Tab
        if (ctrl && (event.key == 'w' || event.key == 'W')) {
            close_tab(tab_bar_->active_id());
            return true;
        }
        // Tab: Select next widget
        if (event.key == 0x09) { // Tab key
            if (event.has_shift()) select_prev_widget();
            else select_next_widget();
            return true;
        }
        // Ctrl+Shift+C: Copy style
        if (ctrl && event.has_shift() && (event.key == 'c' || event.key == 'C')) {
            copy_style();
            return true;
        }
        // Ctrl+Shift+V: Paste style
        if (ctrl && event.has_shift() && (event.key == 'v' || event.key == 'V')) {
            paste_style();
            return true;
        }
        // F2 or Enter: Start inline edit (when not already editing)
        if ((event.key == 0x71 || event.key == '\r') && !selected_id_.empty() && !inline_editing_) { // F2 = 0x71
            start_inline_edit();
            return true;
        }
        // Enter: Confirm inline edit
        if (event.key == '\r' && inline_editing_) {
            end_inline_edit();
            return true;
        }
        // Escape: Cancel inline edit
        if (event.key == 0x1B && inline_editing_) {
            inline_editing_ = false;
            inline_edit_buffer_.clear();
            design_canvas_->set_inline_edit(false, &inline_edit_buffer_);
            return true;
        }
        // Backspace: Delete char in inline edit
        if ((event.key == '\b' || event.key == 0x08) && inline_editing_) {
            if (!inline_edit_buffer_.empty()) inline_edit_buffer_.pop_back();
            return true;
        }
        
        // Script editor key handling
        if (show_script_editor_ && script_editor_->is_editing()) {
            if (script_editor_->handle_key(event.key)) {
                return true;
            }
        }
    }
    
    // Handle inline editing text input
    if (inline_editing_ && event.type == meta_editor::EditorEvent::Type::TextInput) {
        inline_edit_buffer_ += event.text;
        return true;
    }
    
    // Handle script editor text input
    if (show_script_editor_ && script_editor_->is_editing() && 
        event.type == meta_editor::EditorEvent::Type::TextInput) {
        if (script_editor_->handle_text_input(event.text)) {
            return true;
        }
    }

    return false;
}

DesignWidget* Designer::create_widget(WidgetType type, float x, float y) {
    DesignWidget w;
    w.type = type;
    w.id = "widget_" + std::to_string(next_widget_id_++);
    w.x = x;
    w.y = y;

    // Default sizes and text based on type
    switch (type) {
        case WidgetType::Button:
            w.text = "Button";
            w.width = 100;
            w.height = 32;
            break;
        case WidgetType::Label:
            w.text = "Label";
            w.width = 80;
            w.height = 20;
            break;
        case WidgetType::Input:
            w.text = "Enter text...";
            w.width = 150;
            w.height = 32;
            break;
        case WidgetType::Checkbox:
            w.text = "Checkbox";
            w.width = 100;
            w.height = 24;
            break;
        case WidgetType::Switch:
            w.text = "Switch";
            w.width = 60;
            w.height = 28;
            break;
        case WidgetType::Slider:
            w.width = 150;
            w.height = 24;
            w.value = 50;
            break;
        case WidgetType::ProgressBar:
            w.width = 150;
            w.height = 20;
            w.value = 50;
            break;
        case WidgetType::Dropdown:
            w.text = "Select...";
            w.width = 150;
            w.height = 32;
            w.options = {"Option 1", "Option 2", "Option 3"};
            break;
        case WidgetType::Tabs:
            w.width = 300;
            w.height = 40;
            w.options = {"Tab 1", "Tab 2", "Tab 3"};
            break;
        case WidgetType::Card:
            w.text = "Card Title";
            w.width = 200;
            w.height = 150;
            break;
        case WidgetType::Divider:
            w.width = 200;
            w.height = 2;
            break;
        case WidgetType::Container:
            w.width = 250;
            w.height = 200;
            break;
    }

    widgets_.push_back(w);
    return &widgets_.back();
}

void Designer::delete_widget(const std::string& id) {
    auto it = std::find_if(widgets_.begin(), widgets_.end(),
        [&id](const DesignWidget& w) { return w.id == id; });
    
    if (it != widgets_.end()) {
        push_undo("Delete " + id);
        widgets_.erase(it);
        if (selected_id_ == id) {
            selected_id_.clear();
            property_editor_->set_widget(nullptr);
        }
    }
}

void Designer::select_widget(const std::string& id) {
    selected_id_ = id;
    selection_.clear();
    if (!id.empty()) selection_.push_back(id);
    
    auto it = std::find_if(widgets_.begin(), widgets_.end(),
        [&id](const DesignWidget& w) { return w.id == id; });
    
    if (it != widgets_.end()) {
        property_editor_->set_widget(&(*it));
        script_editor_->set_widget(&(*it));
    } else {
        property_editor_->set_widget(nullptr);
        script_editor_->set_widget(nullptr);
    }
}

void Designer::toggle_select(const std::string& id) {
    auto it = std::find(selection_.begin(), selection_.end(), id);
    if (it != selection_.end()) {
        selection_.erase(it);
    } else {
        selection_.push_back(id);
    }
    selected_id_ = selection_.empty() ? "" : selection_.back();
    
    // Use batch mode for multiple selection
    if (selection_.size() > 1) {
        property_editor_->set_widgets(get_selected_widgets());
    } else {
        property_editor_->set_widget(selected_widget());
    }
}

void Designer::select_all() {
    selection_.clear();
    for (auto& w : widgets_) selection_.push_back(w.id);
    selected_id_ = selection_.empty() ? "" : selection_.back();
    
    // Use batch mode for multiple selection
    if (selection_.size() > 1) {
        property_editor_->set_widgets(get_selected_widgets());
    } else {
        property_editor_->set_widget(selected_widget());
    }
}

void Designer::clear_selection() {
    selection_.clear();
    selected_id_.clear();
    property_editor_->set_widget(nullptr);
}

DesignWidget* Designer::selected_widget() {
    if (selected_id_.empty()) return nullptr;
    
    auto it = std::find_if(widgets_.begin(), widgets_.end(),
        [this](const DesignWidget& w) { return w.id == selected_id_; });
    
    return (it != widgets_.end()) ? &(*it) : nullptr;
}

std::vector<DesignWidget*> Designer::get_selected_widgets() {
    std::vector<DesignWidget*> result;
    for (auto& id : selection_) {
        for (auto& w : widgets_) {
            if (w.id == id) { result.push_back(&w); break; }
        }
    }
    return result;
}

void Designer::align_left() {
    auto sel = get_selected_widgets();
    if (sel.size() < 2) return;
    push_undo();
    float min_x = sel[0]->x;
    for (auto* w : sel) min_x = std::min(min_x, w->x);
    for (auto* w : sel) w->x = min_x;
}

void Designer::align_right() {
    auto sel = get_selected_widgets();
    if (sel.size() < 2) return;
    push_undo();
    float max_x = sel[0]->x + sel[0]->width;
    for (auto* w : sel) max_x = std::max(max_x, w->x + w->width);
    for (auto* w : sel) w->x = max_x - w->width;
}

void Designer::align_top() {
    auto sel = get_selected_widgets();
    if (sel.size() < 2) return;
    push_undo();
    float min_y = sel[0]->y;
    for (auto* w : sel) min_y = std::min(min_y, w->y);
    for (auto* w : sel) w->y = min_y;
}

void Designer::align_bottom() {
    auto sel = get_selected_widgets();
    if (sel.size() < 2) return;
    push_undo();
    float max_y = sel[0]->y + sel[0]->height;
    for (auto* w : sel) max_y = std::max(max_y, w->y + w->height);
    for (auto* w : sel) w->y = max_y - w->height;
}

void Designer::align_center_h() {
    auto sel = get_selected_widgets();
    if (sel.size() < 2) return;
    push_undo();
    float sum = 0;
    for (auto* w : sel) sum += w->x + w->width / 2;
    float center = sum / sel.size();
    for (auto* w : sel) w->x = center - w->width / 2;
}

void Designer::align_center_v() {
    auto sel = get_selected_widgets();
    if (sel.size() < 2) return;
    push_undo();
    float sum = 0;
    for (auto* w : sel) sum += w->y + w->height / 2;
    float center = sum / sel.size();
    for (auto* w : sel) w->y = center - w->height / 2;
}

void Designer::distribute_h() {
    auto sel = get_selected_widgets();
    if (sel.size() < 3) return;
    push_undo();
    std::sort(sel.begin(), sel.end(), [](auto* a, auto* b) { return a->x < b->x; });
    float start = sel.front()->x;
    float end = sel.back()->x;
    float spacing = (end - start) / (sel.size() - 1);
    for (size_t i = 0; i < sel.size(); ++i) sel[i]->x = start + i * spacing;
}

void Designer::distribute_v() {
    auto sel = get_selected_widgets();
    if (sel.size() < 3) return;
    push_undo();
    std::sort(sel.begin(), sel.end(), [](auto* a, auto* b) { return a->y < b->y; });
    float start = sel.front()->y;
    float end = sel.back()->y;
    float spacing = (end - start) / (sel.size() - 1);
    for (size_t i = 0; i < sel.size(); ++i) sel[i]->y = start + i * spacing;
}

void Designer::bring_to_front() {
    if (selected_id_.empty()) return;
    push_undo();
    for (size_t i = 0; i < widgets_.size(); ++i) {
        if (widgets_[i].id == selected_id_) {
            auto w = widgets_[i];
            widgets_.erase(widgets_.begin() + i);
            widgets_.push_back(w);
            break;
        }
    }
}

void Designer::send_to_back() {
    if (selected_id_.empty()) return;
    push_undo();
    for (size_t i = 0; i < widgets_.size(); ++i) {
        if (widgets_[i].id == selected_id_) {
            auto w = widgets_[i];
            widgets_.erase(widgets_.begin() + i);
            widgets_.insert(widgets_.begin(), w);
            break;
        }
    }
}

void Designer::bring_forward() {
    if (selected_id_.empty()) return;
    push_undo();
    for (size_t i = 0; i < widgets_.size() - 1; ++i) {
        if (widgets_[i].id == selected_id_) {
            std::swap(widgets_[i], widgets_[i + 1]);
            break;
        }
    }
}

void Designer::send_backward() {
    if (selected_id_.empty()) return;
    push_undo();
    for (size_t i = 1; i < widgets_.size(); ++i) {
        if (widgets_[i].id == selected_id_) {
            std::swap(widgets_[i], widgets_[i - 1]);
            break;
        }
    }
}

void Designer::lock_selected() {
    auto sel = get_selected_widgets();
    for (auto* w : sel) w->locked = true;
}

void Designer::unlock_selected() {
    auto sel = get_selected_widgets();
    for (auto* w : sel) w->locked = false;
}

bool Designer::is_widget_locked(const std::string& id) const {
    for (const auto& w : widgets_) {
        if (w.id == id) return w.locked;
    }
    return false;
}

void Designer::group_selected() {
    if (selection_.size() < 2) return;
    push_undo();
    
    std::string group_id = "group_" + std::to_string(next_widget_id_++);
    for (auto& id : selection_) {
        for (auto& w : widgets_) {
            if (w.id == id) {
                w.group_id = group_id;
                break;
            }
        }
    }
    
    if (status_bar_) status_bar_->set_message("Grouped " + std::to_string(selection_.size()) + " widgets");
}

void Designer::ungroup_selected() {
    if (selected_id_.empty()) return;
    
    std::string group_id;
    for (const auto& w : widgets_) {
        if (w.id == selected_id_ && !w.group_id.empty()) {
            group_id = w.group_id;
            break;
        }
    }
    
    if (group_id.empty()) return;
    
    push_undo();
    int count = 0;
    for (auto& w : widgets_) {
        if (w.group_id == group_id) {
            w.group_id.clear();
            count++;
        }
    }
    
    if (status_bar_) status_bar_->set_message("Ungrouped " + std::to_string(count) + " widgets");
}

void Designer::move_selected(float dx, float dy) {
    auto sel = get_selected_widgets();
    if (sel.empty()) return;
    
    // Check if any selected widget is locked
    bool any_locked = false;
    for (auto* w : sel) {
        if (w->locked) { any_locked = true; break; }
    }
    if (any_locked) {
        if (status_bar_) status_bar_->set_message("Cannot move locked widget");
        return;
    }
    
    if (sel.size() == 1) {
        push_undo("Move " + sel[0]->id);
    } else {
        push_undo("Move " + std::to_string(sel.size()) + " widgets");
    }
    for (auto* w : sel) {
        w->x += dx;
        w->y += dy;
    }
}

void Designer::copy_selected() {
    auto* w = selected_widget();
    if (w) clipboard_ = *w;
}

void Designer::paste() {
    if (!clipboard_) return;
    push_undo("Paste " + clipboard_->id);
    
    DesignWidget w = *clipboard_;
    w.id = "widget_" + std::to_string(next_widget_id_++);
    w.x += 20;
    w.y += 20;
    widgets_.push_back(w);
    select_widget(w.id);
}

void Designer::duplicate_selected() {
    copy_selected();
    paste();
}

void Designer::push_undo() {
    push_undo("Edit");
}

void Designer::push_undo(const std::string& description) {
    HistoryEntry entry;
    entry.state = widgets_;
    entry.description = description;
    undo_stack_.push_back(std::move(entry));
    redo_stack_.clear();
    if (undo_stack_.size() > 50) undo_stack_.erase(undo_stack_.begin());
    
    // Mark current tab as modified
    tab_bar_->set_modified(tab_bar_->active_id(), true);
}

void Designer::undo() {
    if (undo_stack_.empty()) return;
    
    HistoryEntry redo_entry;
    redo_entry.state = widgets_;
    redo_entry.description = undo_stack_.back().description;
    redo_stack_.push_back(std::move(redo_entry));
    
    widgets_ = undo_stack_.back().state;
    undo_stack_.pop_back();
    selected_id_.clear();
    property_editor_->set_widget(nullptr);
    
    // Mark as modified (undo is still a change from saved state)
    tab_bar_->set_modified(tab_bar_->active_id(), true);
}

void Designer::redo() {
    if (redo_stack_.empty()) return;
    
    HistoryEntry undo_entry;
    undo_entry.state = widgets_;
    undo_entry.description = redo_stack_.back().description;
    undo_stack_.push_back(std::move(undo_entry));
    
    widgets_ = redo_stack_.back().state;
    redo_stack_.pop_back();
    selected_id_.clear();
    property_editor_->set_widget(nullptr);
}

void Designer::jump_to_history(int index) {
    size_t undo_count = undo_stack_.size();
    size_t redo_count = redo_stack_.size();
    
    if (index < 0) return;
    
    // Jump to undo entry
    if (index < (int)undo_count) {
        // To restore undo_stack_[index], we need to undo (undo_count - index) times
        // Each undo pops from undo_stack_ and the last pop gives us undo_stack_[index]
        int steps = (int)undo_count - index;
        for (int i = 0; i < steps; ++i) {
            if (undo_stack_.empty()) break;
            HistoryEntry redo_entry;
            redo_entry.state = widgets_;
            redo_entry.description = undo_stack_.back().description;
            redo_stack_.push_back(std::move(redo_entry));
            widgets_ = undo_stack_.back().state;
            undo_stack_.pop_back();
        }
    }
    // Jump to redo entry
    else {
        int redo_index = index - (int)undo_count;
        if (redo_count > 0 && redo_index == 0) {
            // "(Current)" marker - no action
            return;
        }
        // Redo steps
        int steps = redo_index;
        for (int i = 0; i < steps; ++i) {
            if (redo_stack_.empty()) break;
            HistoryEntry undo_entry;
            undo_entry.state = widgets_;
            undo_entry.description = redo_stack_.back().description;
            undo_stack_.push_back(std::move(undo_entry));
            widgets_ = redo_stack_.back().state;
            redo_stack_.pop_back();
        }
    }
    
    selected_id_.clear();
    property_editor_->set_widget(nullptr);
    tab_bar_->set_modified(tab_bar_->active_id(), true);
}

void Designer::show_search_panel() {
    search_panel_->show();
}

void Designer::hide_search_panel() {
    search_panel_->hide();
}

bool Designer::is_search_panel_visible() const {
    return search_panel_->is_visible();
}

void Designer::copy_style() {
    auto* w = selected_widget();
    if (!w) return;
    style_clipboard_.bg_color = w->bg_color;
    style_clipboard_.text_color = w->text_color;
    style_clipboard_.border_color = w->border_color;
    style_clipboard_.border_width = w->border_width;
    style_clipboard_.border_radius = w->border_radius;
    style_clipboard_.font_size = w->font_size;
    style_clipboard_.font_bold = w->font_bold;
    style_clipboard_.padding = w->padding;
    style_clipboard_.valid = true;
}

void Designer::paste_style() {
    if (!style_clipboard_.valid) return;
    auto widgets = get_selected_widgets();
    if (widgets.empty()) return;
    
    push_undo();
    for (auto* w : widgets) {
        w->bg_color = style_clipboard_.bg_color;
        w->text_color = style_clipboard_.text_color;
        w->border_color = style_clipboard_.border_color;
        w->border_width = style_clipboard_.border_width;
        w->border_radius = style_clipboard_.border_radius;
        w->font_size = style_clipboard_.font_size;
        w->font_bold = style_clipboard_.font_bold;
        w->padding = style_clipboard_.padding;
    }
}

void Designer::select_next_widget() {
    if (widgets_.empty()) return;
    
    int current_idx = -1;
    for (int i = 0; i < (int)widgets_.size(); ++i) {
        if (widgets_[i].id == selected_id_) { current_idx = i; break; }
    }
    
    int next_idx = (current_idx + 1) % widgets_.size();
    select_widget(widgets_[next_idx].id);
}

void Designer::select_prev_widget() {
    if (widgets_.empty()) return;
    
    int current_idx = 0;
    for (int i = 0; i < (int)widgets_.size(); ++i) {
        if (widgets_[i].id == selected_id_) { current_idx = i; break; }
    }
    
    int prev_idx = (current_idx - 1 + widgets_.size()) % widgets_.size();
    select_widget(widgets_[prev_idx].id);
}

void Designer::start_inline_edit() {
    auto* w = selected_widget();
    if (!w) return;
    if (w->type != WidgetType::Button && w->type != WidgetType::Label && 
        w->type != WidgetType::Input && w->type != WidgetType::Card) return;
    
    inline_editing_ = true;
    inline_edit_buffer_ = w->text;
    design_canvas_->set_inline_edit(true, &inline_edit_buffer_);
}

void Designer::end_inline_edit() {
    if (!inline_editing_) return;
    
    auto* w = selected_widget();
    if (w && w->text != inline_edit_buffer_) {
        push_undo();
        w->text = inline_edit_buffer_;
    }
    inline_editing_ = false;
    inline_edit_buffer_.clear();
    design_canvas_->set_inline_edit(false, &inline_edit_buffer_);
}

std::string Designer::generate_code() const {
    return code_gen_->generate(widgets_);
}

bool Designer::save_code(const std::string& path) const {
    return code_gen_->save(path, generate_code());
}

void Designer::new_project() {
    widgets_.clear();
    selected_id_.clear();
    property_editor_->set_widget(nullptr);
    next_widget_id_ = 1;
}

bool Designer::save_project(const std::string& path) const {
    std::ofstream file(path);
    if (!file) return false;

    file << "{\n  \"widgets\": [\n";
    for (size_t i = 0; i < widgets_.size(); ++i) {
        const auto& w = widgets_[i];
        file << "    {\n";
        file << "      \"type\": " << (int)w.type << ",\n";
        file << "      \"id\": \"" << w.id << "\",\n";
        file << "      \"x\": " << w.x << ", \"y\": " << w.y << ",\n";
        file << "      \"width\": " << w.width << ", \"height\": " << w.height << ",\n";
        file << "      \"text\": \"" << w.text << "\",\n";
        file << "      \"value\": " << w.value << ",\n";
        file << "      \"min\": " << w.min_value << ", \"max\": " << w.max_value;
        if (!w.options.empty()) {
            file << ",\n      \"options\": [";
            for (size_t j = 0; j < w.options.size(); ++j) {
                file << "\"" << w.options[j] << "\"";
                if (j < w.options.size() - 1) file << ", ";
            }
            file << "]";
        }
        file << "\n    }";
        if (i < widgets_.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ]\n}\n";
    return true;
}

bool Designer::load_project(const std::string& path) {
    std::ifstream file(path);
    if (!file) return false;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    widgets_.clear();
    selected_id_.clear();
    property_editor_->set_widget(nullptr);
    next_widget_id_ = 1;

    size_t pos = 0;
    while ((pos = content.find("\"type\":", pos)) != std::string::npos) {
        DesignWidget w;
        
        auto read_int = [&](const char* key) -> int {
            size_t p = content.find(key, pos);
            if (p == std::string::npos) return 0;
            p += strlen(key);
            while (p < content.size() && (content[p] == ' ' || content[p] == ':')) p++;
            return std::atoi(content.c_str() + p);
        };
        
        auto read_float = [&](const char* key) -> float {
            size_t p = content.find(key, pos);
            if (p == std::string::npos) return 0;
            p += strlen(key);
            while (p < content.size() && (content[p] == ' ' || content[p] == ':')) p++;
            return (float)std::atof(content.c_str() + p);
        };
        
        auto read_str = [&](const char* key) -> std::string {
            size_t p = content.find(key, pos);
            if (p == std::string::npos) return "";
            p = content.find('"', p + strlen(key));
            if (p == std::string::npos) return "";
            size_t end = content.find('"', p + 1);
            if (end == std::string::npos) return "";
            return content.substr(p + 1, end - p - 1);
        };

        w.type = (WidgetType)read_int("\"type\"");
        w.id = read_str("\"id\"");
        w.x = read_float("\"x\"");
        w.y = read_float("\"y\"");
        w.width = read_float("\"width\"");
        w.height = read_float("\"height\"");
        w.text = read_str("\"text\"");
        w.value = read_float("\"value\"");
        w.min_value = read_float("\"min\"");
        w.max_value = read_float("\"max\"");

        if (!w.id.empty()) {
            widgets_.push_back(w);
            int id_num = 0;
            if (sscanf(w.id.c_str(), "widget_%d", &id_num) == 1)
                next_widget_id_ = std::max(next_widget_id_, id_num + 1);
        }

        pos++;
    }

    return true;
}

// Context menu
void Designer::show_context_menu(float x, float y) {
    context_menu_x_ = x;
    context_menu_y_ = y;
    context_menu_visible_ = true;
    
    context_menu_items_.clear();
    
    if (!selected_id_.empty()) {
        context_menu_items_.push_back({"Cut", "Ctrl+X", [this]() { copy_selected(); delete_widget(selected_id_); }});
        context_menu_items_.push_back({"Copy", "Ctrl+C", [this]() { copy_selected(); }});
        context_menu_items_.push_back({"Duplicate", "Ctrl+D", [this]() { duplicate_selected(); }});
        context_menu_items_.push_back({"Delete", "Del", [this]() { delete_widget(selected_id_); }});
        context_menu_items_.push_back({"", "", nullptr, true}); // separator
        context_menu_items_.push_back({"Bring to Front", "", [this]() { bring_to_front(); }});
        context_menu_items_.push_back({"Send to Back", "", [this]() { send_to_back(); }});
    } else {
        context_menu_items_.push_back({"Paste", "Ctrl+V", [this]() { paste(); }});
        context_menu_items_.push_back({"Select All", "Ctrl+A", [this]() { select_all(); }});
    }
}

void Designer::render_context_menu(flex::Renderer& renderer) {
    constexpr float ITEM_H = 26.0f;
    constexpr float PADDING = 8.0f;
    constexpr float WIDTH = 160.0f;
    
    float height = 0;
    for (const auto& item : context_menu_items_) {
        height += item.separator ? 8.0f : ITEM_H;
    }
    
    // Background
    flex::Paint bg = flex::Paint::solid(flex::Color{0.18f, 0.18f, 0.2f, 0.98f});
    flex::Paint border = flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.35f, 1});
    renderer.draw_rect(context_menu_x_, context_menu_y_, WIDTH, height + PADDING * 2, 6, bg, border, 1);
    
    float y = context_menu_y_ + PADDING;
    for (const auto& item : context_menu_items_) {
        if (item.separator) {
            renderer.draw_rect(context_menu_x_ + 8, y + 3, WIDTH - 16, 1, 0, 
                flex::Paint::solid(flex::Color{0.35f, 0.35f, 0.4f, 1}), flex::Paint::none(), 0);
            y += 8.0f;
        } else {
            renderer.draw_text(item.label, context_menu_x_ + 12, y + 17, "sans", 12, false, {0.9f, 0.9f, 0.9f, 1});
            if (!item.shortcut.empty()) {
                renderer.draw_text(item.shortcut, context_menu_x_ + WIDTH - 60, y + 17, "sans", 10, false, {0.5f, 0.5f, 0.55f, 1});
            }
            y += ITEM_H;
        }
    }
}

bool Designer::handle_context_menu_click(float x, float y) {
    constexpr float ITEM_H = 26.0f;
    constexpr float PADDING = 8.0f;
    constexpr float WIDTH = 160.0f;
    
    if (x < context_menu_x_ || x > context_menu_x_ + WIDTH) {
        context_menu_visible_ = false;
        return false;
    }
    
    float cy = context_menu_y_ + PADDING;
    for (const auto& item : context_menu_items_) {
        float item_h = item.separator ? 8.0f : ITEM_H;
        if (!item.separator && y >= cy && y < cy + item_h) {
            if (item.action) item.action();
            context_menu_visible_ = false;
            return true;
        }
        cy += item_h;
    }
    
    context_menu_visible_ = false;
    return false;
}

void Designer::new_tab(const std::string& name) {
    static int tab_counter = 1;
    std::string tab_name = name.empty() ? "Untitled " + std::to_string(tab_counter++) : name;
    std::string tab_id = "tab_" + std::to_string(tab_counter++);
    
    // Save current tab's widgets first
    if (!active_unit_name_.empty()) {
        auto* current_unit = unit_manager_->get_unit(active_unit_name_);
        if (current_unit) {
            current_unit->widgets = widgets_;
        }
    }
    
    // Create unit for this tab
    auto* unit = unit_manager_->create_unit(tab_name);
    if (unit) {
        tab_to_unit_[tab_id] = tab_name;
        active_unit_name_ = tab_name;
        
        // Clear canvas for new tab
        widgets_.clear();
        selected_id_.clear();
        selection_.clear();
        undo_stack_.clear();
        redo_stack_.clear();
        property_editor_->set_widget(nullptr);
        
        tab_bar_->add_tab(tab_id, tab_name);
        tab_bar_->set_active(tab_id);
    }
}

void Designer::close_tab(const std::string& id) {
    if (tab_bar_->tab_count() <= 1) return;
    
    auto it = tab_to_unit_.find(id);
    if (it == tab_to_unit_.end()) return;
    
    std::string unit_name = it->second;
    bool is_active = (id == tab_bar_->active_id());
    
    // Delete unit
    unit_manager_->delete_unit(unit_name);
    tab_to_unit_.erase(it);
    tab_bar_->remove_tab(id);
    
    // If closing active tab, switch_tab callback will load new active
    if (is_active && !tab_bar_->active_id().empty()) {
        switch_tab(tab_bar_->active_id());
    }
}

void Designer::switch_tab(const std::string& id) {
    if (id == tab_bar_->active_id() && !active_unit_name_.empty()) return;
    
    // Save current widgets to current unit
    if (!active_unit_name_.empty()) {
        auto* current_unit = unit_manager_->get_unit(active_unit_name_);
        if (current_unit) {
            current_unit->widgets = widgets_;
        }
    }
    
    // Find target unit
    auto it = tab_to_unit_.find(id);
    if (it == tab_to_unit_.end()) return;
    
    std::string target_unit_name = it->second;
    auto* target_unit = unit_manager_->get_unit(target_unit_name);
    if (!target_unit) return;
    
    // Load widgets from target unit
    widgets_ = target_unit->widgets;
    active_unit_name_ = target_unit_name;
    
    // Reset selection state
    selected_id_.clear();
    selection_.clear();
    undo_stack_.clear();
    redo_stack_.clear();
    property_editor_->set_widget(nullptr);
    
    tab_bar_->set_active(id);
}

const std::string& Designer::active_tab_id() const {
    return tab_bar_->active_id();
}

} // namespace flexui_designer
