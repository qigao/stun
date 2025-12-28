/*
 * Meta Editor - Main Application
 * 
 * Top-level editor class integrating all components.
 */

#pragma once

#include "canvas.h"
#include "selection_manager.h"
#include "tool_manager.h"
#include "command.h"
union SDL_Event;

namespace flexUI { class Box; }

#include <flexUI.h>
#include <memory>
#include <string>

namespace meta_editor {

class ContextToolbar;
class Exporter;
class ZoomPanel;
class NavigatorPanel;
class ShapeCollectionPanel;
class ToolPanel;

/**
 * MetaEditor - Main application class
 * 
 * Integrates:
 * - Canvas (layers, camera)
 * - Tools (select, pen, shape, etc.)
 * - UI Panels (toolbar, layers, properties)
 * - Commands (undo/redo)
 */
class MetaEditor {
public:
    MetaEditor(float width, float height);
    ~MetaEditor();
    
    // Lifecycle
    void init(flex::Renderer* ui_renderer);
    void shutdown();
    
    // Update & Render
    void update(float dt);
    void render(flex::Renderer& renderer);
    
    // Events
    bool handle_event(const union SDL_Event& event);

    // Viewport
    void set_viewport(float width, float height);

    // Access
    Canvas* canvas() { return canvas_.get(); }
    SelectionManager* selection() { return selection_.get(); }
    ToolManager* tools() { return tool_manager_.get(); }
    CommandManager* commands() { return command_manager_.get(); }

    // Context toolbar rendering (called by WorkspaceWidget)
    void render_context_toolbar(flex::Renderer& renderer);
    void render_navigator(flex::Renderer& renderer);
    void render_shape_panel(flex::Renderer& renderer);
    void render_tool_panel(flex::Renderer& renderer);

private:
    void setup_tools();
    void setup_ui();

    // Core systems
    std::unique_ptr<Canvas> canvas_;
    std::unique_ptr<SelectionManager> selection_;
    std::unique_ptr<ToolManager> tool_manager_;
    std::unique_ptr<CommandManager> command_manager_;

    // UI
    std::unique_ptr<flexUI::Box> ui_box_;
    std::unique_ptr<ContextToolbar> context_toolbar_;
    std::unique_ptr<Exporter> exporter_;
    std::unique_ptr<ZoomPanel> zoom_panel_;
    std::unique_ptr<NavigatorPanel> navigator_panel_;
    std::unique_ptr<ShapeCollectionPanel> shape_panel_;
    std::unique_ptr<ToolPanel> tool_panel_;

    // Window size
    float width_;
    float height_;
};

} // namespace meta_editor
