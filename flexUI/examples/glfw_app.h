#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <thorvg.h>
#include <flex/bridge/renderer_thorvg.h>
#include <flexUI/host_bridge.h>
#include "host_media_bridge.h"
#include "win32_ime_helper.h"
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include <flex/bridge/renderer.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>

namespace flexUI {
class Box;
}

namespace flex {

class GlfwApp {
public:
    GlfwApp(const char* title, int width, int height)
        : title_(title), width_(width), height_(height) {}

    virtual ~GlfwApp() {
        renderer_.reset();
        canvas_.reset();
        tvg::Initializer::term();
#ifdef _WIN32
        if (window_ && original_wnd_proc) {
            flexui_examples::win32_ime::restore_window_proc(glfwGetWin32Window(window_), original_wnd_proc);
        }
#endif
        destroy_cursor(arrow_cursor_);
        destroy_cursor(ibeam_cursor_);
        destroy_cursor(hand_cursor_);
        destroy_cursor(crosshair_cursor_);
        destroy_cursor(hresize_cursor_);
        destroy_cursor(vresize_cursor_);
        if (window_) glfwDestroyWindow(window_);
        glfwTerminate();
    }

    bool init() {
        if (!glfwInit()) {
            std::cerr << "GLFW init failed" << std::endl;
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window_ = glfwCreateWindow(width_, height_, title_, nullptr, nullptr);
        if (!window_) {
            std::cerr << "Window creation failed" << std::endl;
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(window_);
        glfwSetWindowUserPointer(window_, this);

        // Callbacks
        glfwSetKeyCallback(window_, key_callback);
        glfwSetMouseButtonCallback(window_, mouse_button_callback);
        glfwSetCursorPosCallback(window_, cursor_pos_callback);
        glfwSetScrollCallback(window_, scroll_callback);
        glfwSetCharCallback(window_, char_callback);
        glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);

        glfwGetFramebufferSize(window_, &width_, &height_);
        glfwGetWindowContentScale(window_, &content_scale_x_, &content_scale_y_);

#ifdef _WIN32
        HWND hwnd = glfwGetWin32Window(window_);
        original_wnd_proc = flexui_examples::win32_ime::subclass_window(hwnd, this, hook_wnd_proc);
        flexui_examples::win32_ime::enable_ime(hwnd);
#endif

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "GLAD init failed" << std::endl;
            return false;
        }

        glfwSwapInterval(1);

        if (tvg::Initializer::init(4) != tvg::Result::Success) {
            std::cerr << "ThorVG init failed" << std::endl;
            return false;
        }

        canvas_.reset(tvg::GlCanvas::gen());
        if (!canvas_) {
            std::cerr << "GlCanvas creation failed" << std::endl;
            return false;
        }

        canvas_->target(glfwGetCurrentContext(), 0, width_, height_, tvg::ColorSpace::ABGR8888S);
        renderer_ = create_thorvg_renderer(canvas_.get());

        if (!renderer_) {
            std::cerr << "Renderer creation failed" << std::endl;
            return false;
        }

        return on_init();
    }

    void run() {
        double last = glfwGetTime();

        while (!glfwWindowShouldClose(window_)) {
            double now = glfwGetTime();
            float dt = static_cast<float>(now - last);
            last = now;

            glfwPollEvents();
            flexui_examples::media::sync_environment(host_box());
            on_update(dt);
            sync_host_cursor();
#ifdef _WIN32
            flexui_examples::media::sync_window_color_scheme(
                glfwGetWin32Window(window_), host_box(), current_color_scheme_);
#endif

            if (should_render_frame()) {
                glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                if (remove_canvas_before_render()) {
                    canvas_->remove();
                }
                on_render();
                glfwSwapBuffers(window_);
            } else {
                glfwWaitEventsTimeout(0.005);
            }
        }
    }

    void quit() { glfwSetWindowShouldClose(window_, GLFW_TRUE); }

    void update_ime_position(int x, int y) {
#ifdef _WIN32
        flexui_examples::win32_ime::update_ime_position(
            glfwGetWin32Window(window_), content_scale_y_, x, y);
#endif
    }

    // Cursor position in the same coordinate space used by flexUI layout.
    float mouse_x() const { return (float)mouse_x_; }
    float mouse_y() const { return (float)mouse_y_; }

    // Accessors
    Renderer* renderer() { return renderer_.get(); }
    tvg::GlCanvas* canvas() { return canvas_.get(); }
    GLFWwindow* window() { return window_; }
    int width() const { return width_; }
    int height() const { return height_; }

    // Mapping Helpers
    static flexUI::MouseButton glfw_to_button(int button) {
        switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT: return flexUI::MouseButton::Left;
            case GLFW_MOUSE_BUTTON_RIGHT: return flexUI::MouseButton::Right;
            case GLFW_MOUSE_BUTTON_MIDDLE: return flexUI::MouseButton::Middle;
            default: return flexUI::MouseButton::Left;
        }
    }

    static flexUI::KeyCode glfw_to_keycode(int key) {
        switch (key) {
            case GLFW_KEY_LEFT: return flexUI::KeyCode::Left;
            case GLFW_KEY_RIGHT: return flexUI::KeyCode::Right;
            case GLFW_KEY_UP: return flexUI::KeyCode::Up;
            case GLFW_KEY_DOWN: return flexUI::KeyCode::Down;
            case GLFW_KEY_HOME: return flexUI::KeyCode::Home;
            case GLFW_KEY_END: return flexUI::KeyCode::End;
            case GLFW_KEY_BACKSPACE: return flexUI::KeyCode::Backspace;
            case GLFW_KEY_DELETE: return flexUI::KeyCode::Delete;
            case GLFW_KEY_ENTER: return flexUI::KeyCode::Enter;
            case GLFW_KEY_TAB: return flexUI::KeyCode::Tab;
            case GLFW_KEY_ESCAPE: return flexUI::KeyCode::Escape;
            case GLFW_KEY_SPACE: return flexUI::KeyCode::Space;
            case GLFW_KEY_A: return flexUI::KeyCode::A;
            case GLFW_KEY_B: return flexUI::KeyCode::B;
            case GLFW_KEY_C: return flexUI::KeyCode::C;
            case GLFW_KEY_D: return flexUI::KeyCode::D;
            case GLFW_KEY_E: return flexUI::KeyCode::E;
            case GLFW_KEY_F: return flexUI::KeyCode::F;
            case GLFW_KEY_G: return flexUI::KeyCode::G;
            case GLFW_KEY_H: return flexUI::KeyCode::H;
            case GLFW_KEY_I: return flexUI::KeyCode::I;
            case GLFW_KEY_J: return flexUI::KeyCode::J;
            case GLFW_KEY_K: return flexUI::KeyCode::K;
            case GLFW_KEY_L: return flexUI::KeyCode::L;
            case GLFW_KEY_M: return flexUI::KeyCode::M;
            case GLFW_KEY_N: return flexUI::KeyCode::N;
            case GLFW_KEY_O: return flexUI::KeyCode::O;
            case GLFW_KEY_P: return flexUI::KeyCode::P;
            case GLFW_KEY_Q: return flexUI::KeyCode::Q;
            case GLFW_KEY_R: return flexUI::KeyCode::R;
            case GLFW_KEY_S: return flexUI::KeyCode::S;
            case GLFW_KEY_T: return flexUI::KeyCode::T;
            case GLFW_KEY_U: return flexUI::KeyCode::U;
            case GLFW_KEY_V: return flexUI::KeyCode::V;
            case GLFW_KEY_W: return flexUI::KeyCode::W;
            case GLFW_KEY_X: return flexUI::KeyCode::X;
            case GLFW_KEY_Y: return flexUI::KeyCode::Y;
            case GLFW_KEY_Z: return flexUI::KeyCode::Z;
            case GLFW_KEY_0: return flexUI::KeyCode::Num0;
            case GLFW_KEY_1: return flexUI::KeyCode::Num1;
            case GLFW_KEY_2: return flexUI::KeyCode::Num2;
            case GLFW_KEY_3: return flexUI::KeyCode::Num3;
            case GLFW_KEY_4: return flexUI::KeyCode::Num4;
            case GLFW_KEY_5: return flexUI::KeyCode::Num5;
            case GLFW_KEY_6: return flexUI::KeyCode::Num6;
            case GLFW_KEY_7: return flexUI::KeyCode::Num7;
            case GLFW_KEY_8: return flexUI::KeyCode::Num8;
            case GLFW_KEY_9: return flexUI::KeyCode::Num9;
            default: return flexUI::KeyCode::Unknown;
        }
    }

    static int glfw_to_mods(int mods) {
        int r = 0;
        if (mods & GLFW_MOD_SHIFT) r |= (int)flexUI::KeyMod::Shift;
        if (mods & GLFW_MOD_CONTROL) r |= (int)flexUI::KeyMod::Control;
        if (mods & GLFW_MOD_ALT) r |= (int)flexUI::KeyMod::Alt;
        if (mods & GLFW_MOD_SUPER) r |= (int)flexUI::KeyMod::Super;
        return r;
    }

protected:
    virtual bool on_init() { return true; }
    virtual void on_update(float dt) {}
    virtual void on_render() {}
    virtual void on_key(int key, int action, int mods) {}
    virtual void on_mouse_button(int button, int action, int mods) {}
    virtual void on_cursor_pos(double x, double y) {}
    virtual void on_scroll(double dx, double dy) {}
    virtual void on_char(unsigned int codepoint) {}
    virtual void on_composition_start() {}
    virtual void on_composition_update(const std::string& text) {}
    virtual void on_composition_end() {}
    virtual flexUI::Box* ime_box() { return nullptr; }
    virtual flexUI::Box* host_box() { return ime_box(); }
    virtual bool should_render_frame() const { return true; }
    virtual bool remove_canvas_before_render() const { return true; }
    virtual void on_resize(int w, int h) {
        width_ = w;
        height_ = h;
        glfwGetWindowContentScale(window_, &content_scale_x_, &content_scale_y_);
        glViewport(0, 0, w, h);
        if (canvas_) canvas_->target(glfwGetCurrentContext(), 0, w, h, tvg::ColorSpace::ABGR8888S);
    }

    bool load_font(const char* name, const char* path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return false;
        auto size = file.tellg();
        file.seekg(0);
        std::vector<char> buf(size);
        file.read(buf.data(), size);
        return tvg::Text::load(name, buf.data(), (uint32_t)size, "ttf", true) == tvg::Result::Success;
    }

    void sync_ime_caret(flexUI::Box* box) {
#ifdef _WIN32
        flexui_examples::win32_ime::sync_ime_caret(
            glfwGetWin32Window(window_), box, content_scale_y_);
#else
        (void)box;
#endif
    }

    void sync_ime_caret() {
        sync_ime_caret(ime_box());
    }

    void sync_host_cursor() {
        if (!window_) return;
        flexUI::host::sync_cursor(host_box(), current_cursor_name_,
                                  [this](const std::string& desired) {
                                      current_cursor_name_ = desired;
                                      glfwSetCursor(window_, resolve_host_cursor(desired));
                                  });
    }

    void cursor_position(float& x, float& y) const {
        double cursor_x = 0.0;
        double cursor_y = 0.0;
        glfwGetCursorPos(window_, &cursor_x, &cursor_y);
        x = static_cast<float>(cursor_x);
        y = static_cast<float>(cursor_y);
    }

private:
    GLFWcursor* resolve_host_cursor(const std::string& cursor_name) {
        if (cursor_name == "text" || cursor_name == "vertical-text") {
            return ensure_standard_cursor(GLFW_IBEAM_CURSOR, ibeam_cursor_);
        }
        if (cursor_name == "pointer") {
            return ensure_standard_cursor(GLFW_HAND_CURSOR, hand_cursor_);
        }
        if (cursor_name == "crosshair") {
            return ensure_standard_cursor(GLFW_CROSSHAIR_CURSOR, crosshair_cursor_);
        }
#ifdef GLFW_HRESIZE_CURSOR
        if (cursor_name == "ew-resize" || cursor_name == "col-resize" ||
            cursor_name == "e-resize" || cursor_name == "w-resize") {
            return ensure_standard_cursor(GLFW_HRESIZE_CURSOR, hresize_cursor_);
        }
#endif
#ifdef GLFW_VRESIZE_CURSOR
        if (cursor_name == "ns-resize" || cursor_name == "row-resize" ||
            cursor_name == "n-resize" || cursor_name == "s-resize") {
            return ensure_standard_cursor(GLFW_VRESIZE_CURSOR, vresize_cursor_);
        }
#endif
        return ensure_standard_cursor(GLFW_ARROW_CURSOR, arrow_cursor_);
    }

    static void destroy_cursor(GLFWcursor*& cursor) {
        if (!cursor) return;
        glfwDestroyCursor(cursor);
        cursor = nullptr;
    }

    static GLFWcursor* ensure_standard_cursor(int shape, GLFWcursor*& slot) {
        if (!slot) slot = glfwCreateStandardCursor(shape);
        return slot;
    }

    static void key_callback(GLFWwindow* w, int key, int, int action, int mods) {
        auto* app = static_cast<GlfwApp*>(glfwGetWindowUserPointer(w));
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) app->quit();
        else app->on_key(key, action, mods);
        app->sync_host_cursor();
    }

    static void mouse_button_callback(GLFWwindow* w, int button, int action, int mods) {
        auto* app = static_cast<GlfwApp*>(glfwGetWindowUserPointer(w));
        app->on_mouse_button(button, action, mods);
        app->sync_host_cursor();
    }

    static void cursor_pos_callback(GLFWwindow* w, double x, double y) {
        auto* app = static_cast<GlfwApp*>(glfwGetWindowUserPointer(w));
        app->mouse_x_ = x;
        app->mouse_y_ = y;
        app->on_cursor_pos(app->mouse_x_, app->mouse_y_);
        app->sync_host_cursor();
    }

    static void scroll_callback(GLFWwindow* w, double dx, double dy) {
        auto* app = static_cast<GlfwApp*>(glfwGetWindowUserPointer(w));
        app->on_scroll(dx, dy);
        app->sync_host_cursor();
    }

    static void char_callback(GLFWwindow* w, unsigned int codepoint) {
        static_cast<GlfwApp*>(glfwGetWindowUserPointer(w))->on_char(codepoint);
    }

    static void framebuffer_size_callback(GLFWwindow* w, int width, int height) {
        static_cast<GlfwApp*>(glfwGetWindowUserPointer(w))->on_resize(width, height);
    }

#ifdef _WIN32
    static WNDPROC original_wnd_proc;
    static LRESULT CALLBACK hook_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        GlfwApp* app = (GlfwApp*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (app && app->ime_box() && flexui_examples::win32_ime::handle_ime_for_box(
                                      hwnd,
                                      msg,
                                      lp,
                                      app->ime_box(),
                                      app->content_scale_y_)) {
            return 0;
        }
        if (app && flexui_examples::win32_ime::handle_ime_message(
                       hwnd,
                       msg,
                       lp,
                       [app]() { app->on_composition_start(); },
                       [app](const std::string& text) { app->on_composition_update(text); },
                       [app]() { app->on_composition_end(); })) {
            return 0;
        }
        (void)wp;
        return CallWindowProc(original_wnd_proc, hwnd, msg, wp, lp);
    }
#endif

    const char* title_;
    int width_ = 0, height_ = 0;
    float content_scale_x_ = 1.0f, content_scale_y_ = 1.0f;
    double mouse_x_ = 0, mouse_y_ = 0;

    GLFWwindow* window_ = nullptr;
    std::unique_ptr<tvg::GlCanvas> canvas_;
    std::unique_ptr<Renderer> renderer_;
    std::string current_cursor_name_ = "default";
    std::string current_color_scheme_ = "normal";
    GLFWcursor* arrow_cursor_ = nullptr;
    GLFWcursor* ibeam_cursor_ = nullptr;
    GLFWcursor* hand_cursor_ = nullptr;
    GLFWcursor* crosshair_cursor_ = nullptr;
    GLFWcursor* hresize_cursor_ = nullptr;
    GLFWcursor* vresize_cursor_ = nullptr;
};

#ifdef _WIN32
inline WNDPROC GlfwApp::original_wnd_proc = nullptr;
#endif

} // namespace flex
