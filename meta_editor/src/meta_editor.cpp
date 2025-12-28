#include "meta_editor/meta_editor.h"
#include "meta_editor/tool.h"
#include "meta_editor/selection_manager.h"
#include "meta_editor/tool_manager.h"
#include "meta_editor/command.h"
#include "meta_editor/canvas.h"
#include "meta_editor/exporter.h"
#include "meta_editor/tools/select_tool.h"
#include "meta_editor/tools/pen_tool.h"
#include "meta_editor/tools/shape_tool.h"
#include "meta_editor/view/context_toolbar.h"
#include "meta_editor/view/zoom_panel.h"
#include "meta_editor/view/navigator_panel.h"
#include "meta_editor/view/shape_collection_panel.h"
#include "meta_editor/view/tool_panel.h"
#include "flexUI/box.h"
#include "flexUI/element.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

namespace meta_editor {

/**
 * WorkspaceWidget - Renders the 2D canvas and tool overlays
 * within a flexUI element.
 */
class WorkspaceWidget : public flexUI::Widget {
public:
    explicit WorkspaceWidget(MetaEditor* editor) : editor_(editor) {}

    void render(const flexUI::Element& elem, flexUI::Renderer& renderer) override {
        // Update canvas size to match UI element
        editor_->canvas()->set_size(elem.width(), elem.height());

        // Render canvas
        editor_->canvas()->render(renderer.flex());

        // Render tool overlays
        editor_->tools()->render_overlay(renderer.flex());

        // Render UI panels
        editor_->render_tool_panel(renderer.flex());
        editor_->render_shape_panel(renderer.flex());
        editor_->render_context_toolbar(renderer.flex());
        editor_->render_navigator(renderer.flex());
    }

    bool handle_event(const flexUI::Event& event, flexUI::Element& elem) override {
        // Optionally handle events directly here or let MetaEditor::handle_event do it
        return false;
    }

    const char* type_name() const override { return "WorkspaceWidget"; }

private:
    MetaEditor* editor_;
};

namespace {
    // Helper to map SDL buttons to flexUI buttons
    flexUI::MouseButton sdl_to_flex_button(Uint8 button) {
        if (button == SDL_BUTTON_RIGHT) return flexUI::MouseButton::Right;
        if (button == SDL_BUTTON_MIDDLE) return flexUI::MouseButton::Middle;
        return flexUI::MouseButton::Left;
    }

    // Helper to load CSS from file
    std::string load_css_file(const std::string& path) {
        // Try multiple paths
        std::vector<std::string> paths = {
            path,
            "../assets/editor.css",
            "../../meta_editor/assets/editor.css",
            "../../../meta_editor/assets/editor.css"
        };

        for (const auto& p : paths) {
            std::ifstream file(p);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                std::cout << "Loaded CSS from: " << p << std::endl;
                return buffer.str();
            }
        }

        std::cerr << "Failed to load CSS, using fallback" << std::endl;
        // Return minimal fallback CSS
        return R"(
            #root { width: 100%; height: 100%; display: block; background-color: #1a1a1a; position: relative; }
            #workspace_container { width: 100%; height: 100%; background-color: #1a1a1a; }
            .floating_panel { position: absolute; display: flex; flex-direction: row; background-color: #2b2b2b; gap: 4px; padding: 6px 8px; border-radius: 8px; z-index: 100; }
            #zoom_panel { bottom: 16px; right: 16px; }
            .zoom_btn { padding: 6px 10px; background-color: #3a3a3a; color: #d0d0d0; border-radius: 6px; font-size: 14px; }
            .zoom_btn:hover { background-color: #4a4a4a; }
            .zoom_text { padding: 6px 8px; color: #d0d0d0; font-size: 13px; min-width: 45px; text-align: center; }
        )";
    }
}


MetaEditor::MetaEditor(float width, float height)
    : width_(width), height_(height) {}

MetaEditor::~MetaEditor() = default;

void MetaEditor::init(flex::Renderer* ui_renderer) {
    // Create core systems
    canvas_ = std::make_unique<Canvas>(width_, height_);
    selection_ = std::make_unique<SelectionManager>(canvas_.get());
    command_manager_ = std::make_unique<CommandManager>();
    tool_manager_ = std::make_unique<ToolManager>(canvas_.get(), selection_.get(), command_manager_.get());

    // Create context toolbar
    context_toolbar_ = std::make_unique<ContextToolbar>(canvas_.get(), selection_.get());

    // Create exporter
    exporter_ = std::make_unique<Exporter>(canvas_.get());

    // Create zoom panel
    zoom_panel_ = std::make_unique<ZoomPanel>(canvas_.get());

    // Create navigator panel
    navigator_panel_ = std::make_unique<NavigatorPanel>(canvas_.get());

    // Create shape collection panel
    shape_panel_ = std::make_unique<ShapeCollectionPanel>(canvas_.get());
    shape_panel_->set_position(16, 300);  // Below tool panel
    shape_panel_->set_shape_added_callback([this](flex::Shape* shape) {
        // Select the newly added shape
        selection_->select(shape);
    });

    // Create tool panel
    tool_panel_ = std::make_unique<ToolPanel>(tool_manager_.get());

    // Create UI box
    if (ui_renderer) {
        ui_box_ = std::make_unique<flexUI::Box>(ui_renderer);
        ui_box_->set_viewport(width_, height_);
    }

    // Setup tools
    setup_tools();

    // Setup UI
    setup_ui();

    // Create default layer
    canvas_->create_layer("Layer 1");
}

void MetaEditor::shutdown() {
    // Cleanup
    tool_manager_.reset();
    command_manager_.reset();
    selection_.reset();
    canvas_.reset();
}

void MetaEditor::setup_tools() {
    // Register tools
    tool_manager_->register_tool(std::make_unique<SelectTool>());
    tool_manager_->register_tool(std::make_unique<PenTool>());
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Rectangle));
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Circle));
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Ellipse));
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Star));
    tool_manager_->register_tool(std::make_unique<ShapeTool>(ShapeTool::ShapeType::Polygon));
    
    // Set default tool
    tool_manager_->set_active_tool("Select");
}

void MetaEditor::setup_ui() {
    if (!ui_box_) return;

    // Load styles from external CSS file
    ui_box_->load_css(load_css_file("assets/editor.css"));

    auto* root = ui_box_->create("div", "root");
    ui_box_->set_root(root);

    // Workspace (Canvas) - takes full space
    auto* workspace = ui_box_->create_with_widget("div", new WorkspaceWidget(this), "workspace_container");
    root->append(workspace);

    // Setup zoom panel UI (flexUI-based)
    if (zoom_panel_) {
        zoom_panel_->setup_ui(ui_box_.get(), root);
    }

    // Position navigator panel at bottom-right
    if (navigator_panel_) {
        navigator_panel_->set_position(width_ - 160, height_ - 110);
    }
}

void MetaEditor::update(float dt) {
    canvas_->update(dt);
    tool_manager_->update(dt);

    // Update context toolbar
    if (context_toolbar_) {
        context_toolbar_->update();
    }

    // Update zoom panel display
    if (zoom_panel_) {
        zoom_panel_->update();
    }

    if (ui_box_) {
        ui_box_->update_time(dt * 1000.0f);
        ui_box_->update();
    }
}

void MetaEditor::render(flex::Renderer& renderer) {
    // If UI is active, rendering is handled by WorkspaceWidget inside ui_box_->update()
    if (!ui_box_) {
        canvas_->render(renderer);
        tool_manager_->render_overlay(renderer);
    }
}

void MetaEditor::render_context_toolbar(flex::Renderer& renderer) {
    if (context_toolbar_) {
        context_toolbar_->render(renderer);
    }
}

void MetaEditor::render_navigator(flex::Renderer& renderer) {
    if (navigator_panel_) {
        navigator_panel_->render(renderer);
    }
}

void MetaEditor::render_shape_panel(flex::Renderer& renderer) {
    if (shape_panel_) {
        shape_panel_->render(renderer);
    }
}

void MetaEditor::render_tool_panel(flex::Renderer& renderer) {
    if (tool_panel_) {
        tool_panel_->render(renderer);
    }
}

void MetaEditor::set_viewport(float width, float height) {
    width_ = width;
    height_ = height;

    if (ui_box_) {
        ui_box_->set_viewport(width, height);
    }

    // Reposition navigator panel
    if (navigator_panel_) {
        navigator_panel_->set_position(width - 160, height - 110);
    }
}

bool MetaEditor::handle_event(const SDL_Event& event) {
    if (ui_box_) {
        // Prepare flexUI event
        if (event.type == SDL_MOUSEMOTION || 
            event.type == SDL_MOUSEBUTTONDOWN || 
            event.type == SDL_MOUSEBUTTONUP || 
            event.type == SDL_MOUSEWHEEL) {
            
            flexUI::Event ui_event;
            bool handled = false;
            
            if (event.type == SDL_MOUSEMOTION) {
                ui_event = flexUI::Event::mouse_move((float)event.motion.x, (float)event.motion.y);
                handled = true;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                ui_event = flexUI::Event::mouse_down((float)event.button.x, (float)event.button.y, sdl_to_flex_button(event.button.button));
                handled = true;
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                ui_event = flexUI::Event::mouse_up((float)event.button.x, (float)event.button.y, sdl_to_flex_button(event.button.button));
                handled = true;
            } else if (event.type == SDL_MOUSEWHEEL) {
                int mx, my;
                SDL_GetMouseState(&mx, &my);
                ui_event = flexUI::Event::mouse_wheel((float)mx, (float)my, (float)event.wheel.x, (float)event.wheel.y);
                handled = true;
            }
            
            if (handled) {
                ui_box_->dispatch_event(ui_event);
                
                // If the event was handled by a UI element (like a button in the toolbar),
                // we should probably stop here.
                // However, how do we know if it was "consumed"?
                // Currently box_->dispatch_event doesn't return a value, but we can check ui_event.handled
                if (ui_event.handled) {
                    return true;
                }
            }
        }
    }

    switch (event.type) {
        case SDL_MOUSEBUTTONDOWN: {
            flex::Vec2 screen_pos(event.button.x, event.button.y);
            flex::Vec2 world_pos = canvas_->screen_to_world(screen_pos);

            if (event.button.button == SDL_BUTTON_LEFT) {
                // Check tool panel - click or drag
                if (tool_panel_) {
                    if (tool_panel_->handle_click(screen_pos.x(), screen_pos.y())) {
                        return true;
                    }
                    if (tool_panel_->handle_drag_start(screen_pos.x(), screen_pos.y())) {
                        return true;
                    }
                }

                // Check shape panel - click or drag
                if (shape_panel_) {
                    if (shape_panel_->handle_click(screen_pos.x(), screen_pos.y())) {
                        return true;
                    }
                    if (shape_panel_->handle_drag_start(screen_pos.x(), screen_pos.y())) {
                        return true;
                    }
                }

                // Check navigator panel
                if (navigator_panel_) {
                    if (navigator_panel_->handle_click(screen_pos.x(), screen_pos.y())) {
                        return true;
                    }
                    if (navigator_panel_->handle_drag_start(screen_pos.x(), screen_pos.y())) {
                        return true;
                    }
                }

                // Check context toolbar
                if (context_toolbar_) {
                    if (context_toolbar_->handle_click(screen_pos.x(), screen_pos.y())) {
                        return true;
                    }
                }

                return tool_manager_->on_pointer_down(screen_pos, world_pos);
            } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                // Pan mode - handled in demo
                return false;
            }
            break;
        }

        case SDL_MOUSEMOTION: {
            flex::Vec2 screen_pos(event.motion.x, event.motion.y);
            flex::Vec2 world_pos = canvas_->screen_to_world(screen_pos);

            // Handle panel dragging
            if (tool_panel_ && tool_panel_->handle_drag_move(screen_pos.x(), screen_pos.y())) {
                return true;
            }
            if (shape_panel_ && shape_panel_->handle_drag_move(screen_pos.x(), screen_pos.y())) {
                return true;
            }
            if (navigator_panel_) {
                if (navigator_panel_->handle_drag_move(screen_pos.x(), screen_pos.y())) {
                    return true;
                }
                // Navigator also has its own viewport drag
                if (navigator_panel_->handle_drag(screen_pos.x(), screen_pos.y())) {
                    return true;
                }
            }

            return tool_manager_->on_pointer_move(screen_pos, world_pos);
        }

        case SDL_MOUSEBUTTONUP: {
            flex::Vec2 screen_pos(event.button.x, event.button.y);
            flex::Vec2 world_pos = canvas_->screen_to_world(screen_pos);

            // End panel drags
            if (tool_panel_) tool_panel_->handle_drag_end();
            if (shape_panel_) shape_panel_->handle_drag_end();
            if (navigator_panel_) {
                navigator_panel_->handle_drag_end();
                navigator_panel_->end_drag();  // Navigator viewport drag
            }

            if (event.button.button == SDL_BUTTON_LEFT) {
                return tool_manager_->on_pointer_up(screen_pos, world_pos);
            }
            break;
        }
        
        case SDL_MOUSEWHEEL: {
            // Zoom - handled in demo
            return false;
        }
        
        case SDL_KEYDOWN: {
            int key = event.key.keysym.sym;
            int mods = event.key.keysym.mod;

            // Handle shortcuts
            if (mods & KMOD_CTRL) {
                if (key == SDLK_z) {
                    command_manager_->undo();
                    return true;
                } else if (key == SDLK_y) {
                    command_manager_->redo();
                    return true;
                } else if (key == SDLK_a) {
                    selection_->select_all();
                    return true;
                } else if (key == SDLK_s) {
                    // Ctrl+S = Save SVG
                    if (exporter_) {
                        if (exporter_->save_svg("output.svg")) {
                            std::cout << "Saved to output.svg" << std::endl;
                        }
                    }
                    return true;
                } else if (key == SDLK_c && (mods & KMOD_SHIFT)) {
                    // Ctrl+Shift+C = Copy selection as SVG
                    if (exporter_ && selection_->has_selection()) {
                        std::string svg = exporter_->selection_to_svg(selection_->selection());
                        std::cout << "SVG copied:\n" << svg << std::endl;
                        // TODO: Actually copy to clipboard
                    }
                    return true;
                }
            }

            // Delete selection
            if (key == SDLK_DELETE || key == SDLK_BACKSPACE) {
                if (selection_->has_selection()) {
                    for (auto* node : selection_->selection()) {
                        if (node->parent()) {
                            static_cast<flex::Group*>(node->parent())->remove_child(node);
                        }
                    }
                    selection_->clear_selection();
                    return true;
                }
            }

            // Escape - clear selection
            if (key == SDLK_ESCAPE) {
                selection_->clear_selection();
                return true;
            }

            // Tool shortcuts
            if (key == SDLK_v) {
                tool_manager_->set_active_tool("Select");
                return true;
            } else if (key == SDLK_p) {
                tool_manager_->set_active_tool("Pen");
                return true;
            } else if (key == SDLK_r) {
                tool_manager_->set_active_tool("Rectangle");
                return true;
            } else if (key == SDLK_o) {
                tool_manager_->set_active_tool("Circle");
                return true;
            } else if (key == SDLK_e) {
                tool_manager_->set_active_tool("Ellipse");
                return true;
            } else if (key == SDLK_s) {
                tool_manager_->set_active_tool("Star");
                return true;
            } else if (key == SDLK_g && !(mods & KMOD_CTRL)) {
                // Toggle grid snap (G without Ctrl)
                canvas_->set_snap_to_grid(!canvas_->is_snap_to_grid());
                return true;
            } else if (key == SDLK_g && (mods & KMOD_CTRL)) {
                // Ctrl+G for polygon tool
                tool_manager_->set_active_tool("Polygon");
                return true;
            }

            return tool_manager_->on_key_down(key, mods);
        }
        
        case SDL_KEYUP: {
            int key = event.key.keysym.sym;
            int mods = event.key.keysym.mod;
            return tool_manager_->on_key_up(key, mods);
        }
    }
    
    return false;
}

} // namespace meta_editor
