/*
 * Meta Editor SDK - Core Editor
 *
 * Platform-agnostic editor core. No UI dependencies.
 * This is the main entry point for SDK users.
 *
 * Usage:
 *   Editor editor(800, 600);
 *   editor.init(renderer);
 *   editor.handle_event(event);  // Platform-agnostic events
 *   editor.update(dt);
 *   editor.render(renderer);
 */

#pragma once

#include "editor_event.h"
#include "../canvas.h"
#include "../selection_manager.h"
#include "../tool_manager.h"
#include "../command.h"
#include "../exporter.h"
#include "../serializer.h"
#include "../connector.h"
#include "../svg_importer.h"
#include "../page.h"
#include <flex.h>
#include <memory>
#include <string>
#include <functional>

namespace meta_editor {

// Clipboard data for copy/paste (public for SDK users to extend)
struct ClipboardShape {
    flex::GeometryType geometry_type;
    float x, y, width, height;
    flex::Color fill_color;
    flex::Color stroke_color;
    float stroke_width;
    std::string path_data;
    int sides;
    int points;
    float radius, inner_radius;
};

/**
 * Editor - Core editing engine (SDK entry point)
 *
 * Provides:
 * - Canvas with layers and camera
 * - Selection management
 * - Tool system (extensible)
 * - Command system (undo/redo)
 * - Import/Export
 *
 * Does NOT provide:
 * - UI panels (use meta_editor::Panel subclasses or build your own)
 * - Platform event handling (convert to EditorEvent first)
 */
class Editor {
public:
    Editor(float width, float height);
    ~Editor();

    // Lifecycle
    void init();
    void shutdown();

    // Core loop
    void update(float dt);
    void render(flex::Renderer& renderer);
    void render_tool_overlay(flex::Renderer& renderer);

    // Event handling (platform-agnostic)
    bool handle_event(const EditorEvent& event);

    // Viewport
    void set_viewport(float width, float height);
    float width() const { return width_; }
    float height() const { return height_; }

    // Core systems access
    PageManager* page_manager() { return page_manager_.get(); }
    Page* active_page() { return page_manager_->active_page(); }

    Canvas* canvas();
    SelectionManager* selection();
    ToolManager* tools() { return tool_manager_.get(); }
    CommandManager* command_manager();
    ConnectorManager* connectors();

    // Serialization
    bool save_project(const std::string& path);
    bool load_project(const std::string& path);
    std::string to_json() const;
    bool from_json(const std::string& json);

    // Export
    bool save_svg(const std::string& path);
    bool save_png(const std::string& path, const uint32_t* buffer, int w, int h);
    std::string selection_to_svg();

    // Import
    bool import_svg(const std::string& path);
    bool import_svg_string(const std::string& svg_content);

    // Clipboard
    void copy_selection();
    void paste();
    void duplicate_selection();
    bool has_clipboard() const { return !clipboard_.empty(); }

    // Selection shortcuts
    void select_all();
    void delete_selection();
    void group_selection();
    void ungroup_selection();
    void lock_selection();
    void unlock_selection();
    
    // Notification
    void notify_change();

    // Page handling
    void set_active_page(int index);

    // Callbacks for UI integration
    using ChangeCallback = std::function<void()>;
    using ContextMenuCallback = std::function<void(float x, float y)>;
    void set_change_callback(ChangeCallback cb) { change_callback_ = std::move(cb); }
    void set_shortcut_callback(ChangeCallback cb) { shortcut_callback_ = std::move(cb); }
    void set_context_menu_callback(ContextMenuCallback cb) { context_menu_callback_ = std::move(cb); }

protected:
    void setup_default_tools();

    // Handle specific event types
    bool handle_pointer_down(const EditorEvent& event);
    bool handle_pointer_move(const EditorEvent& event);
    bool handle_pointer_up(const EditorEvent& event);
    bool handle_key_down(const EditorEvent& event);
    bool handle_key_up(const EditorEvent& event);

    // Core systems
    std::unique_ptr<PageManager> page_manager_;
    std::unique_ptr<ToolManager> tool_manager_;

    // State
    float width_;
    float height_;
    std::vector<ClipboardShape> clipboard_;
    
    // Space key temporary tool switch
    std::string previous_tool_;
    bool space_held_ = false;

    // Callbacks
    ChangeCallback change_callback_;
    ChangeCallback shortcut_callback_;
    ContextMenuCallback context_menu_callback_;
};

} // namespace meta_editor
