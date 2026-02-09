#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <thorvg.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <imm.h>
#pragma comment(lib, "imm32.lib")
#endif
#include <flex/bridge/renderer.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>

namespace flex {

class GlfwApp {
public:
    GlfwApp(const char* title, int width, int height)
        : title_(title), width_(width), height_(height) {}

    virtual ~GlfwApp() {
        renderer_.reset();
        canvas_.reset();
        tvg::Initializer::term();
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
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
        original_wnd_proc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)hook_wnd_proc);
        
        // GLFW disables IME by default, re-enable it by associating a new context
        HIMC himc = ImmGetContext(hwnd);
        if (!himc) {
            himc = ImmCreateContext();
            ImmAssociateContext(hwnd, himc);
        } else {
            ImmReleaseContext(hwnd, himc);
        }
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
            on_update(dt);

            glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            canvas_->remove();
            on_render();
            glfwSwapBuffers(window_);
        }
    }

    void quit() { glfwSetWindowShouldClose(window_, GLFW_TRUE); }

    void update_ime_position(int x, int y) {
#ifdef _WIN32
        HWND hwnd = glfwGetWin32Window(window_);
        HIMC himc = ImmGetContext(hwnd);
        if (himc) {
            // COMPOSITIONFORM needs client coordinates
            COMPOSITIONFORM cf;
            cf.dwStyle = CFS_POINT;
            cf.ptCurrentPos.x = x;
            cf.ptCurrentPos.y = y;
            ImmSetCompositionWindow(himc, &cf);

            // Also set candidate window position
            CANDIDATEFORM caf;
            caf.dwIndex = 0;
            caf.dwStyle = CFS_CANDIDATEPOS;
            caf.ptCurrentPos.x = x;
            caf.ptCurrentPos.y = y;
            ImmSetCandidateWindow(himc, &caf);

            // Set font height to help IME positioning
            LOGFONTA lf;
            memset(&lf, 0, sizeof(lf));
            lf.lfHeight = (int)(-20 * content_scale_y_); 
            lf.lfWeight = FW_NORMAL;
            lf.lfCharSet = DEFAULT_CHARSET;
            strcpy(lf.lfFaceName, "Microsoft YaHei");
            ImmSetCompositionFontA(himc, &lf);

            ImmReleaseContext(hwnd, himc);
        }
#endif
    }

    // Physical pixel position of mouse
    float mouse_x() const { return (float)mouse_x_; }
    float mouse_y() const { return (float)mouse_y_; }

    // Accessors
    Renderer* renderer() { return renderer_.get(); }
    tvg::GlCanvas* canvas() { return canvas_.get(); }
    GLFWwindow* window() { return window_; }
    int width() const { return width_; }
    int height() const { return height_; }

    // Mapping Helpers
    static flex::MouseButton glfw_to_button(int button) {
        switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT: return flex::MouseButton::Left;
            case GLFW_MOUSE_BUTTON_RIGHT: return flex::MouseButton::Right;
            case GLFW_MOUSE_BUTTON_MIDDLE: return flex::MouseButton::Middle;
            default: return flex::MouseButton::Left;
        }
    }

    static flex::KeyCode glfw_to_keycode(int key) {
        switch (key) {
            case GLFW_KEY_LEFT: return flex::KeyCode::Left;
            case GLFW_KEY_RIGHT: return flex::KeyCode::Right;
            case GLFW_KEY_UP: return flex::KeyCode::Up;
            case GLFW_KEY_DOWN: return flex::KeyCode::Down;
            case GLFW_KEY_HOME: return flex::KeyCode::Home;
            case GLFW_KEY_END: return flex::KeyCode::End;
            case GLFW_KEY_BACKSPACE: return flex::KeyCode::Backspace;
            case GLFW_KEY_DELETE: return flex::KeyCode::Delete;
            case GLFW_KEY_ENTER: return flex::KeyCode::Enter;
            case GLFW_KEY_TAB: return flex::KeyCode::Tab;
            case GLFW_KEY_ESCAPE: return flex::KeyCode::Escape;
            case GLFW_KEY_SPACE: return flex::KeyCode::Space;
            case GLFW_KEY_A: return flex::KeyCode::A;
            case GLFW_KEY_B: return flex::KeyCode::B;
            case GLFW_KEY_C: return flex::KeyCode::C;
            case GLFW_KEY_D: return flex::KeyCode::D;
            case GLFW_KEY_E: return flex::KeyCode::E;
            case GLFW_KEY_F: return flex::KeyCode::F;
            case GLFW_KEY_G: return flex::KeyCode::G;
            case GLFW_KEY_H: return flex::KeyCode::H;
            case GLFW_KEY_I: return flex::KeyCode::I;
            case GLFW_KEY_J: return flex::KeyCode::J;
            case GLFW_KEY_K: return flex::KeyCode::K;
            case GLFW_KEY_L: return flex::KeyCode::L;
            case GLFW_KEY_M: return flex::KeyCode::M;
            case GLFW_KEY_N: return flex::KeyCode::N;
            case GLFW_KEY_O: return flex::KeyCode::O;
            case GLFW_KEY_P: return flex::KeyCode::P;
            case GLFW_KEY_Q: return flex::KeyCode::Q;
            case GLFW_KEY_R: return flex::KeyCode::R;
            case GLFW_KEY_S: return flex::KeyCode::S;
            case GLFW_KEY_T: return flex::KeyCode::T;
            case GLFW_KEY_U: return flex::KeyCode::U;
            case GLFW_KEY_V: return flex::KeyCode::V;
            case GLFW_KEY_W: return flex::KeyCode::W;
            case GLFW_KEY_X: return flex::KeyCode::X;
            case GLFW_KEY_Y: return flex::KeyCode::Y;
            case GLFW_KEY_Z: return flex::KeyCode::Z;
            case GLFW_KEY_0: return flex::KeyCode::Num0;
            case GLFW_KEY_1: return flex::KeyCode::Num1;
            case GLFW_KEY_2: return flex::KeyCode::Num2;
            case GLFW_KEY_3: return flex::KeyCode::Num3;
            case GLFW_KEY_4: return flex::KeyCode::Num4;
            case GLFW_KEY_5: return flex::KeyCode::Num5;
            case GLFW_KEY_6: return flex::KeyCode::Num6;
            case GLFW_KEY_7: return flex::KeyCode::Num7;
            case GLFW_KEY_8: return flex::KeyCode::Num8;
            case GLFW_KEY_9: return flex::KeyCode::Num9;
            default: return flex::KeyCode::Unknown;
        }
    }

    static int glfw_to_mods(int mods) {
        int r = 0;
        if (mods & GLFW_MOD_SHIFT) r |= (int)flex::KeyMod::Shift;
        if (mods & GLFW_MOD_CONTROL) r |= (int)flex::KeyMod::Control;
        if (mods & GLFW_MOD_ALT) r |= (int)flex::KeyMod::Alt;
        if (mods & GLFW_MOD_SUPER) r |= (int)flex::KeyMod::Super;
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

private:
    static void key_callback(GLFWwindow* w, int key, int, int action, int mods) {
        auto* app = static_cast<GlfwApp*>(glfwGetWindowUserPointer(w));
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) app->quit();
        else app->on_key(key, action, mods);
    }

    static void mouse_button_callback(GLFWwindow* w, int button, int action, int mods) {
        static_cast<GlfwApp*>(glfwGetWindowUserPointer(w))->on_mouse_button(button, action, mods);
    }

    static void cursor_pos_callback(GLFWwindow* w, double x, double y) {
        auto* app = static_cast<GlfwApp*>(glfwGetWindowUserPointer(w));
        // Keep coordinates logical for high-DPI consistency.
        // Screen-to-Physical scaling is handled by the renderer global transform.
        app->mouse_x_ = (float)x;
        app->mouse_y_ = (float)y;
        app->on_cursor_pos(app->mouse_x_, app->mouse_y_);
    }

    static void scroll_callback(GLFWwindow* w, double dx, double dy) {
        static_cast<GlfwApp*>(glfwGetWindowUserPointer(w))->on_scroll(dx, dy);
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
        if (app) {
            switch (msg) {
                case WM_IME_SETCONTEXT:
                    // Ensure all IME UI components are shown
                    lp |= ISC_SHOWUIALL; 
                    break;
                case WM_IME_STARTCOMPOSITION:
                    // std::cout << "IME Start" << std::endl;
                    {
                        HIMC himc = ImmGetContext(hwnd);
                        if (himc) {
                            ImmSetOpenStatus(himc, TRUE);
                            ImmReleaseContext(hwnd, himc);
                        }
                    }
                    app->on_composition_start();
                    break;
                case WM_IME_COMPOSITION: {
                    // std::cout << "IME Comp: " << lp << std::endl;
                    HIMC himc = ImmGetContext(hwnd);
                    if (lp & GCS_COMPSTR) {
                        int len = ImmGetCompositionStringW(himc, GCS_COMPSTR, NULL, 0);
                        if (len > 0) {
                            std::vector<wchar_t> buf(len / 2 + 1);
                            ImmGetCompositionStringW(himc, GCS_COMPSTR, buf.data(), len);
                            buf[len / 2] = 0;
                            // Convert WCHAR to UTF-8
                            int utf8_len = WideCharToMultiByte(CP_UTF8, 0, buf.data(), -1, NULL, 0, NULL, NULL);
                            std::string utf8_buf(utf8_len - 1, '\0');
                            WideCharToMultiByte(CP_UTF8, 0, buf.data(), -1, &utf8_buf[0], utf8_len, NULL, NULL);
                            app->on_composition_update(utf8_buf);
                        } else {
                            app->on_composition_update("");
                        }
                    }
                    ImmReleaseContext(hwnd, himc);
                    break;
                }
                case WM_IME_ENDCOMPOSITION:
                    // std::cout << "IME End" << std::endl;
                    app->on_composition_end();
                    break;
                case WM_IME_NOTIFY:
                    // std::cout << "IME Notify: " << wp << std::endl;
                    break;
            }
        }
        return CallWindowProc(original_wnd_proc, hwnd, msg, wp, lp);
    }
#endif

protected:
    const char* title_;
    int width_ = 0, height_ = 0;
    float content_scale_x_ = 1.0f, content_scale_y_ = 1.0f;
    double mouse_x_ = 0, mouse_y_ = 0;

    GLFWwindow* window_ = nullptr;
    std::unique_ptr<tvg::GlCanvas> canvas_;
    std::unique_ptr<Renderer> renderer_;
};

#ifdef _WIN32
inline WNDPROC GlfwApp::original_wnd_proc = nullptr;
#endif

} // namespace flex
