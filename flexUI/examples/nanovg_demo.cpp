#include <glad/glad.h>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "win32_ime_helper.h"
#endif
#include <nanovg.h>
#define NANOVG_GL3 1
#include <nanovg_gl.h>

#include <backends/nanovg/init.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/host_bridge.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/widgets/progressbar_widget.h>
#include <flexUI/widgets/textarea_widget.h>
#include "host_input_bridge.h"
#include "host_media_bridge.h"
#include "renderer_capability_label.h"

#include <iostream>
#include <memory>
#include <string>

namespace {

constexpr const char* kCss = R"(
#root {
    width: 100%;
    height: 100%;
    display: flex;
    flex-direction: column;
    gap: 14px;
    padding: 24px;
    background-color: #101418;
}

#hero {
    display: flex;
    flex-direction: column;
    gap: 8px;
    padding: 20px;
    background-color: #172026;
    border-radius: 10px;
}

#title {
    font-size: 26px;
    color: #d8f3ff;
}

#subtitle {
    font-size: 13px;
    color: #8fb8c9;
}

#backend-note {
    font-size: 12px;
    color: #6f94a2;
}

#controls {
    display: flex;
    flex-direction: row;
    gap: 10px;
    align-items: center;
}

#search {
    width: 260px;
    height: 36px;
}

#editor {
    width: 100%;
    height: 110px;
}

#progress {
    width: 100%;
    height: 18px;
}

.btn-ghost {
    background-color: #1f313a;
    color: #d8f3ff;
}
)";

bool load_demo_fonts() {
    bool ok = false;
    ok |= flex::nanovg_backend::load_font("Arial", "C:/Windows/Fonts/arial.ttf");
    ok |= flex::nanovg_backend::load_font("Consolas", "C:/Windows/Fonts/consola.ttf");
    ok |= flex::nanovg_backend::load_font("Segoe UI", "C:/Windows/Fonts/segoeui.ttf");
    return ok;
}

flexUI::MouseButton glfw_to_button(int button) {
    switch (button) {
        case GLFW_MOUSE_BUTTON_RIGHT: return flexUI::MouseButton::Right;
        case GLFW_MOUSE_BUTTON_MIDDLE: return flexUI::MouseButton::Middle;
        default: return flexUI::MouseButton::Left;
    }
}

flexUI::KeyCode glfw_to_keycode(int key) {
    switch (key) {
        case GLFW_KEY_LEFT: return flexUI::KeyCode::Left;
        case GLFW_KEY_RIGHT: return flexUI::KeyCode::Right;
        case GLFW_KEY_UP: return flexUI::KeyCode::Up;
        case GLFW_KEY_DOWN: return flexUI::KeyCode::Down;
        case GLFW_KEY_HOME: return flexUI::KeyCode::Home;
        case GLFW_KEY_END: return flexUI::KeyCode::End;
        case GLFW_KEY_PAGE_UP: return flexUI::KeyCode::PageUp;
        case GLFW_KEY_PAGE_DOWN: return flexUI::KeyCode::PageDown;
        case GLFW_KEY_BACKSPACE: return flexUI::KeyCode::Backspace;
        case GLFW_KEY_DELETE: return flexUI::KeyCode::Delete;
        case GLFW_KEY_ENTER: return flexUI::KeyCode::Enter;
        case GLFW_KEY_TAB: return flexUI::KeyCode::Tab;
        case GLFW_KEY_ESCAPE: return flexUI::KeyCode::Escape;
        case GLFW_KEY_SPACE: return flexUI::KeyCode::Space;
        default:
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
                return static_cast<flexUI::KeyCode>('A' + (key - GLFW_KEY_A));
            }
            if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
                return static_cast<flexUI::KeyCode>('0' + (key - GLFW_KEY_0));
            }
            return flexUI::KeyCode::Unknown;
    }
}

int glfw_to_mods(int mods) {
    int result = 0;
    if (mods & GLFW_MOD_SHIFT) result |= static_cast<int>(flexUI::KeyMod::Shift);
    if (mods & GLFW_MOD_CONTROL) result |= static_cast<int>(flexUI::KeyMod::Control);
    if (mods & GLFW_MOD_ALT) result |= static_cast<int>(flexUI::KeyMod::Alt);
    if (mods & GLFW_MOD_SUPER) result |= static_cast<int>(flexUI::KeyMod::Super);
    return result;
}

std::string utf8_from_codepoint(unsigned int codepoint) {
    std::string utf8;
    if (codepoint <= 0x7F) {
        utf8.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        utf8.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        utf8.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        utf8.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    return utf8;
}

GLFWcursor* ensure_standard_cursor(GLFWcursor*& slot, int shape) {
    if (!slot) slot = glfwCreateStandardCursor(shape);
    return slot;
}

void destroy_standard_cursor(GLFWcursor*& cursor) {
    if (!cursor) return;
    glfwDestroyCursor(cursor);
    cursor = nullptr;
}

class NanoVGFlexUIDemo {
public:
    bool init() {
        if (!glfwInit()) {
            std::cerr << "GLFW init failed\n";
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window_ = glfwCreateWindow(1100, 760, "flexUI NanoVG Demo", nullptr, nullptr);
        if (!window_) {
            std::cerr << "Window creation failed\n";
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1);
        glfwSetWindowUserPointer(window_, this);
        glfwSetMouseButtonCallback(window_, mouse_button_callback);
        glfwSetCursorPosCallback(window_, cursor_pos_callback);
        glfwSetScrollCallback(window_, scroll_callback);
        glfwSetKeyCallback(window_, key_callback);
        glfwSetCharCallback(window_, char_callback);
        glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);
#ifdef _WIN32
        HWND hwnd = glfwGetWin32Window(window_);
        original_wnd_proc_ = flexui_examples::win32_ime::subclass_window(hwnd, this, hook_wnd_proc);
        flexui_examples::win32_ime::enable_ime(hwnd);
#endif

        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            std::cerr << "GLAD init failed\n";
            return false;
        }

        vg_ = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
        if (!vg_) {
            std::cerr << "NanoVG context creation failed\n";
            return false;
        }

        flex::nanovg_backend::init();
        flex::nanovg_backend::register_backend();
        load_demo_fonts();

        renderer_ = flex::nanovg_backend::create_renderer(vg_);
        if (!renderer_) {
            std::cerr << "NanoVG renderer creation failed\n";
            return false;
        }

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window_, &width, &height);
        glfwGetWindowContentScale(window_, &content_scale_x_, &content_scale_y_);

        box_ = std::make_unique<flexUI::Box>(renderer_.get());
        box_->set_viewport(static_cast<float>(width), static_cast<float>(height));
        flexui_examples::media::sync_environment(box_.get());
        box_->load_css(kCss);
        build_ui();
        return true;
    }

    ~NanoVGFlexUIDemo() {
        box_.reset();
        renderer_.reset();
#ifdef _WIN32
        if (window_ && original_wnd_proc_) {
            HWND hwnd = glfwGetWin32Window(window_);
            flexui_examples::win32_ime::restore_window_proc(hwnd, original_wnd_proc_);
        }
#endif
        destroy_standard_cursor(arrow_cursor_);
        destroy_standard_cursor(ibeam_cursor_);
        destroy_standard_cursor(hand_cursor_);
        destroy_standard_cursor(crosshair_cursor_);
        destroy_standard_cursor(hresize_cursor_);
        destroy_standard_cursor(vresize_cursor_);
        if (vg_) nvgDeleteGL3(vg_);
        if (window_) glfwDestroyWindow(window_);
        glfwTerminate();
    }

    void run() {
        double last = glfwGetTime();
        while (!glfwWindowShouldClose(window_)) {
            double now = glfwGetTime();
            float dt = static_cast<float>(now - last);
            last = now;

            glfwPollEvents();
            flexui_examples::media::sync_environment(box_.get());
            progress_ += dt * 22.0f;
            if (progress_ > 100.0f) progress_ = 0.0f;
            if (progress_elem_ && progress_elem_->widget) {
                static_cast<flexUI::ProgressBarWidget*>(progress_elem_->widget)->set_value(progress_);
                progress_elem_->mark_paint_dirty();
            }

            box_->update_time(dt * 1000.0f);
            box_->update();
            sync_ime_caret();
            sync_host_cursor();
            glfwSwapBuffers(window_);
        }
    }

private:
    void build_ui() {
        auto* root = box_->create("div", "root");
        box_->set_root(root);

        auto* hero = box_->create("div", "hero");
        root->append(hero);

        auto* title = box_->create_widget<flexUI::LabelWidget>("div", "title", "flexUI on NanoVG");
        hero->append(title);

        auto* subtitle = box_->create_widget<flexUI::LabelWidget>(
            "div", "subtitle", "Minimal GLFW + NanoVG host with flexUI events, input and progress.");
        hero->append(subtitle);

        const auto caps = box_->renderer_capabilities();
        auto* backend_note = box_->create_widget<flexUI::LabelWidget>(
            "div", "backend-note",
            std::string("NanoVG ") + flexui_examples::renderer_capability_label(caps));
        hero->append(backend_note);

        auto* controls = box_->create("div", "controls");
        root->append(controls);

        auto* button = box_->create_widget<flexUI::ButtonWidget>("button", "", "Trigger");
        button->on_click([this]() {
            status_label_->set_text("Action fired");
            status_elem_->mark_paint_dirty();
        });
        controls->append(button);

        auto* ghost = box_->create_widget<flexUI::ButtonWidget>("button", "", "Secondary");
        ghost->add_class("btn-ghost");
        controls->append(ghost);

        auto* search = box_->create_widget<flexUI::InputWidget>("input", "search");
        static_cast<flexUI::InputWidget*>(search->widget)->set_placeholder("Type to test text input...");
        controls->append(search);

        status_elem_ = box_->create_widget<flexUI::LabelWidget>("div", "", "Ready");
        status_label_ = static_cast<flexUI::LabelWidget*>(status_elem_->widget);
        controls->append(status_elem_);

        auto* editor = box_->create_widget<flexUI::TextAreaWidget>("div", "editor");
        static_cast<flexUI::TextAreaWidget*>(editor->widget)->set_placeholder("Textarea on NanoVG...");
        root->append(editor);

        progress_elem_ = box_->create_widget<flexUI::ProgressBarWidget>("div", "progress");
        static_cast<flexUI::ProgressBarWidget*>(progress_elem_->widget)->set_value(progress_);
        root->append(progress_elem_);
    }

    void on_mouse_button(int button, int action) {
        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(window_, &x, &y);
        x *= content_scale_x_;
        y *= content_scale_y_;
        auto event = (action == GLFW_PRESS)
            ? flexUI::Event::mouse_down(static_cast<float>(x), static_cast<float>(y), glfw_to_button(button))
            : flexUI::Event::mouse_up(static_cast<float>(x), static_cast<float>(y), glfw_to_button(button));
        box_->dispatch_event(event);
        sync_host_cursor();
    }

    void on_cursor_pos(double x, double y) {
        auto event = flexUI::Event::mouse_move(static_cast<float>(x * content_scale_x_),
                                               static_cast<float>(y * content_scale_y_));
        box_->dispatch_event(event);
        sync_host_cursor();
    }

    void on_scroll(double dx, double dy) {
        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(window_, &x, &y);
        x *= content_scale_x_;
        y *= content_scale_y_;
        auto event = flexUI::Event::mouse_wheel(static_cast<float>(x), static_cast<float>(y),
                                                static_cast<float>(dx), static_cast<float>(dy));
        box_->dispatch_event(event);
        sync_host_cursor();
    }

    void on_key(int key, int action, int mods) {
        auto event = (action == GLFW_PRESS || action == GLFW_REPEAT)
            ? flexUI::Event::key_down(glfw_to_keycode(key), glfw_to_mods(mods))
            : flexUI::Event::key_up(glfw_to_keycode(key), glfw_to_mods(mods));
        box_->dispatch_event(event);
        sync_host_cursor();
    }

    void on_char(unsigned int codepoint) {
        flexui_examples::dispatch_text_input_if_focused(
            box_.get(), utf8_from_codepoint(codepoint));
    }

    void on_resize(int width, int height) {
        glViewport(0, 0, width, height);
        glfwGetWindowContentScale(window_, &content_scale_x_, &content_scale_y_);
        box_->set_viewport(static_cast<float>(width), static_cast<float>(height));
        box_->invalidate();
        sync_host_cursor();
    }

    GLFWcursor* resolve_host_cursor(const std::string& cursor_name) {
        if (cursor_name == "text" || cursor_name == "vertical-text") {
            return ensure_standard_cursor(ibeam_cursor_, GLFW_IBEAM_CURSOR);
        }
        if (cursor_name == "pointer") {
            return ensure_standard_cursor(hand_cursor_, GLFW_HAND_CURSOR);
        }
        if (cursor_name == "crosshair") {
            return ensure_standard_cursor(crosshair_cursor_, GLFW_CROSSHAIR_CURSOR);
        }
#ifdef GLFW_HRESIZE_CURSOR
        if (cursor_name == "ew-resize" || cursor_name == "col-resize" ||
            cursor_name == "e-resize" || cursor_name == "w-resize") {
            return ensure_standard_cursor(hresize_cursor_, GLFW_HRESIZE_CURSOR);
        }
#endif
#ifdef GLFW_VRESIZE_CURSOR
        if (cursor_name == "ns-resize" || cursor_name == "row-resize" ||
            cursor_name == "n-resize" || cursor_name == "s-resize") {
            return ensure_standard_cursor(vresize_cursor_, GLFW_VRESIZE_CURSOR);
        }
#endif
        return ensure_standard_cursor(arrow_cursor_, GLFW_ARROW_CURSOR);
    }

    void sync_host_cursor() {
        if (!window_) return;
        flexUI::host::sync_cursor(box_.get(), current_cursor_name_,
                                  [this](const std::string& desired) {
                                      current_cursor_name_ = desired;
                                      glfwSetCursor(window_, resolve_host_cursor(desired));
                                  });
    }

#ifdef _WIN32
    void sync_ime_caret() {
        flexui_examples::win32_ime::sync_ime_caret(
            glfwGetWin32Window(window_), box_.get(), content_scale_y_);
    }

    static LRESULT CALLBACK hook_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto* app = reinterpret_cast<NanoVGFlexUIDemo*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (flexui_examples::win32_ime::handle_ime_for_box(
                hwnd,
                msg,
                lp,
                app ? app->box_.get() : nullptr,
                app ? app->content_scale_y_ : 1.0f)) {
            return 0;
        }
        (void)wp;
        return CallWindowProc(app ? app->original_wnd_proc_ : DefWindowProc, hwnd, msg, wp, lp);
    }
#else
    void sync_ime_caret() {}
#endif

    static void mouse_button_callback(GLFWwindow* window, int button, int action, int) {
        static_cast<NanoVGFlexUIDemo*>(glfwGetWindowUserPointer(window))->on_mouse_button(button, action);
    }

    static void cursor_pos_callback(GLFWwindow* window, double x, double y) {
        static_cast<NanoVGFlexUIDemo*>(glfwGetWindowUserPointer(window))->on_cursor_pos(x, y);
    }

    static void scroll_callback(GLFWwindow* window, double dx, double dy) {
        static_cast<NanoVGFlexUIDemo*>(glfwGetWindowUserPointer(window))->on_scroll(dx, dy);
    }

    static void key_callback(GLFWwindow* window, int key, int, int action, int mods) {
        static_cast<NanoVGFlexUIDemo*>(glfwGetWindowUserPointer(window))->on_key(key, action, mods);
    }

    static void char_callback(GLFWwindow* window, unsigned int codepoint) {
        static_cast<NanoVGFlexUIDemo*>(glfwGetWindowUserPointer(window))->on_char(codepoint);
    }

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
        static_cast<NanoVGFlexUIDemo*>(glfwGetWindowUserPointer(window))->on_resize(width, height);
    }

    GLFWwindow* window_ = nullptr;
    NVGcontext* vg_ = nullptr;
    std::unique_ptr<flex::Renderer> renderer_;
    std::unique_ptr<flexUI::Box> box_;
    float content_scale_x_ = 1.0f;
    float content_scale_y_ = 1.0f;
    std::string current_cursor_name_ = "default";
    GLFWcursor* arrow_cursor_ = nullptr;
    GLFWcursor* ibeam_cursor_ = nullptr;
    GLFWcursor* hand_cursor_ = nullptr;
    GLFWcursor* crosshair_cursor_ = nullptr;
    GLFWcursor* hresize_cursor_ = nullptr;
    GLFWcursor* vresize_cursor_ = nullptr;
#ifdef _WIN32
    WNDPROC original_wnd_proc_ = nullptr;
#endif
    float progress_ = 28.0f;
    flexUI::Element* progress_elem_ = nullptr;
    flexUI::Element* status_elem_ = nullptr;
    flexUI::LabelWidget* status_label_ = nullptr;
};

} // namespace

int main() {
    NanoVGFlexUIDemo app;
    if (!app.init()) {
        return 1;
    }
    app.run();
    return 0;
}
