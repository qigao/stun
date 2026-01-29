/*
 * Full Editor Application - Implementation
 *
 * Reference implementation showing all built-in panels composed together.
 */

#include "full_editor_app.h"
#include <meta_editor/core/sdl_adapter.h>
#include <meta_editor/view/context_toolbar.h>
#include <meta_editor/view/zoom_panel.h>
#include <meta_editor/view/navigator_panel.h>
#include <meta_editor/view/shape_collection_panel.h>
#include <meta_editor/view/tool_panel.h>
#include <meta_editor/view/properties_panel.h>
#include <meta_editor/view/layers_panel.h>
#include <meta_editor/view/align_panel.h>
#include <meta_editor/view/shortcut_overlay.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <SDL2/SDL.h>
#include <fstream>
#include <sstream>
#include <iostream>

namespace meta_editor {

class WorkspaceWidget : public flexUI::Widget {
public:
    explicit WorkspaceWidget(FullEditorApp* app) : app_(app) {}

    void render(const flexUI::Element& elem, flexUI::Renderer& renderer) override {
        app_->canvas()->set_size(elem.width(), elem.height());
        app_->canvas()->render(renderer.flex());
        app_->editor()->render_tool_overlay(renderer.flex());

        app_->render_tool_panel(renderer.flex());
        app_->render_shape_panel(renderer.flex());
        app_->render_properties_panel(renderer.flex());
        app_->render_layers_panel(renderer.flex());
        app_->render_align_panel(renderer.flex());
        app_->render_context_toolbar(renderer.flex());
        app_->render_navigator(renderer.flex());
        app_->render_shortcut_overlay(renderer.flex());
    }

    const char* type_name() const override { return "WorkspaceWidget"; }

private:
    FullEditorApp* app_;
};

namespace {
    flexUI::MouseButton sdl_to_flex_button(Uint8 button) {
        if (button == SDL_BUTTON_RIGHT) return flexUI::MouseButton::Right;
        if (button == SDL_BUTTON_MIDDLE) return flexUI::MouseButton::Middle;
        return flexUI::MouseButton::Left;
    }

    std::string load_css_file(const std::string& path) {
        std::ifstream file(path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }
        return R"(
            #root { width: 100%; height: 100%; background-color: #1a1a1a; }
            #workspace_container { width: 100%; height: 100%; background-color: #1a1a1a; }
        )";
    }
}

FullEditorApp::FullEditorApp(float width, float height)
    : width_(width), height_(height) {}

FullEditorApp::~FullEditorApp() = default;

void FullEditorApp::init(flex::Renderer* ui_renderer) {
    editor_ = std::make_unique<Editor>(width_, height_);
    editor_->init();

    context_toolbar_ = std::make_unique<ContextToolbar>(canvas(), selection());
    zoom_panel_ = std::make_unique<ZoomPanel>(canvas());
    navigator_panel_ = std::make_unique<NavigatorPanel>(canvas());

    shape_panel_ = std::make_unique<ShapeCollectionPanel>(canvas());
    shape_panel_->set_position(16, 300);
    shape_panel_->set_shape_added_callback([this](flex::Shape* shape) {
        selection()->select(shape);
    });

    tool_panel_ = std::make_unique<ToolPanel>(tools());

    properties_panel_ = std::make_unique<PropertiesPanel>(canvas(), selection());
    properties_panel_->set_position(width_ - 196, 16);

    layers_panel_ = std::make_unique<LayersPanel>(canvas(), selection());
    layers_panel_->set_position(width_ - 196, 250);

    align_panel_ = std::make_unique<AlignPanel>(canvas(), selection());
    align_panel_->set_position(width_ - 196, 450);

    shortcut_overlay_ = std::make_unique<ShortcutOverlay>();

    panels_ = {
        tool_panel_.get(),
        shape_panel_.get(),
        navigator_panel_.get(),
        properties_panel_.get(),
        layers_panel_.get(),
        align_panel_.get()
    };

    setup_panel_callbacks();

    if (ui_renderer) {
        ui_box_ = std::make_unique<flexUI::Box>(ui_renderer);
        ui_box_->set_viewport(width_, height_);
    }

    setup_ui();

    editor_->set_change_callback([this]() {
        if (ui_box_) ui_box_->invalidate();
    });
}

void FullEditorApp::shutdown() {
    editor_->shutdown();
}

void FullEditorApp::setup_panel_callbacks() {
    align_panel_->set_group_callback([this]() { editor_->group_selection(); });
    align_panel_->set_ungroup_callback([this]() { editor_->ungroup_selection(); });
}

void FullEditorApp::setup_ui() {
    if (!ui_box_) return;

    ui_box_->load_css(load_css_file("assets/editor.css"));

    auto* root = ui_box_->create("div", "root");
    ui_box_->set_root(root);

    auto* workspace = ui_box_->create_with_widget("div", new WorkspaceWidget(this), "workspace_container");
    root->append(workspace);

    if (zoom_panel_) zoom_panel_->setup_ui(ui_box_.get(), root);
    if (navigator_panel_) navigator_panel_->set_position(width_ - 160, height_ - 110);
}

void FullEditorApp::update(float dt) {
    editor_->update(dt);
    if (context_toolbar_) context_toolbar_->update();
    if (zoom_panel_) zoom_panel_->update();
    if (ui_box_) {
        ui_box_->update_time(dt * 1000.0f);
        ui_box_->update();
    }
}

void FullEditorApp::render(flex::Renderer& renderer) {
    if (!ui_box_) {
        editor_->render(renderer);
        editor_->render_tool_overlay(renderer);
    }
}

void FullEditorApp::set_viewport(float width, float height) {
    width_ = width;
    height_ = height;
    editor_->set_viewport(width, height);
    if (ui_box_) ui_box_->set_viewport(width, height);
    if (navigator_panel_) navigator_panel_->set_position(width - 160, height - 110);
    if (properties_panel_) properties_panel_->set_position(width - 196, 16);
    if (layers_panel_) layers_panel_->set_position(width - 196, 250);
    if (align_panel_) align_panel_->set_position(width - 196, 450);
}

bool FullEditorApp::handle_event(const SDL_Event& event) {
    if (ui_box_) {
        flexUI::Event ui_event;
        bool has_ui_event = false;

        if (event.type == SDL_MOUSEMOTION) {
            ui_event = flexUI::Event::mouse_move((float)event.motion.x, (float)event.motion.y);
            has_ui_event = true;
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
            ui_event = flexUI::Event::mouse_down((float)event.button.x, (float)event.button.y,
                                                  sdl_to_flex_button(event.button.button));
            has_ui_event = true;
        } else if (event.type == SDL_MOUSEBUTTONUP) {
            ui_event = flexUI::Event::mouse_up((float)event.button.x, (float)event.button.y,
                                                sdl_to_flex_button(event.button.button));
            has_ui_event = true;
        } else if (event.type == SDL_MOUSEWHEEL) {
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            ui_event = flexUI::Event::mouse_wheel((float)mx, (float)my,
                                                   (float)event.wheel.x, (float)event.wheel.y);
            has_ui_event = true;
        }

        if (has_ui_event) {
            ui_box_->dispatch_event(ui_event);
            if (ui_event.handled) return true;
        }
    }

    EditorEvent editor_event = sdl_to_editor_event(event);
    if (editor_event.type == EditorEvent::Type::None) return false;

    if (editor_event.type == EditorEvent::Type::PointerDown) {
        if (shortcut_overlay_ && shortcut_overlay_->handle_click(editor_event.x, editor_event.y)) {
            return true;
        }
        if (editor_event.button == MouseButton::Left) {
            for (auto* panel : panels_) {
                if (panel && panel->handle_pointer_down(editor_event.x, editor_event.y)) {
                    return true;
                }
            }
            if (context_toolbar_ && context_toolbar_->handle_click(editor_event.x, editor_event.y)) {
                return true;
            }
        }
    }

    if (editor_event.type == EditorEvent::Type::PointerMove) {
        for (auto* panel : panels_) {
            if (panel && panel->handle_drag_move(editor_event.x, editor_event.y)) {
                return true;
            }
        }
        if (navigator_panel_ && navigator_panel_->handle_drag(editor_event.x, editor_event.y)) {
            return true;
        }
    }

    if (editor_event.type == EditorEvent::Type::PointerUp) {
        for (auto* panel : panels_) {
            if (panel) panel->handle_drag_end();
        }
        if (navigator_panel_) navigator_panel_->end_drag();
    }

    if (editor_event.type == EditorEvent::Type::KeyDown) {
        int key = editor_event.key;
        if (key == SDLK_ESCAPE && shortcut_overlay_ && shortcut_overlay_->visible()) {
            shortcut_overlay_->hide();
            return true;
        }
        if (key == SDLK_SLASH && shortcut_overlay_) {
            shortcut_overlay_->toggle();
            return true;
        }
        if (editor_event.has_ctrl() && (key == SDLK_s || key == 's')) {
            if (!editor_event.has_shift()) {
                save_project("project.json");
                return true;
            } else {
                editor_->save_svg("output.svg");
                return true;
            }
        }
        if (editor_event.has_ctrl() && (key == SDLK_o || key == 'o')) {
            load_project("project.json");
            return true;
        }
    }

    return editor_->handle_event(editor_event);
}

void FullEditorApp::render_context_toolbar(flex::Renderer& r) { if (context_toolbar_) context_toolbar_->render(r); }
void FullEditorApp::render_navigator(flex::Renderer& r) { if (navigator_panel_) navigator_panel_->render(r); }
void FullEditorApp::render_shape_panel(flex::Renderer& r) { if (shape_panel_) shape_panel_->render(r); }
void FullEditorApp::render_tool_panel(flex::Renderer& r) { if (tool_panel_) tool_panel_->render(r); }
void FullEditorApp::render_properties_panel(flex::Renderer& r) { if (properties_panel_) properties_panel_->render(r); }
void FullEditorApp::render_layers_panel(flex::Renderer& r) { if (layers_panel_) layers_panel_->render(r); }
void FullEditorApp::render_align_panel(flex::Renderer& r) { if (align_panel_) align_panel_->render(r); }
void FullEditorApp::render_shortcut_overlay(flex::Renderer& r) { if (shortcut_overlay_) shortcut_overlay_->render(r, width_, height_); }

} // namespace meta_editor
