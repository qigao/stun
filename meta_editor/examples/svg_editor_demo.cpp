/*
 * SVG Editor Demo - Inkscape-like SVG editor (GLFW + OpenGL)
 */

#include <meta_editor/core/editor.h>
#include <meta_editor/core/glfw_adapter.h>
#include <meta_editor/view/tool_panel.h>
#include <meta_editor/view/properties_panel.h>
#include <meta_editor/view/layers_panel.h>
#include <flex/backends/thorvg/init.h>
#include <flex/bridge/renderer.h>
#include <flex/app/glfw_app.h>
#include <thorvg.h>
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
        layers_panel_->set_position((float)width() - 220, 300);

        std::cout << "SVG Editor - Ctrl+O Open, Ctrl+S Save, Ctrl+N New\n";
        return true;
    }

    void on_resize(int w, int h) override {
        GlfwApp::on_resize(w, h);
        editor_.set_viewport((float)w, (float)h);
        props_panel_->set_position((float)w - 220, 16);
        layers_panel_->set_position((float)w - 220, 300);
    }

    void on_render() override {
        editor_.update(1.0f / 60.0f);
        renderer()->begin_frame((float)width(), (float)height(), 1.0f);
        renderer()->clear(flex::Color{0.15f, 0.15f, 0.17f, 1.0f});
        editor_.render(*renderer());
        editor_.render_tool_overlay(*renderer());
        tool_panel_->render(*renderer());
        props_panel_->render(*renderer());
        layers_panel_->render(*renderer());
        renderer()->end_frame();
    }

    void on_mouse_button(int button, int action, int mods) override {
        if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            panning_ = (action == GLFW_PRESS);
            return;
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            float mx = (float)last_mx_, my = (float)last_my_;
            if (tool_panel_->contains(mx, my)) { tool_panel_->handle_click(mx, my); return; }
            if (props_panel_->contains(mx, my)) { props_panel_->handle_click(mx, my); return; }
            if (layers_panel_->contains(mx, my)) { layers_panel_->handle_click(mx, my); return; }
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
            auto ev = glfw_cursor_event(x, y, mods);
            editor_.handle_event(ev);
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
            auto file = open_file_dialog();
            if (!file.empty() && editor_.import_svg(file)) {
                current_file_ = file;
                glfwSetWindowTitle(window(), ("SVG Editor - " + current_file_).c_str());
            }
            return;
        }
        if (ctrl && key == GLFW_KEY_S) {
            auto file = save_file_dialog(current_file_);
            if (!file.empty()) {
                editor_.save_svg(file);
                current_file_ = file;
                glfwSetWindowTitle(window(), ("SVG Editor - " + current_file_).c_str());
            }
            return;
        }
        if (ctrl && key == GLFW_KEY_N) {
            editor_.shutdown();
            editor_.init();
            current_file_.clear();
            glfwSetWindowTitle(window(), "SVG Editor");
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
