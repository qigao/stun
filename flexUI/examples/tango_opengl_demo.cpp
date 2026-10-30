/*
 * Explicit Tango OpenGL cell-grid host.
 *
 * Unlike the native TTY sample, this owns a GLFW context and asks gCanvas GL
 * to draw a frozen Tango C11 grid. No CSS, alternate Unicode parser, or TTY
 * fallback. The GL presenter/window is deliberately kept separate from
 * RendererBackend::OpenGL until full product/pixel/retirement acceptance.
 */
#include <backends/tango_opengl/gl_presenter.h>
#include <backends/tango_opengl/cell_input.h>
#include <tui_grid_snapshot.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
using flex::tango_opengl::CellPixelMetrics;
using flex::tango_opengl::ExternalGLCellCreateInfo;

flex::opengl_backend::OpenGLProcAddress get_proc(void*, const char* name) {
    return reinterpret_cast<flex::opengl_backend::OpenGLProcAddress>(
        glfwGetProcAddress(name));
}

struct TangoBuffer {
    tui_buffer_t* buffer = nullptr;
    explicit TangoBuffer(int cols, int rows) : buffer(tui_buffer_create(cols, rows)) {
        if (!buffer) throw std::runtime_error("Tango grid allocation failed");
    }
    ~TangoBuffer() { tui_buffer_destroy(buffer); }
    TangoBuffer(const TangoBuffer&) = delete;
    TangoBuffer& operator=(const TangoBuffer&) = delete;
};

struct TangoSnapshot {
    tui_grid_snapshot_t* snapshot = nullptr;
    TangoSnapshot(const tui_buffer_t* source, std::uint64_t revision) {
        const auto status = tui_grid_snapshot_capture(
            source, revision, 1024, 1024 * 1024, &snapshot);
        if (status != TUI_GRID_SNAPSHOT_OK)
            throw std::runtime_error("Tango cell snapshot failed; invalid input or budget");
    }
    ~TangoSnapshot() { tui_grid_snapshot_destroy(snapshot); }
    TangoSnapshot(const TangoSnapshot&) = delete;
    TangoSnapshot& operator=(const TangoSnapshot&) = delete;
};

void paint_demo_cells(tui_buffer_t* grid) {
    tui_buffer_clear_color(grid, TUI_COLOR(12, 19, 31));
    tui_buffer_text(grid, 2, 1, "TANGO / OPENGL",
                    TUI_COLOR(235, 239, 255), TUI_COLOR(12, 19, 31));
    tui_buffer_text(grid, 2, 3, "One Tango C11 grid, two explicit presenters.",
                    TUI_GREEN, TUI_COLOR(12, 19, 31));
    tui_buffer_text(grid, 2, 5, "TTY = VT/ConPTY. OpenGL = real gCanvas glyphs.",
                    TUI_WHITE, TUI_COLOR(12, 19, 31));
    tui_buffer_text(grid, 2, 7, "Esc to exit. No fallback, no CSS.",
                    TUI_YELLOW, TUI_COLOR(12, 19, 31));
    tui_buffer_set_grapheme(grid, 2, 9, "\xE4\xB8\xAD", 3,
                            TUI_CYAN, TUI_COLOR(12, 19, 31), TUI_STYLE_NONE);
    tui_buffer_set(grid, 4, 9, tui_cell(' ', TUI_WHITE, TUI_COLOR(12, 19, 31)));
    tui_buffer_text(grid, 2, 11, "OpenGL context belongs to the GLFW host.",
                    TUI_WHITE, TUI_COLOR(12, 19, 31));
}

using flex::tango_opengl::CellHostInput;
using flex::tango_opengl::CellMouseAction;
using flex::tango_opengl::CellMouseButton;
using flex::tango_opengl::CellWindowExtents;
using flex::tango_opengl::CellPixelPresenter;
using flex::tango_opengl::CellHostFrameGate;

tui_key_t native_navigation_key(int key) noexcept {
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12)
        return static_cast<tui_key_t>(TUI_KEY_F1 + (key - GLFW_KEY_F1));
    switch (key) {
        case GLFW_KEY_ESCAPE: return TUI_KEY_ESCAPE;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER: return TUI_KEY_ENTER;
        case GLFW_KEY_TAB: return TUI_KEY_TAB;
        case GLFW_KEY_BACKSPACE: return TUI_KEY_BACKSPACE;
        case GLFW_KEY_UP: return TUI_KEY_UP;
        case GLFW_KEY_DOWN: return TUI_KEY_DOWN;
        case GLFW_KEY_LEFT: return TUI_KEY_LEFT;
        case GLFW_KEY_RIGHT: return TUI_KEY_RIGHT;
        case GLFW_KEY_HOME: return TUI_KEY_HOME;
        case GLFW_KEY_END: return TUI_KEY_END;
        case GLFW_KEY_PAGE_UP: return TUI_KEY_PAGE_UP;
        case GLFW_KEY_PAGE_DOWN: return TUI_KEY_PAGE_DOWN;
        case GLFW_KEY_INSERT: return TUI_KEY_INSERT;
        case GLFW_KEY_DELETE: return TUI_KEY_DELETE;
        default: return TUI_KEY_NONE; // Printable text comes ONLY from char callback.
    }
}

std::uint8_t native_modifiers(int glfw_mods) {
    constexpr int known = GLFW_MOD_SHIFT | GLFW_MOD_CONTROL | GLFW_MOD_ALT |
                          GLFW_MOD_SUPER | GLFW_MOD_CAPS_LOCK | GLFW_MOD_NUM_LOCK;
    if ((glfw_mods & ~known) != 0 || (glfw_mods & GLFW_MOD_SUPER) != 0)
        throw std::invalid_argument(
            "TangoGL input cannot represent this GLFW modifier in C11 Tango");
    std::uint8_t modifiers = TUI_MOD_NONE;
    if (glfw_mods & GLFW_MOD_SHIFT) modifiers |= TUI_MOD_SHIFT;
    if (glfw_mods & GLFW_MOD_CONTROL) modifiers |= TUI_MOD_CTRL;
    if (glfw_mods & GLFW_MOD_ALT) modifiers |= TUI_MOD_ALT;
    return modifiers;
}

int current_modifiers(GLFWwindow* window) {
    int modifiers = 0;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
        modifiers |= GLFW_MOD_SHIFT;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)
        modifiers |= GLFW_MOD_CONTROL;
    if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS)
        modifiers |= GLFW_MOD_ALT;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS)
        modifiers |= GLFW_MOD_SUPER;
    return modifiers;
}

CellMouseButton native_button(int button) {
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT: return CellMouseButton::Left;
        case GLFW_MOUSE_BUTTON_MIDDLE: return CellMouseButton::Middle;
        case GLFW_MOUSE_BUTTON_RIGHT: return CellMouseButton::Right;
        default:
            throw std::invalid_argument(
                "TangoGL host mouse button has no C11 Tango mapping");
    }
}

/*
 * GLFW invokes these callbacks synchronously from glfwPollEvents on the
 * SAME GL/UI owner as present(). No new event queue, thread, or renderer.
 * Native events are dispatched into the one application model; C++ exceptions
 * are captured and surfaced outside the C callback boundary.
 */
struct GlfwTangoInput final {
    GLFWwindow* window;
    CellPixelPresenter* presenter;
    CellHostFrameGate gate;
    std::exception_ptr failure;
    double wheel_remainder = 0.0;
    int last_column = -1;
    int last_row = -1;
    int last_leading = -1;
    std::uint32_t last_scalar = 0;
    std::uint64_t last_revision = 0;
    bool dirty = false;

    GlfwTangoInput(GLFWwindow* host, CellPixelPresenter& frame)
        : window(host), presenter(&frame), gate(frame) {
        gate.set_focus(glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE);
        glfwSetWindowUserPointer(window, this);
        glfwSetMouseButtonCallback(window, &on_button);
        glfwSetCursorPosCallback(window, &on_cursor);
        glfwSetScrollCallback(window, &on_scroll);
        glfwSetKeyCallback(window, &on_key);
        glfwSetCharCallback(window, &on_character);
        glfwSetWindowFocusCallback(window, &on_focus);
        glfwSetWindowSizeCallback(window, &on_window_size);
        glfwSetFramebufferSizeCallback(window, &on_framebuffer_size);
    }
    ~GlfwTangoInput() {
        glfwSetMouseButtonCallback(window, nullptr);
        glfwSetCursorPosCallback(window, nullptr);
        glfwSetScrollCallback(window, nullptr);
        glfwSetKeyCallback(window, nullptr);
        glfwSetCharCallback(window, nullptr);
        glfwSetWindowFocusCallback(window, nullptr);
        glfwSetWindowSizeCallback(window, nullptr);
        glfwSetFramebufferSizeCallback(window, nullptr);
        glfwSetWindowUserPointer(window, nullptr);
    }
    GlfwTangoInput(const GlfwTangoInput&) = delete;
    GlfwTangoInput& operator=(const GlfwTangoInput&) = delete;

    CellWindowExtents extents() const {
        CellWindowExtents result{};
        glfwGetWindowSize(window, &result.window_width, &result.window_height);
        glfwGetFramebufferSize(
            window, &result.framebuffer_width, &result.framebuffer_height);
        return result;
    }
    void dispatch(const std::optional<CellHostInput>& translated) {
        if (!translated) return; // No submitted frame/outside its painted cells.
        const tui_event_t& event = translated->event;
        if (event.type == TUI_EVENT_KEY && event.key == TUI_KEY_ESCAPE)
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        if (event.type == TUI_EVENT_MOUSE_PRESS &&
            event.key == TUI_KEY_MOUSE_LEFT) {
            last_column = event.x; // Physical cell, same as TTY mouse semantics.
            last_row = event.y;
            last_leading = translated->leading_column;
            last_revision = translated->grid_revision;
            dirty = true;
            std::cout << "TANGO_GL_NATIVE_MOUSE row=" << last_row
                      << " column=" << last_column
                      << " leading=" << last_leading
                      << " continuation=" << translated->continuation
                      << " revision=" << last_revision << '\n';
        }
        if (event.type == TUI_EVENT_KEY &&
            event.ch >= 32u && event.ch != 127u) {
            last_scalar = event.ch; // Committed Unicode, not IME preedit.
            last_revision = translated->grid_revision;
            dirty = true;
            std::cout << "TANGO_GL_NATIVE_KEY scalar=" << last_scalar
                      << " revision=" << last_revision << '\n';
        }
    }
    void paint_status(tui_buffer_t* grid) const {
        char status[160];
        std::snprintf(status, sizeof(status),
                      "Mouse (%d,%d) lead %d | Unicode U+%04X | frame %llu",
                      last_column, last_row, last_leading,
                      static_cast<unsigned>(last_scalar),
                      static_cast<unsigned long long>(last_revision));
        tui_buffer_text(grid, 1, 17, status,
                        TUI_YELLOW, TUI_COLOR(12, 19, 31));
    }
    void fail() noexcept {
        if (!failure) failure = std::current_exception();
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    template <typename F>
    static void guarded(GLFWwindow* host, F&& fn) noexcept {
        auto* self = static_cast<GlfwTangoInput*>(glfwGetWindowUserPointer(host));
        if (!self || self->failure) return;
        try { fn(*self); }
        catch (...) { self->fail(); }
    }
    void pointer_at(double x, double y, CellMouseAction action,
                    CellMouseButton button, std::uint8_t modifiers) {
        dispatch(gate.pointer(x, y, extents(), action, button, modifiers));
    }
    static void on_button(GLFWwindow* host, int button, int action, int mods) {
        guarded(host, [&](GlfwTangoInput& self) {
            if (action != GLFW_PRESS && action != GLFW_RELEASE)
                throw std::invalid_argument("TangoGL unsupported mouse transition");
            double x = 0.0, y = 0.0;
            glfwGetCursorPos(host, &x, &y);
            self.pointer_at(
                x, y, action == GLFW_PRESS ? CellMouseAction::Press :
                                             CellMouseAction::Release,
                native_button(button), native_modifiers(mods));
        });
    }
    static void on_cursor(GLFWwindow* host, double x, double y) {
        guarded(host, [&](GlfwTangoInput& self) {
            self.pointer_at(x, y, CellMouseAction::Move, CellMouseButton::None,
                            native_modifiers(current_modifiers(host)));
        });
    }
    static void on_scroll(GLFWwindow* host, double dx, double dy) {
        guarded(host, [&](GlfwTangoInput& self) {
            // The native C11 model expresses vertical whole-wheel ticks only.
            // Preserve fractional GLFW deltas; reject unsupported axes/bursts.
            if (!std::isfinite(dx) || !std::isfinite(dy) || dx != 0.0 ||
                std::fabs(dy) > 8.0 || std::fabs(self.wheel_remainder + dy) > 8.0)
                throw std::invalid_argument("TangoGL scroll exceeds C11 wheel profile");
            self.wheel_remainder += dy;
            double x = 0.0, y = 0.0;
            glfwGetCursorPos(host, &x, &y);
            while (self.wheel_remainder >= 1.0) {
                self.pointer_at(x, y, CellMouseAction::WheelUp,
                                CellMouseButton::None,
                                native_modifiers(current_modifiers(host)));
                self.wheel_remainder -= 1.0;
            }
            while (self.wheel_remainder <= -1.0) {
                self.pointer_at(x, y, CellMouseAction::WheelDown,
                                CellMouseButton::None,
                                native_modifiers(current_modifiers(host)));
                self.wheel_remainder += 1.0;
            }
        });
    }
    static void on_key(GLFWwindow* host, int key, int, int action, int mods) {
        guarded(host, [&](GlfwTangoInput& self) {
            if (action == GLFW_RELEASE) return; // C11 Tango has no key-up event.
            if (action != GLFW_PRESS && action != GLFW_REPEAT)
                throw std::invalid_argument("TangoGL unsupported key transition");
            const auto canonical = native_navigation_key(key);
            if (canonical == TUI_KEY_NONE) return;
            self.dispatch(self.gate.key(
                canonical, native_modifiers(mods), self.extents()));
        });
    }
    static void on_character(GLFWwindow* host, unsigned int scalar) {
        guarded(host, [&](GlfwTangoInput& self) {
            self.dispatch(self.gate.character(scalar, self.extents()));
        });
    }
    static void on_focus(GLFWwindow* host, int focused) {
        guarded(host, [&](GlfwTangoInput& self) {
            self.gate.set_focus(focused == GLFW_TRUE);
            self.wheel_remainder = 0.0; // Do not replay a pre-blur scroll.
        });
    }
    static void on_window_size(GLFWwindow* host, int, int) {
        guarded(host, [&](GlfwTangoInput& self) {
            self.gate.invalidate_geometry();
            self.wheel_remainder = 0.0;
        });
    }
    static void on_framebuffer_size(GLFWwindow* host, int, int) {
        guarded(host, [&](GlfwTangoInput& self) {
            self.gate.invalidate_geometry();
            self.wheel_remainder = 0.0;
        });
    }
};
} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: tango_opengl_demo <regular-font.ttf> <bold-font.ttf>\n";
        return 2;
    }
    if (!glfwInit()) {
        std::cerr << "GLFW init failed\n";
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(768, 480,
        "Tango OpenGL C11 Cell Presenter", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        std::cerr << "GLFW cannot create OpenGL 4.1 core context\n";
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int exit_code = 0;
    try {
        const std::string font_path(argv[1]);
        const std::string bold_font_path(argv[2]);
        ExternalGLCellCreateInfo gl;
        gl.get_proc_address = &get_proc;
        gl.font_family = "tango-monospace";
        gl.font_path = font_path;
        gl.bold_font_path = bold_font_path;
        auto presenter = flex::tango_opengl::create_external_gl_presenter(gl);
        TangoBuffer grid(48, 20);
        paint_demo_cells(grid.buffer);
        auto frame = std::make_unique<TangoSnapshot>(grid.buffer, 1);

        CellPixelMetrics layout;
        layout.cell_width = 16.0f;
        layout.cell_height = 24.0f;
        layout.font_size = 15.0f;
        layout.baseline = 18.0f;
        layout.pixel_ratio = 1.0f;
        layout.font_family = gl.font_family;
        layout.expected_grid_revision = 1;
        const CellPixelMetrics base_layout = layout;

        GlfwTangoInput input(window, *presenter);
        std::uint64_t revision = 1;
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            if (input.failure) std::rethrow_exception(input.failure);

            int window_width = 0, window_height = 0;
            int framebuffer_width = 0, framebuffer_height = 0;
            glfwGetWindowSize(window, &window_width, &window_height);
            glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
            if (window_width <= 0 || window_height <= 0 ||
                framebuffer_width <= 0 || framebuffer_height <= 0) {
                // Minimized/unmapped window has no drawable framebuffer.
                // Do not guess a replacement frame or allow stale input.
                input.gate.invalidate_geometry();
                glfwWaitEventsTimeout(0.025);
                continue;
            }
            if (input.dirty) {
                // One existing Tango C11 source/grid. No second cell layout.
                paint_demo_cells(grid.buffer);
                input.paint_status(grid.buffer);
                if (revision == UINT64_MAX)
                    throw std::overflow_error("TangoGL source revision exhausted");
                ++revision;
                frame = std::make_unique<TangoSnapshot>(grid.buffer, revision);
                input.dirty = false;
            }
            // Reproject the SAME immutable Tango C11 cell grid using the
            // actual host-reported logical window and framebuffer extents.
            // Unsupported/transitioning nonuniform DPI fails before GL work.
            CellPixelMetrics intended = base_layout;
            intended.expected_grid_revision = revision;
            const CellWindowExtents current{
                window_width, window_height, framebuffer_width, framebuffer_height};
            layout = flex::tango_opengl::fit_cell_metrics_to_window(
                frame->snapshot, current, intended);
            presenter->present(frame->snapshot, layout);
            glfwSwapBuffers(window);
            // Focus/resize alone cannot make an old frame clickable.
            // Admit only the exact just-presented native window+pixel frame.
            input.gate.accept_presented_frame(input.extents());
        }
        // Presenter and gCanvas GPU resources destruct with current GL
        // context, before glfwDestroyWindow below.
    } catch (const std::exception& error) {
        std::cerr << "TangoOpenGL rejected the frame: " << error.what() << "\n";
        exit_code = 1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return exit_code;
}
