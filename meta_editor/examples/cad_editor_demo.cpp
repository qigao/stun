/*
 * CAD Editor Demo - Modern CAD UI layout
 */

#include <meta_editor/core/editor.h>
#include <meta_editor/core/glfw_adapter.h>
#include <meta_editor/view/cad_toolbox.h>
#include <meta_editor/view/cad_top_bar.h>
#include <meta_editor/view/cad_bottom_bar.h>
#include <meta_editor/view/icon_system.h>
#include <flex/bridge/renderer.h>
#include "../examples/glfw_app.h"
#include <iostream>

using namespace meta_editor;

class CadEditorApp : public flex::GlfwApp {
public:
    Editor editor_;
    std::unique_ptr<CADToolbox> toolbox_;
    std::unique_ptr<CADTopBar> top_bar_;
    std::unique_ptr<CADBottomBar> bottom_bar_;
    std::unique_ptr<IconSystem> icons_;

    CadEditorApp() : GlfwApp("CAD Editor Demo", 1280, 850), editor_(1280, 850) {
        icons_ = std::make_unique<IconSystem>();
    }

    bool on_init() override {
        load_font("Arial", "C:/Windows/Fonts/arial.ttf");
        editor_.init();

        toolbox_ = std::make_unique<CADToolbox>();
        top_bar_ = std::make_unique<CADTopBar>();
        bottom_bar_ = std::make_unique<CADBottomBar>();
        
        // Try to load icons from absolute path first
        if (!icons_->load_json("c:/projects/cpp/nanogui/assets/icons/gis.json")) {
            fprintf(stderr, "Failed to load gis.json from absolute path\n");
            // Try relative as fallback
            if (!icons_->load_json("assets/icons/gis.json")) {
                fprintf(stderr, "Failed to load gis.json from relative path\n");
            }
        }
        
        if (!icons_->load_json("c:/projects/cpp/nanogui/assets/icons/material-symbols.json")) {
            fprintf(stderr, "Failed to load material-symbols.json from absolute path\n");
             if (!icons_->load_json("assets/icons/material-symbols.json")) {
                fprintf(stderr, "Failed to load material-symbols.json from relative path\n");
            }
        }

        toolbox_->set_icons(icons_.get());
        top_bar_->set_icons(icons_.get());
        bottom_bar_->set_icons(icons_.get());

        on_resize(width(), height());
        return true;
    }

    void on_resize(int w, int h) override {
        GlfwApp::on_resize(w, h);
        
        float lw = (float)w / content_scale_x_;
        float lh = (float)h / content_scale_y_;
        
        editor_.set_viewport(lw, lh);
        
        if (top_bar_) top_bar_->set_layout(0, 0, lw);
        if (bottom_bar_) bottom_bar_->set_layout(0, lh - 36, lw);
        
        if (toolbox_) {
            toolbox_->set_position(10, 80);
        }
    }

    void on_render() override {
        editor_.update(1.0f / 60.0f);
        
        renderer()->begin_frame((float)width() / content_scale_x_,
                                (float)height() / content_scale_y_,
                                content_scale_x_);
        renderer()->clear(flex::Color{0.25f, 0.25f, 0.25f, 1.0f}); // CAD gray-ish background
        
        // Render canvas
        editor_.canvas()->render_content(*renderer());
        
        // Draw grid lines (CAD style)
        draw_cad_grid();
        
        editor_.canvas()->render_overlay(*renderer());
        editor_.render_tool_overlay(*renderer());
        
        // UI
        if (toolbox_) toolbox_->render(*renderer());
        if (top_bar_) top_bar_->render(*renderer());
        if (bottom_bar_) bottom_bar_->render(*renderer());
        
        renderer()->end_frame();
    }

    void draw_cad_grid() {
        // Simple major/minor axis for visual context
        float lw = (float)width() / content_scale_x_;
        float lh = (float)height() / content_scale_y_;
        
        flex::Paint axis_p = flex::Paint::solid({0.4f, 0.2f, 0.2f, 0.5f});
        renderer()->draw_line(0, lh/2, lw, lh/2, axis_p, 1.0f);
        renderer()->draw_line(lw/2, 0, lw/2, lh, axis_p, 1.0f);
    }

    void on_mouse_button(int button, int action, int mods) override {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            float mx = mouse_x(), my = mouse_y();
            if (top_bar_ && top_bar_->handle_pointer_down(mx, my)) return;
            if (bottom_bar_ && bottom_bar_->handle_pointer_down(mx, my)) return;
            if (toolbox_ && toolbox_->handle_pointer_down(mx, my)) return;
        }

        if (action == GLFW_RELEASE) {
            if (top_bar_) top_bar_->handle_drag_end();
            if (bottom_bar_) bottom_bar_->handle_drag_end();
            if (toolbox_) toolbox_->handle_drag_end();
        }

        auto ev = glfw_mouse_button_event(button, action, mods, mouse_x(), mouse_y());
        editor_.handle_event(ev);
    }

    void on_cursor_pos(double x, double y) override {
        float mx = (float)x, my = (float)y;
        bool dragging = false;
        if (top_bar_ && top_bar_->handle_drag_move(mx, my)) dragging = true;
        if (bottom_bar_ && bottom_bar_->handle_drag_move(mx, my)) dragging = true;
        if (toolbox_ && toolbox_->handle_drag_move(mx, my)) dragging = true;

        if (!dragging) {
            int mods = 0; // Simple mods for move
            auto ev = glfw_cursor_event(x, y, mods);
            editor_.handle_event(ev);
        }
    }

    void on_scroll(double dx, double dy) override {
        editor_.canvas()->zoom_at(mouse_x(), mouse_y(), 1.0f + (float)dy * 0.1f);
    }

    ~CadEditorApp() {
        editor_.shutdown();
    }
};

int main(int argc, char* argv[]) {
    CadEditorApp app;
    if (!app.init()) return 1;
    app.run();
    return 0;
}
