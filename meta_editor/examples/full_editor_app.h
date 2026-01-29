/*
 * Full Editor Application
 *
 * Example showing how to compose Editor + all built-in panels.
 * This is NOT part of the SDK - just a reference implementation.
 *
 * For custom applications, use Editor directly and add only the panels you need.
 * See: sdk_minimal.cpp, line_tool_demo.cpp
 */

#pragma once

#include <meta_editor/core/editor.h>
#include <meta_editor/view/panel.h>
#include <flexUI.h>
#include <memory>
#include <vector>

union SDL_Event;

namespace meta_editor {

class ContextToolbar;
class ZoomPanel;
class NavigatorPanel;
class ShapeCollectionPanel;
class ToolPanel;
class PropertiesPanel;
class LayersPanel;
class AlignPanel;
class ShortcutOverlay;

class FullEditorApp {
public:
    FullEditorApp(float width, float height);
    ~FullEditorApp();

    void init(flex::Renderer* ui_renderer);
    void shutdown();
    void update(float dt);
    void render(flex::Renderer& renderer);
    bool handle_event(const SDL_Event& event);
    void set_viewport(float width, float height);

    Editor* editor() { return editor_.get(); }
    Canvas* canvas() { return editor_->canvas(); }
    SelectionManager* selection() { return editor_->selection(); }
    ToolManager* tools() { return editor_->tools(); }

    bool save_project(const std::string& path) { return editor_->save_project(path); }
    bool load_project(const std::string& path) { return editor_->load_project(path); }

    void render_context_toolbar(flex::Renderer& renderer);
    void render_navigator(flex::Renderer& renderer);
    void render_shape_panel(flex::Renderer& renderer);
    void render_tool_panel(flex::Renderer& renderer);
    void render_properties_panel(flex::Renderer& renderer);
    void render_layers_panel(flex::Renderer& renderer);
    void render_align_panel(flex::Renderer& renderer);
    void render_shortcut_overlay(flex::Renderer& renderer);

private:
    void setup_ui();
    void setup_panel_callbacks();

    std::unique_ptr<Editor> editor_;
    std::unique_ptr<flexUI::Box> ui_box_;

    std::unique_ptr<ContextToolbar> context_toolbar_;
    std::unique_ptr<ZoomPanel> zoom_panel_;
    std::unique_ptr<NavigatorPanel> navigator_panel_;
    std::unique_ptr<ShapeCollectionPanel> shape_panel_;
    std::unique_ptr<ToolPanel> tool_panel_;
    std::unique_ptr<PropertiesPanel> properties_panel_;
    std::unique_ptr<LayersPanel> layers_panel_;
    std::unique_ptr<AlignPanel> align_panel_;
    std::unique_ptr<ShortcutOverlay> shortcut_overlay_;

    std::vector<Panel*> panels_;
    float width_;
    float height_;
};

} // namespace meta_editor
