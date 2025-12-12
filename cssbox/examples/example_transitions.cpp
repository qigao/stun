/*
 * NanoVG CSS Example - Transitions (GLFW version)
 */

#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>

#include <cssbox.h>
#include <fmtlog.h>
#include <chrono>
#include <set>

class TransitionsDemo {
public:
    TransitionsDemo() {
        init_glfw();
        init_nanovg();
        setup_scene();
        last_time_ = std::chrono::high_resolution_clock::now();
    }

    ~TransitionsDemo() {
        if (renderer) cssboxDeleteRenderer(renderer);
        if (vg) nvgDeleteGL3(vg);
        if (window) glfwDestroyWindow(window);
        glfwTerminate();
    }

    void run() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            auto current_time = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(current_time - last_time_).count();
            last_time_ = current_time;
            cssboxUpdate(renderer, dt);
            if (render()) {
                glfwSwapBuffers(window);
            } else {
                glfwWaitEventsTimeout(0.001);
            }
        }
    }

    void handle_mouse_move(float mx, float my) {
        update_hover(color_box, mx, my, 50, 80, 120, 120);
        update_hover(scale_box, mx, my, 200, 80, 120, 120);
        update_hover(fade_box, mx, my, 350, 80, 120, 120);
        update_hover(rotate_box, mx, my, 500, 80, 120, 120);
        update_hover(multi_box, mx, my, 50, 240, 120, 120);
        update_hover(position_box, mx, my, 200, 240, 120, 120);
        update_hover(fast_box, mx, my, 350, 240, 120, 120);
        update_hover(slow_box, mx, my, 500, 240, 120, 120);
    }

private:
    GLFWwindow* window = nullptr;
    NVGcontext* vg = nullptr;
    cssboxRenderer* renderer = nullptr;
    std::chrono::high_resolution_clock::time_point last_time_;

    cssboxElement* color_box = nullptr;
    cssboxElement* scale_box = nullptr;
    cssboxElement* fade_box = nullptr;
    cssboxElement* rotate_box = nullptr;
    cssboxElement* multi_box = nullptr;
    cssboxElement* position_box = nullptr;
    cssboxElement* fast_box = nullptr;
    cssboxElement* slow_box = nullptr;

    std::set<cssboxElement*> hovered_elements;

    static void cursor_pos_callback(GLFWwindow* win, double xpos, double ypos) {
        auto* app = static_cast<TransitionsDemo*>(glfwGetWindowUserPointer(win));
        if (app) app->handle_mouse_move((float)xpos, (float)ypos);
    }

    static void window_refresh_callback(GLFWwindow* win) {
        auto* app = static_cast<TransitionsDemo*>(glfwGetWindowUserPointer(win));
        if (app && app->renderer) cssboxInvalidatePaint(app->renderer);
    }

    void init_glfw() {
        if (!glfwInit()) return;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_STENCIL_BITS, 8);
        glfwWindowHint(GLFW_SAMPLES, 4);
        window = glfwCreateWindow(1000, 700, "NanoVG CSS Transitions", nullptr, nullptr);
        if (!window) { glfwTerminate(); return; }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        glfwSetWindowUserPointer(window, this);
        glfwSetCursorPosCallback(window, cursor_pos_callback);
        glfwSetWindowRefreshCallback(window, window_refresh_callback);
    }

    void init_nanovg() {
        gladLoadGL();
        vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
        renderer = cssboxCreateRenderer(vg);
    }

    void setup_scene();

    cssboxElement* create_box(const char* id) {
        auto* box = cssboxCreateElement(renderer, id, "div");
        cssboxAddClass(box, "box");
        return box;
    }

    void create_label(const char* id, const char* text) {
        auto* label = cssboxCreateElement(renderer, id, "div");
        cssboxAddClass(label, "label");
        cssboxSetText(label, text);
    }

    void update_hover(cssboxElement* elem, float mx, float my, float x, float y, float w, float h) {
        if (!elem) return;
        bool is_inside = (mx >= x && mx <= x + w && my >= y && my <= y + h);
        if (is_inside) {
            cssboxSetPseudoStateEx(renderer, elem, "hover", 1);
            hovered_elements.insert(elem);
        } else {
            cssboxSetPseudoStateEx(renderer, elem, "hover", 0);
            hovered_elements.erase(elem);
        }
    }

    bool render() {
        int win_w, win_h, fb_w, fb_h;
        glfwGetWindowSize(window, &win_w, &win_h);
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        float pixel_ratio = (float)fb_w / (float)win_w;
        cssboxSetViewport(renderer, (float)win_w, (float)win_h);
        if (!cssboxNeedsPaint(renderer)) return false;
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.98f, 0.98f, 0.98f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        nvgBeginFrame(vg, win_w, win_h, pixel_ratio);
        cssboxRender(renderer);
        nvgEndFrame(vg);
        return true;
    }
};

void TransitionsDemo::setup_scene() {
    const char* css = R"(
        .box { width: 120px; height: 120px; border-radius: 12px; position: absolute; }
        #color-box { top: 80px; left: 50px; background: #2196F3; transition: background-color 0.3s ease-in-out; }
        #color-box:hover { background: #F44336; }
        #scale-box { top: 80px; left: 200px; background: #4CAF50; transition: transform 0.3s ease-in-out; }
        #scale-box:hover { transform: scale(1.3); }
        #fade-box { top: 80px; left: 350px; background: #9C27B0; opacity: 0.4; transition: opacity 0.4s linear; }
        #fade-box:hover { opacity: 1.0; }
        #rotate-box { top: 80px; left: 500px; background: #FF9800; transition: transform 0.4s ease; }
        #rotate-box:hover { transform: rotate(45deg); }
        #multi-box { top: 240px; left: 50px; background: #00BCD4; opacity: 0.7; transition: background-color 0.3s ease, opacity 0.3s ease, transform 0.3s ease; }
        #multi-box:hover { background: #E91E63; opacity: 1.0; transform: scale(1.2) rotate(10deg); }
        #position-box { top: 240px; left: 200px; background: #673AB7; transition: left 0.5s ease-in-out, top 0.5s ease-in-out; }
        #position-box:hover { left: 250px; top: 290px; }
        #fast-box { top: 240px; left: 350px; background: #FFC107; transition: background-color 0.1s linear, transform 0.1s linear; }
        #fast-box:hover { background: #795548; transform: scale(1.15); }
        #slow-box { top: 240px; left: 500px; background: #607D8B; transition: background-color 1.0s ease-in-out, transform 1.0s ease-in-out; }
        #slow-box:hover { background: #FF5722; transform: rotate(180deg) scale(1.1); }
        .label { position: absolute; color: #333; font-size: 12px; font-weight: bold; }
        #label-section1 { top: 40px; left: 50px; font-size: 16px; }
        #label-section2 { top: 200px; left: 50px; font-size: 16px; }
        #label-color { top: 210px; left: 55px; }
        #label-scale { top: 210px; left: 205px; }
        #label-fade { top: 210px; left: 360px; }
        #label-rotate { top: 210px; left: 505px; }
        #label-multi { top: 370px; left: 55px; }
        #label-position { top: 370px; left: 190px; }
        #label-fast { top: 370px; left: 345px; }
        #label-slow { top: 370px; left: 500px; }
        #instructions { position: absolute; top: 420px; left: 50px; width: 600px; color: #666; font-size: 14px; }
    )";
    cssboxParseCSS(renderer, css);
    color_box = create_box("color-box");
    scale_box = create_box("scale-box");
    fade_box = create_box("fade-box");
    rotate_box = create_box("rotate-box");
    multi_box = create_box("multi-box");
    position_box = create_box("position-box");
    fast_box = create_box("fast-box");
    slow_box = create_box("slow-box");
    create_label("label-section1", "Simple Property Transitions");
    create_label("label-section2", "Complex Transitions");
    create_label("label-color", "Color 0.3s");
    create_label("label-scale", "Scale 0.3s");
    create_label("label-fade", "Opacity 0.4s");
    create_label("label-rotate", "Rotate 0.4s");
    create_label("label-multi", "Multiple Props");
    create_label("label-position", "Position 0.5s");
    create_label("label-fast", "Fast 0.1s");
    create_label("label-slow", "Slow 1.0s");
    create_label("instructions", "Hover over boxes to see transitions.");
}

int main(int argc, char* argv[]) {
    fmtlog::setLogLevel(fmtlog::INF);
    TransitionsDemo demo;
    demo.run();
    return 0;
}
