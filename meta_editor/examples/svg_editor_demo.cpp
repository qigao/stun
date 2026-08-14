/*
 * SVG Editor Demo - Inkscape-like SVG editor (GLFW + OpenGL)
 */

#include <meta_editor/core/editor.h>
#include <meta_editor/core/glfw_adapter.h>
#include <meta_editor/view/tool_panel.h>
#include <meta_editor/view/properties_panel.h>
#include <meta_editor/view/layers_panel.h>
#include <meta_editor/view/top_bar.h>
#include <meta_editor/view/bottom_bar.h>
#include <flex/bridge/renderer.h>
#include "glfw_app.h"
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

using namespace meta_editor;

static std::string open_file_dialog() {
#ifdef _WIN32
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "SVG Files\0*.svg\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) return filename;
#endif
    return "";
}

static std::string save_file_dialog(const std::string& current) {
#ifdef _WIN32
    char filename[MAX_PATH] = "";
    if (!current.empty()) strncpy(filename, current.c_str(), MAX_PATH - 1);
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "SVG Files\0*.svg\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = "svg";
    if (GetSaveFileNameA(&ofn)) return filename;
#endif
    return "";
}

class SvgEditorApp : public flex::GlfwApp {
public:
    Editor editor_;
    std::unique_ptr<ToolPanel> tool_panel_;
    std::unique_ptr<PropertiesPanel> props_panel_;
    std::unique_ptr<LayersPanel> layers_panel_;
    std::unique_ptr<TopBar> top_bar_;
    std::unique_ptr<BottomBar> bottom_bar_;
    std::string current_file_;
    bool panning_ = false;
    double last_mx_ = 0, last_my_ = 0;

    SvgEditorApp() : GlfwApp("SVG Editor", 1280, 800), editor_(1280, 800) {}

    bool on_init() override {
        load_font("Arial", "C:/Windows/Fonts/arial.ttf");
        editor_.init();

        tool_panel_ = std::make_unique<ToolPanel>(editor_.tools());
        tool_panel_->set_position(16, 16);

        props_panel_ = std::make_unique<PropertiesPanel>(editor_.canvas(), editor_.selection());
        props_panel_->set_position((float)width() - 220, 16);

        layers_panel_ = std::make_unique<LayersPanel>(editor_.canvas(), editor_.selection());
        // Init Top/Bottom bars
        top_bar_ = std::make_unique<TopBar>(&editor_);
        bottom_bar_ = std::make_unique<BottomBar>(&editor_);
        
        // Initial layout
        int w = width();
        int h = height();
        if (top_bar_) top_bar_->set_layout(0, 0, (float)w);
        if (bottom_bar_) bottom_bar_->set_layout(0, (float)h - 30, (float)w);
        
        float top_h = top_bar_ ? top_bar_->height() : 0;
        
        tool_panel_->set_position(16, top_h + 16);
        props_panel_->set_position((float)w - 220, top_h + 16);
        layers_panel_->set_position((float)w - 220, top_h + 300);

        // Hook up callbacks
        top_bar_->on_new = [this]() { on_new_file(); };
        top_bar_->on_open = [this]() { on_open_file(); };
        top_bar_->on_save = [this](const std::string&) { on_save_file(); };

        std::cout << "SVG Editor - Ctrl+O Open, Ctrl+S Save, Ctrl+N New\n";
        return true;
    }

    void on_new_file() {
        editor_.shutdown();
        editor_.init();
        current_file_.clear();
        glfwSetWindowTitle(window(), "SVG Editor");
    }

    void on_open_file() {
        auto file = open_file_dialog();
        if (!file.empty() && editor_.import_svg(file)) {
            current_file_ = file;
            glfwSetWindowTitle(window(), ("SVG Editor - " + current_file_).c_str());
        }
    }

    void on_save_file() {
        auto file = save_file_dialog(current_file_);
        if (!file.empty()) {
            editor_.save_svg(file);
            current_file_ = file;
            glfwSetWindowTitle(window(), ("SVG Editor - " + current_file_).c_str());
        }
    }

    void on_resize(int w, int h) override {
        GlfwApp::on_resize(w, h);
        
        // Use logical units for layout and viewport.
        // On a 1.25x scale screen, physical 1280px = logical 1024px.
        float lw = (float)w / content_scale_x_;
        float lh = (float)h / content_scale_y_;
        
        editor_.set_viewport(lw, lh);
        
        float bottom_h = 30.0f;
        
        if (top_bar_) top_bar_->set_layout(0, 0, lw);
        if (bottom_bar_) bottom_bar_->set_layout(0, lh - bottom_h, lw);
        
        float top_h = top_bar_ ? top_bar_->height() : 0;
        
        tool_panel_->set_position(16, top_h + 16);
        props_panel_->set_position(lw - 220, top_h + 16);
        layers_panel_->set_position(lw - 220, top_h + 300);
    }

    void on_render() override {
        editor_.update(1.0f / 60.0f);
        
        // Passing content_scale_x_ here allows drawing in logical units (e.g. 1024 width)
        // to fill the physical framebuffer (e.g. 1280 width).
        renderer()->begin_frame((float)width() / content_scale_x_,
                                (float)height() / content_scale_y_,
                                content_scale_x_);
        
        renderer()->clear(flex::Color{0.15f, 0.15f, 0.17f, 1.0f});
        
        // Render content and overlay
        editor_.canvas()->render_content(*renderer());
        editor_.canvas()->render_overlay(*renderer());
        editor_.render_tool_overlay(*renderer());
        
        tool_panel_->render(*renderer());
        props_panel_->render(*renderer());
        layers_panel_->render(*renderer());
        
        if (top_bar_) top_bar_->render(*renderer());
        if (bottom_bar_) bottom_bar_->render(*renderer());
        
        renderer()->end_frame();
    }

    void on_mouse_button(int button, int action, int mods) override {
        if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            panning_ = (action == GLFW_PRESS);
            return;
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            float mx = (float)last_mx_, my = (float)last_my_;
            
            if (tool_panel_->handle_pointer_down(mx, my)) return;
            if (props_panel_->handle_pointer_down(mx, my)) return;
            if (layers_panel_->handle_pointer_down(mx, my)) return;
        }

        if (action == GLFW_RELEASE) {
            if (top_bar_) top_bar_->handle_drag_end();
            if (bottom_bar_) bottom_bar_->handle_drag_end();
            if (tool_panel_) tool_panel_->handle_drag_end();
            if (props_panel_) props_panel_->handle_drag_end();
            if (layers_panel_) layers_panel_->handle_drag_end();
        }

        auto ev = glfw_mouse_button_event(button, action, mods, last_mx_, last_my_);
        editor_.handle_event(ev);
    }

    void on_cursor_pos(double x, double y) override {
        if (panning_) {
            editor_.canvas()->pan((float)(x - last_mx_), (float)(y - last_my_));
        } else {
            int mods = 0;
            if (glfwGetKey(window(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) mods |= GLFW_MOD_SHIFT;
            if (glfwGetKey(window(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) mods |= GLFW_MOD_CONTROL;

            float mx = (float)x, my = (float)y;
            bool dragging = false;
            if (top_bar_ && top_bar_->handle_drag_move(mx, my)) dragging = true;
            if (bottom_bar_ && bottom_bar_->handle_drag_move(mx, my)) dragging = true;
            if (tool_panel_ && tool_panel_->handle_drag_move(mx, my)) dragging = true;
            if (props_panel_ && props_panel_->handle_drag_move(mx, my)) dragging = true;
            if (layers_panel_ && layers_panel_->handle_drag_move(mx, my)) dragging = true;

            if (!dragging) {
                auto ev = glfw_cursor_event(x, y, mods);
                editor_.handle_event(ev);
            }
        }
        last_mx_ = x;
        last_my_ = y;
    }

    void on_scroll(double dx, double dy) override {
        editor_.canvas()->zoom_at((float)last_mx_, (float)last_my_, 1.0f + (float)dy * 0.1f);
    }

    void on_key(int key, int action, int mods) override {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) {
            auto ev = glfw_key_event(key, action, mods);
            editor_.handle_event(ev);
            return;
        }

        bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;

        if (ctrl && key == GLFW_KEY_O) {
            on_open_file();
            return;
        }
        if (ctrl && key == GLFW_KEY_S) {
            on_save_file();
            return;
        }
        if (ctrl && key == GLFW_KEY_N) {
            on_new_file();
            return;
        }

        auto ev = glfw_key_event(key, action, mods);
        editor_.handle_event(ev);
    }

    ~SvgEditorApp() {
        editor_.shutdown();
    }
};

int main(int argc, char* argv[]) {
    SvgEditorApp app;
    if (!app.init()) return 1;
    app.run();
    return 0;
}
