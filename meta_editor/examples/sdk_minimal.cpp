/*
 * Meta Editor SDK - Minimal Example (GLFW + OpenGL)
 *
 * Demonstrates using Editor directly without MetaEditor's UI panels.
 */

#include <meta_editor/core/editor.h>
#include <meta_editor/core/glfw_adapter.h>
#include <flex/bridge/renderer.h>
#include  "glfw_app.h"
#include <iostream>

using namespace meta_editor;

class EditorDemo : public flex::GlfwApp {
public:
    Editor editor_;
    bool panning_ = false;
    double last_mx_ = 0, last_my_ = 0;

    EditorDemo() : GlfwApp("Meta Editor SDK Demo", 1024, 768), editor_(1024, 768) {}

    bool on_init() override {
        load_font("Arial", "C:/Windows/Fonts/arial.ttf");
        editor_.init();
        create_demo_shapes();
        print_help();
        return true;
    }

    void create_demo_shapes() {
        auto* canvas = editor_.canvas();
        auto* allocator = canvas->instance()->object_allocator();
        auto layers = canvas->get_all_layers();
        if (layers.empty()) return;

        auto* layer = layers[0];

        auto* rect = flex::Shape::create(*allocator);
        rect->set_rect(100, 80, 8);
        rect->set_position(100, 100);
        rect->set_fill(flex::Color{0.2f, 0.6f, 0.9f, 1.0f});
        rect->set_stroke(flex::Color{0.1f, 0.3f, 0.5f, 1.0f}, 2.0f);
        layer->add_child(rect);

        auto* circle = flex::Shape::create(*allocator);
        circle->set_circle(50);
        circle->set_position(350, 150);
        circle->set_fill(flex::Color{0.9f, 0.3f, 0.3f, 1.0f});
        layer->add_child(circle);

        auto* star = flex::Shape::create(*allocator);
        star->set_star(5, 60, 30);
        star->set_position(550, 150);
        star->set_fill(flex::Color{0.9f, 0.8f, 0.2f, 1.0f});
        star->set_stroke(flex::Color{0.6f, 0.5f, 0.1f, 1.0f}, 2.0f);
        layer->add_child(star);
    }

    void print_help() {
        std::cout << "Meta Editor SDK Demo\n"
                  << "V - Select, R - Rectangle, O - Circle, P - Pen\n"
                  << "Delete - Delete, Ctrl+Z/Y - Undo/Redo\n"
                  << "Middle mouse - Pan, Scroll - Zoom\n";
    }

    void on_resize(int w, int h) override {
        GlfwApp::on_resize(w, h);
        editor_.set_viewport((float)w, (float)h);
    }

    void on_render() override {
        editor_.update(1.0f / 60.0f);
        renderer()->begin_frame((float)width() / content_scale_x_,
                                (float)height() / content_scale_y_,
                                content_scale_x_);
        renderer()->clear(flex::Color{0.12f, 0.12f, 0.14f, 1.0f});
        editor_.render(*renderer());
        editor_.render_tool_overlay(*renderer());
        renderer()->end_frame();
    }

    void on_mouse_button(int button, int action, int mods) override {
        if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            panning_ = (action == GLFW_PRESS);
            return;
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
        if (action == GLFW_RELEASE) {
            auto ev = glfw_key_event(key, action, mods);
            editor_.handle_event(ev);
            return;
        }
        auto ev = glfw_key_event(key, action, mods);
        editor_.handle_event(ev);
    }

    ~EditorDemo() {
        editor_.shutdown();
    }
};

int main() {
    EditorDemo app;
    if (!app.init()) return 1;
    app.run();
    return 0;
}
