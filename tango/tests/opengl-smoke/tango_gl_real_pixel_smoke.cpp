/*
 * Real desktop OpenGL/Mesa cell-grid pixel acceptance.
 *
 * The test uses the ACTUAL production Tango C11 snapshot, gCanvas shader
 * renderer and a real GLFW GL4.1 framebuffer. No fake renderer, no CSS,
 * no TTY fallback, and no CPU-generated replacement image.
 */
#include <backends/tango_opengl/gl_presenter.h>
#include <backends/tango_opengl/cell_input.h>
#include <tui_grid_snapshot.h>
#include <GLFW/glfw3.h>
// This existing Mesa/Xvfb-only test injects input at the X server, rather
// than directly invoking an application callback or fabricating events.
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr int kColumns = 8;
constexpr int kRows = 4;
constexpr int kCellWidth = 32;
constexpr int kCellHeight = 24;
constexpr int kWidth = kColumns * kCellWidth;
constexpr int kHeight = kRows * kCellHeight;
using Pixel = std::array<std::uint8_t, 4>;

void require(bool ok, const char* description) {
    if (!ok) throw std::runtime_error(description);
}

struct GlfwSession final {
    GlfwSession() {
        require(glfwInit() == GLFW_TRUE, "GLFW cannot initialize with native X11/Mesa");
    }
    ~GlfwSession() { glfwTerminate(); }
    GlfwSession(const GlfwSession&) = delete;
    GlfwSession& operator=(const GlfwSession&) = delete;
};

struct GlfwWindow final {
    GLFWwindow* handle = nullptr;
    GlfwWindow() {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_STENCIL_BITS, 8);
        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        handle = glfwCreateWindow(kWidth, kHeight, "Tango GL real pixel smoke",
                                  nullptr, nullptr);
        require(handle != nullptr, "GLFW cannot create a real OpenGL 4.1 context");
        glfwMakeContextCurrent(handle);
        glfwSwapInterval(0);
        int width = 0, height = 0;
        glfwGetFramebufferSize(handle, &width, &height);
        require(width == kWidth && height == kHeight,
                "GLFW framebuffer size differs from the qualified Tango pixel grid");
        require(glGetString(GL_VERSION) != nullptr, "No live OpenGL version");
        require(glGetString(GL_RENDERER) != nullptr, "No live OpenGL renderer");
        require(glGetString(GL_VENDOR) != nullptr, "No live OpenGL vendor");
        const std::string renderer_name(
            reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        std::cout << "GL_VERSION=" << glGetString(GL_VERSION) << '\n'
                  << "GL_RENDERER=" << glGetString(GL_RENDERER) << '\n'
                  << "GL_VENDOR=" << glGetString(GL_VENDOR) << '\n';
        // The standalone job is a Mesa/llvmpipe software-driver gate, NOT a
        // physical GPU acceptance. Do not trust the environment variable
        // alone to prove which driver actually produced these pixels.
        require(renderer_name.find("llvmpipe") != std::string::npos,
                "Expected real Mesa llvmpipe OpenGL driver, not an unverified renderer");
    }
    ~GlfwWindow() {
        if (handle) {
            glfwMakeContextCurrent(handle);
            glfwDestroyWindow(handle);
        }
    }
    GlfwWindow(const GlfwWindow&) = delete;
    GlfwWindow& operator=(const GlfwWindow&) = delete;
};

struct OwnedGrid final {
    tui_buffer_t* source = nullptr;
    tui_grid_snapshot_t* frame = nullptr;
    OwnedGrid() {
        source = tui_buffer_create(kColumns, kRows);
        require(source != nullptr, "Tango source buffer allocation failed");
        tui_buffer_clear_color(source, TUI_COLOR(14, 23, 52));
        tui_buffer_set(source, 1, 1,
                       tui_cell('A', TUI_WHITE, TUI_COLOR(220, 25, 28)));
        tui_buffer_set(source, 5, 1,
                       tui_cell('B', TUI_WHITE, TUI_COLOR(22, 190, 35)));
        require(tui_grid_snapshot_capture(
                    source, 101, kColumns * kRows, 8192, &frame) ==
                    TUI_GRID_SNAPSHOT_OK,
                "Native Tango cell snapshot or validation failed");
        require(frame != nullptr, "Tango snapshot missing after accepted capture");
        // A genuine independent frame; source can now be destroyed/changed.
        tui_buffer_destroy(source);
        source = nullptr;
    }
    ~OwnedGrid() {
        tui_grid_snapshot_destroy(frame);
        tui_buffer_destroy(source);
    }
    OwnedGrid(const OwnedGrid&) = delete;
    OwnedGrid& operator=(const OwnedGrid&) = delete;
};

flex::opengl_backend::OpenGLProcAddress gl_proc(void*, const char* name) {
    return reinterpret_cast<flex::opengl_backend::OpenGLProcAddress>(
        glfwGetProcAddress(name));
}

bool exact_gl_context_is_current(void* opaque_window) {
    return glfwGetCurrentContext() == static_cast<GLFWwindow*>(opaque_window);
}

std::vector<std::uint8_t> read_back_rgba(
    int width = kWidth, int height = kHeight) {
    require(width > 0 && height > 0 &&
            static_cast<std::uint64_t>(width) * height <= 4096u * 4096u,
            "TangoGL requested framebuffer readback exceeds bounded pixel budget");
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(width) * height * 4u);
    glFinish();
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels.data());
    require(glGetError() == GL_NO_ERROR,
            "Native glReadPixels failed; GPU framebuffer cannot be verified");
    return pixels;
}

Pixel at(const std::vector<std::uint8_t>& image, int x, int top_y,
         int width = kWidth, int height = kHeight) {
    require(width > 0 && height > 0 &&
            x >= 0 && x < width && top_y >= 0 && top_y < height &&
            image.size() == static_cast<std::size_t>(width) * height * 4u,
            "Invalid Tango GPU pixel sampling coordinates or source size");
    const auto offset =
        static_cast<std::size_t>(((height - 1 - top_y) * width + x) * 4);
    return {image[offset], image[offset + 1], image[offset + 2],
            image[offset + 3]};
}

bool has_white_glyph(const std::vector<std::uint8_t>& image,
                     int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const Pixel p = at(image, x, y);
            if (p[0] > 200 && p[1] > 200 && p[2] > 200 && p[3] > 240)
                return true;
        }
    }
    return false;
}

void save_ppm(const std::vector<std::uint8_t>& rgba,
              const std::string& destination,
              int width = kWidth, int height = kHeight) {
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    require(output.is_open(), "Cannot create exact-head Tango GL pixel artifact");
    require(width > 0 && height > 0 &&
            rgba.size() == static_cast<std::size_t>(width) * height * 4u,
            "TangoGL cannot save mismatched resized pixel bytes");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const Pixel p = at(rgba, x, y, width, height);
            output.write(reinterpret_cast<const char*>(p.data()), 3);
        }
    }
    output.flush();
    require(output.good(), "Cannot write complete Tango GL pixel artifact");
}

void require_rgb_dominance(const Pixel& p, int channel,
                           const char* description) {
    require(p[3] >= 240, "Tango GPU pixel alpha is not opaque");
    if (channel == 0) {
        require(p[0] > 150 && p[0] > p[1] + 70 &&
                p[0] > p[2] + 70, description);
    } else if (channel == 1) {
        require(p[1] > 120 && p[1] > p[0] + 70 &&
                p[1] > p[2] + 70, description);
    } else {
        require(p[2] > p[0] + 18 && p[2] > p[1] + 18,
                description);
    }
}


// Actual XTest -> X11 -> GLFW callback -> submitted Tango C11 event.
// This is a live window / server-delivered test; the independent pixel-plan
// assertions above still run before it. An IME preedit test is NOT implied.
struct NativeX11Input final {
    GLFWwindow* window;
    flex::tango_opengl::CellPixelPresenter& presenter;
    flex::tango_opengl::CellHostFrameGate gate;
    int focus_loss_callbacks = 0;
    int focus_gain_callbacks = 0;
    int resize_callbacks = 0;
    std::vector<flex::tango_opengl::CellHostInput> presses;
    std::vector<flex::tango_opengl::CellHostInput> releases;
    std::vector<flex::tango_opengl::CellHostInput> keys;
    std::vector<flex::tango_opengl::CellHostInput> characters;
    std::exception_ptr failure;

    NativeX11Input(GLFWwindow* handle,
                   flex::tango_opengl::CellPixelPresenter& owner)
        : window(handle), presenter(owner), gate(owner) {
        gate.set_focus(glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE);
        glfwSetWindowUserPointer(window, this);
        glfwSetMouseButtonCallback(window, &on_mouse);
        glfwSetKeyCallback(window, &on_key);
        glfwSetCharCallback(window, &on_character);
        glfwSetWindowFocusCallback(window, &on_focus);
        glfwSetWindowSizeCallback(window, &on_window_size);
        glfwSetFramebufferSizeCallback(window, &on_framebuffer_size);
    }
    ~NativeX11Input() {
        glfwSetMouseButtonCallback(window, nullptr);
        glfwSetKeyCallback(window, nullptr);
        glfwSetCharCallback(window, nullptr);
        glfwSetWindowFocusCallback(window, nullptr);
        glfwSetWindowSizeCallback(window, nullptr);
        glfwSetFramebufferSizeCallback(window, nullptr);
        glfwSetWindowUserPointer(window, nullptr);
    }
    NativeX11Input(const NativeX11Input&) = delete;
    NativeX11Input& operator=(const NativeX11Input&) = delete;

    flex::tango_opengl::CellWindowExtents extents() const {
        flex::tango_opengl::CellWindowExtents result{};
        glfwGetWindowSize(window, &result.window_width, &result.window_height);
        glfwGetFramebufferSize(window, &result.framebuffer_width,
                               &result.framebuffer_height);
        return result;
    }

    template <typename F>
    static void guarded(GLFWwindow* handle, F&& fn) noexcept {
        auto* self = static_cast<NativeX11Input*>(
            glfwGetWindowUserPointer(handle));
        if (!self || self->failure) return;
        try {
            fn(*self);
        } catch (...) {
            self->failure = std::current_exception();
        }
    }

    static void on_mouse(GLFWwindow* handle, int button,
                         int action, int mods) noexcept {
        guarded(handle, [&](NativeX11Input& self) {
            if (button != GLFW_MOUSE_BUTTON_LEFT || mods != 0 ||
                (action != GLFW_PRESS && action != GLFW_RELEASE))
                throw std::runtime_error(
                    "Injected mouse button/modifiers are not the expected X11 events");
            double x = 0.0, y = 0.0;
            glfwGetCursorPos(handle, &x, &y);
            const auto hit = self.gate.pointer(
                x, y, self.extents(),
                action == GLFW_PRESS ?
                    flex::tango_opengl::CellMouseAction::Press :
                    flex::tango_opengl::CellMouseAction::Release,
                flex::tango_opengl::CellMouseButton::Left, TUI_MOD_NONE);
            if (!hit) return; // Unfocused or retired frames are never input.
            if (action == GLFW_PRESS)
                self.presses.push_back(*hit);
            else
                self.releases.push_back(*hit);
        });
    }

    static void on_key(GLFWwindow* handle, int key, int,
                       int action, int mods) noexcept {
        guarded(handle, [&](NativeX11Input& self) {
            if (action == GLFW_RELEASE) return;
            if (mods != 0 || action != GLFW_PRESS)
                throw std::runtime_error("XTest sent unexpected keyboard state");
            if (key == GLFW_KEY_LEFT) {
                const auto event = self.gate.key(
                    TUI_KEY_LEFT, TUI_MOD_NONE, self.extents());
                if (event) self.keys.push_back(*event);
            } else if (key != GLFW_KEY_A) {
                throw std::runtime_error("XTest keyboard input was not Left or A");
            }
        });
    }

    static void on_character(GLFWwindow* handle,
                             unsigned int scalar) noexcept {
        guarded(handle, [&](NativeX11Input& self) {
            const auto event = self.gate.character(scalar, self.extents());
            if (event) self.characters.push_back(*event);
        });
    }

    static void on_focus(GLFWwindow* handle, int focused) noexcept {
        guarded(handle, [&](NativeX11Input& self) {
            self.gate.set_focus(focused == GLFW_TRUE);
            if (focused == GLFW_TRUE) ++self.focus_gain_callbacks;
            else ++self.focus_loss_callbacks;
        });
    }

    static void on_window_size(GLFWwindow* handle, int, int) noexcept {
        guarded(handle, [&](NativeX11Input& self) {
            self.gate.invalidate_geometry();
            ++self.resize_callbacks;
        });
    }

    static void on_framebuffer_size(GLFWwindow* handle, int, int) noexcept {
        guarded(handle, [&](NativeX11Input& self) {
            self.gate.invalidate_geometry();
            ++self.resize_callbacks;
        });
    }

    template <typename F>
    void wait_for(F&& condition, const char* error) {
        for (int attempt = 0; attempt != 100; ++attempt) {
            glfwPollEvents(); // OS events, not a direct callback invocation.
            if (failure) std::rethrow_exception(failure);
            if (condition()) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        throw std::runtime_error(error);
    }
};

void exercise_real_x11_input(
    GLFWwindow* window,
    flex::tango_opengl::CellPixelPresenter& presenter,
    const tui_grid_snapshot_t* original_frame,
    flex::tango_opengl::CellPixelMetrics layout,
    const std::string& pixel_artifact) {
    Display* const display = glfwGetX11Display();
    const Window native_window = glfwGetX11Window(window);
    require(display && native_window,
            "TangoGL has no actual X11 GLFW host window");

    int event_base = 0, error_base = 0, major = 0, minor = 0;
    require(XTestQueryExtension(display, &event_base, &error_base,
                                &major, &minor) == True,
            "Xvfb XTest server extension is required; no fake input fallback");
    NativeX11Input events(window, presenter);

    // The original pixel smoke window was hidden to avoid window-system
    // effects during framebuffer readback. Map and focus it explicitly
    // before sending real X-server device events.
    glfwShowWindow(window);
    XMapRaised(display, native_window);
    XSetInputFocus(display, native_window, RevertToParent, CurrentTime);
    XSync(display, False);
    events.wait_for(
        [&] { return glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE &&
                     events.focus_gain_callbacks != 0; },
        "GLFW did not receive real X11 focus callback after window mapping");
    require(!events.gate.key(TUI_KEY_LEFT, TUI_MOD_NONE, events.extents()),
            "X11 initial focus accepted keyboard without a fresh GPU frame");
    presenter.present(original_frame, layout);
    glfwSwapBuffers(window);
    events.gate.accept_presented_frame(events.extents());

    int root_x = 0, root_y = 0;
    Window child = None;
    require(XTranslateCoordinates(display, native_window,
                                  DefaultRootWindow(display), 0, 0,
                                  &root_x, &root_y, &child) == True,
            "Cannot resolve real X11 window position for server injection");

    const auto inject_click = [&](int cell_x, int cell_y) {
        const int px = root_x + cell_x * kCellWidth + 3;
        const int py = root_y + cell_y * kCellHeight + 3;
        require(XTestFakeMotionEvent(
                    display, DefaultScreen(display), px, py, 0) == True,
                "XTest real server mouse motion injection failed");
        XSync(display, False);
        glfwPollEvents();
        const std::size_t previous = events.presses.size();
        require(XTestFakeButtonEvent(display, 1u, True, 0) == True &&
                XTestFakeButtonEvent(display, 1u, False, 0) == True,
                "XTest real server button injection failed");
        XSync(display, False);
        events.wait_for(
            [&] { return events.presses.size() == previous + 1u &&
                         events.releases.size() == previous + 1u; },
            "XTest pointer press/release did not reach actual GLFW callbacks");
        const auto& press = events.presses.back();
        const auto& release = events.releases.back();
        require(press.event.type == TUI_EVENT_MOUSE_PRESS &&
                press.event.key == TUI_KEY_MOUSE_LEFT &&
                release.event.type == TUI_EVENT_MOUSE_RELEASE &&
                release.event.key == TUI_KEY_MOUSE_LEFT &&
                press.event.x == cell_x && press.event.y == cell_y &&
                release.event.x == cell_x && release.event.y == cell_y &&
                press.grid_revision == 101u && release.grid_revision == 101u,
                "XTest GLFW pointer and painted C11 cell disagree");
        std::cout << "TANGO_GL_XTEST_MOUSE cell=" << press.event.x << ','
                  << press.event.y << " revision=" << press.grid_revision
                  << " server=XVFB_XTEST\n";
    };

    inject_click(1, 1); // Actually GPU-painted red A cell.
    inject_click(5, 1); // Actually GPU-painted green B cell.

    const auto inject_key = [&](KeySym symbol) {
        const KeyCode key = XKeysymToKeycode(display, symbol);
        require(key != 0, "X11 keysym has no physical server keycode");
        require(XTestFakeKeyEvent(display, key, True, 0) == True &&
                XTestFakeKeyEvent(display, key, False, 0) == True,
                "XTest key injection could not reach real X11 keyboard");
        XSync(display, False);
    };
    inject_key(XK_Left);
    events.wait_for([&] { return events.keys.size() == 1u; },
                    "Real XTest Left key did not reach GLFW key callback");
    require(events.keys[0].event.type == TUI_EVENT_KEY &&
            events.keys[0].event.key == TUI_KEY_LEFT &&
            events.keys[0].grid_revision == 101u,
            "Real X11 Left key was not delivered as canonical C11 key");

    inject_key(XK_a);
    events.wait_for([&] { return events.characters.size() == 1u; },
                    "Real X11 text key did not reach GLFW committed char callback");
    require(events.characters[0].event.type == TUI_EVENT_KEY &&
            events.characters[0].event.ch == static_cast<std::uint32_t>('a') &&
            events.characters[0].grid_revision == 101u,
            "X11 committed printable key was not the canonical C11 text event");

    // A real injected cell click now drives a new *native Tango C11 source*
    // snapshot and is observed in the REAL GPU framebuffer. Do not merely
    // assert the translated event or paint a synthetic comparison buffer.
    std::unique_ptr<tui_buffer_t, decltype(&tui_buffer_destroy)> source(
        tui_buffer_create(kColumns, kRows), &tui_buffer_destroy);
    require(source != nullptr, "Cannot allocate new Tango C11 reaction frame");
    tui_buffer_clear_color(source.get(), TUI_COLOR(14, 23, 52));
    const auto& selected = events.presses.front().event;
    tui_buffer_set(source.get(), selected.x, selected.y,
                   tui_cell('A', TUI_BLACK, TUI_COLOR(245, 215, 38)));
    // Preserve the second GPU-green cell for a resized visual + hit audit.
    tui_buffer_set(source.get(), 5, 1,
                   tui_cell('B', TUI_WHITE, TUI_COLOR(22, 190, 35)));
    tui_grid_snapshot_t* raw = nullptr;
    require(tui_grid_snapshot_capture(
                source.get(), 102u, kColumns * kRows, 8192, &raw) ==
            TUI_GRID_SNAPSHOT_OK,
            "XTest-driven UI update did not produce a canonical C11 snapshot");
    std::unique_ptr<tui_grid_snapshot_t, decltype(&tui_grid_snapshot_destroy)>
        updated(raw, &tui_grid_snapshot_destroy);
    layout.expected_grid_revision = 102u;
    presenter.present(updated.get(), layout);
    require(presenter.last_submitted_plan() &&
            presenter.last_submitted_plan()->grid_revision() == 102u,
            "Injected click failed to update the submitted Tango frame");
    const auto image = read_back_rgba();
    const Pixel highlight = at(image, selected.x * kCellWidth + 3,
                               selected.y * kCellHeight + 3);
    require(highlight[0] > 175 && highlight[1] > 160 &&
            highlight[2] < 85 && highlight[3] >= 240,
            "XTest clicked cell did not repaint with the real GL highlight");
    save_ppm(image, pixel_artifact);
    glfwSwapBuffers(window);
    events.gate.accept_presented_frame(events.extents());

    // Move X11 focus to the REAL root window. A stale GPU frame must never
    // remain keyboard-active after GLFW receives a server focus-out event.
    XSetInputFocus(display, DefaultRootWindow(display),
                   RevertToParent, CurrentTime);
    XSync(display, False);
    events.wait_for(
        [&] { return glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_FALSE &&
                     events.focus_loss_callbacks != 0; },
        "X11 focus-out event never reached the GLFW host");
    require(!events.gate.key(TUI_KEY_LEFT, TUI_MOD_NONE, events.extents()) &&
            !events.gate.character('a', events.extents()),
            "Old TangoGL GPU frame wrongly admitted input after real X11 focus loss");

    const std::size_t before_blur_keys = events.keys.size();
    inject_key(XK_Left); // Actual server key goes to the focused root, not GUI.
    for (int i = 0; i != 10; ++i) {
        glfwPollEvents();
        if (events.failure) std::rethrow_exception(events.failure);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    require(events.keys.size() == before_blur_keys,
            "Unfocused TangoGL admitted an injected X11 keyboard event");

    // Returning focus cannot reactivate the old painted plan by itself.
    XSetInputFocus(display, native_window, RevertToParent, CurrentTime);
    XSync(display, False);
    const int prior_focus_gains = events.focus_gain_callbacks;
    events.wait_for(
        [&] { return glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE &&
                     events.focus_gain_callbacks > prior_focus_gains; },
        "X11 focus recovery event never reached the GLFW host");
    require(!events.gate.key(TUI_KEY_LEFT, TUI_MOD_NONE, events.extents()),
            "GLFW focus regain wrongly resurrected old TangoGL input admission");
    bool reused_old = false;
    try {
        events.gate.accept_presented_frame(events.extents());
    } catch (const std::logic_error&) {
        reused_old = true;
    }
    require(reused_old, "Focus regain accepted an old completed GPU frame");
    presenter.present(updated.get(), layout);
    glfwSwapBuffers(window);
    events.gate.accept_presented_frame(events.extents());
    inject_key(XK_Left);
    events.wait_for([&] { return events.keys.size() == before_blur_keys + 1u; },
                    "X11 focus recovery failed to restore real C11 keyboard input");
    require(events.keys.back().grid_revision == 102u,
            "X11 refocused keyboard was attached to a stale grid revision");

    // Real X server resize: both logical dimensions and the physical
    // framebuffer change. The old frame is rejected before re-projection.
    XResizeWindow(display, native_window,
                  static_cast<unsigned>(kWidth + 48),
                  static_cast<unsigned>(kHeight + 24));
    XSync(display, False);
    events.wait_for(
        [&] { return events.extents().window_width == kWidth + 48 &&
                     events.extents().window_height == kHeight + 24 &&
                     events.resize_callbacks != 0; },
        "Native X11 two-axis window resize was not observed by GLFW");
    require(!events.gate.key(TUI_KEY_LEFT, TUI_MOD_NONE, events.extents()) &&
            !events.gate.character('a', events.extents()),
            "Unpresented resized X11 viewport wrongly admitted stale input");

    // POSITIVE resize: use the CURRENT native framebuffer metrics and the
    // SAME canonical C11 grid snapshot/renderer. No second layout tree,
    // guessed DPI, terminal-cell pixel equivalence, or synthetic screenshot.
    const auto observed = events.extents();
    const auto scaled = flex::tango_opengl::fit_cell_metrics_to_window(
        updated.get(), observed, layout);
    require(scaled.expected_grid_revision == 102u &&
            scaled.cell_width > layout.cell_width &&
            scaled.cell_height > layout.cell_height,
            "Real host Resize did not reproject the existing C11 cell spans");
    presenter.present(updated.get(), scaled);
    const auto resized_image = read_back_rgba(
        observed.framebuffer_width, observed.framebuffer_height);
    const int new_red_x = static_cast<int>(scaled.cell_width + 3.0f);
    const int new_green_x = static_cast<int>(5.0f * scaled.cell_width + 3.0f);
    const int new_cell_y = static_cast<int>(scaled.cell_height + 3.0f);
    const Pixel new_highlight = at(
        resized_image, new_red_x, new_cell_y,
        observed.framebuffer_width, observed.framebuffer_height);
    const Pixel new_green = at(
        resized_image, new_green_x, new_cell_y,
        observed.framebuffer_width, observed.framebuffer_height);
    require(new_highlight[0] > 175 && new_highlight[1] > 160 &&
            new_highlight[2] < 85 && new_highlight[3] >= 240,
            "Actual resized GL framebuffer did not relocate the yellow C11 cell");
    require_rgb_dominance(
        new_green, 1, "Actual resized GL framebuffer did not relocate green cell");
    save_ppm(resized_image, pixel_artifact + ".resized.ppm",
             observed.framebuffer_width, observed.framebuffer_height);
    glfwSwapBuffers(window);
    events.gate.accept_presented_frame(observed);
    require(events.gate.key(TUI_KEY_LEFT, TUI_MOD_NONE, observed).has_value(),
            "A genuinely resized and submitted GPU frame cannot admit C11 input");

    // A real XTest click must now use the *resized* cell geometry. Checking
    // (5,1) after both axes resize prevents passing by reusing 32x24 geometry.
    int resized_root_x = 0, resized_root_y = 0;
    Window resized_child = None;
    require(XTranslateCoordinates(display, native_window,
                                  DefaultRootWindow(display), 0, 0,
                                  &resized_root_x, &resized_root_y,
                                  &resized_child) == True,
            "X11 failed to report resized native window origin");
    const int new_pointer_x = resized_root_x +
        static_cast<int>(5.0f * scaled.cell_width + scaled.cell_width * 0.5f);
    const int new_pointer_y = resized_root_y +
        static_cast<int>(scaled.cell_height + scaled.cell_height * 0.5f);
    require(XTestFakeMotionEvent(
                display, DefaultScreen(display),
                new_pointer_x, new_pointer_y, 0) == True,
            "XTest could not move to the resized GPU green cell");
    XSync(display, False);
    glfwPollEvents();
    if (events.failure) std::rethrow_exception(events.failure);
    const std::size_t old_presses = events.presses.size();
    const std::size_t old_releases = events.releases.size();
    require(XTestFakeButtonEvent(display, 1u, True, 0) == True &&
            XTestFakeButtonEvent(display, 1u, False, 0) == True,
            "XTest could not inject input into the resized host");
    XSync(display, False);
    events.wait_for(
        [&] { return events.presses.size() == old_presses + 1u &&
                     events.releases.size() == old_releases + 1u; },
        "Native XTest mouse click did not reach the newly resized C11 frame");
    const auto& resized_hit = events.presses.back();
    require(resized_hit.event.x == 5 && resized_hit.event.y == 1 &&
            resized_hit.grid_revision == 102u &&
            events.releases.back().grid_revision == 102u,
            "Resized X11 mouse selected an old physical cell/old frame revision");
    std::cout << "TANGO_GL_XTEST_RESIZE_REPAINT="
              << observed.framebuffer_width << 'x'
              << observed.framebuffer_height << "\n"
              << "TANGO_GL_XTEST_RESIZE_REHIT=5,1;revision=102\n"
              << "TANGO_GL_XTEST_FOCUS_RETIRE=1\n"
              << "TANGO_GL_XTEST_FOCUS_REPRESENT=1\n"
              << "TANGO_GL_XTEST_RESIZE_RETIRE=1\n";

    std::cout << "TANGO_GL_XTEST_KEY=LEFT,A\n"
              << "TANGO_GL_XTEST_FRAME_REVISION=102\n"
              << "TANGO_GL_XTEST_REAL_PIXEL_PASS=1\n";
}


// Controlled native context replacement: release the actual gCanvas GPU
// presenter while the OLD GLFW context is still current; destroy that real
// window/context, then create a DIFFERENT GLFW context and renderer for the
// SAME C11 grid revision. A retired gate is safe even after its GPU presenter
// no longer exists. This is NOT unexpected context loss or fence retirement.

/*
 * Real Mesa GPU shared-atlas mutation barrier.
 *
 * A configured two-fence queue has spare capacity after the first frame,
 * yet a second draw MUST be denied until the first native GLsync retires:
 * the same gCanvas font atlas/texture/shader objects may be modified by
 * subsequent glyph work. After explicit GPU completion a different C11
 * glyph and background must render in the SAME real context. No fake
 * renderer, synthetic frame, direct callback or glFinish-as-fence.
 */
void exercise_native_serial_atlas_frames(
    const flex::tango_opengl::ExternalGLCellCreateInfo& gl,
    flex::tango_opengl::NativeGLFenceConfig config,
    int generation, const std::string& updated_frame_path) {
    require(generation > 0 && glfwGetCurrentContext() == config.host_user_data,
            "Atlas serial acceptance needs the owning live GLFW GL context");
    config.device_generation += 100u;
    config.max_in_flight = 2u; // Spare capacity must NOT admit shared-atlas writes.
    flex::tango_opengl::NativeGLPresentationOwner gpu(gl, config);
    require(gpu.device_generation() == config.device_generation &&
            gpu.in_flight() == 0u,
            "Serial atlas owner did not capture a fresh GL context generation");

    using Frozen = std::unique_ptr<tui_grid_snapshot_t,
                                   decltype(&tui_grid_snapshot_destroy)>;
    const auto capture = [](std::uint64_t revision,
                            std::uint32_t glyph, tui_color_t background) -> Frozen {
        std::unique_ptr<tui_buffer_t, decltype(&tui_buffer_destroy)> source(
            tui_buffer_create(kColumns, kRows), &tui_buffer_destroy);
        require(source != nullptr, "Serial atlas C11 source allocation failed");
        tui_buffer_clear_color(source.get(), TUI_COLOR(14, 23, 52));
        tui_buffer_set(source.get(), 1, 1,
                       tui_cell(glyph, TUI_WHITE, background));
        tui_buffer_set(source.get(), 5, 1,
                       tui_cell('B', TUI_WHITE, TUI_COLOR(22, 190, 35)));
        tui_grid_snapshot_t* result = nullptr;
        require(tui_grid_snapshot_capture(
                    source.get(), revision, kColumns * kRows, 8192, &result) ==
                TUI_GRID_SNAPSHOT_OK,
                "Serial atlas frame was not captured by canonical C11 Tango");
        return Frozen(result, &tui_grid_snapshot_destroy);
    };

    constexpr std::uint64_t first_revision = 403u;
    constexpr std::uint64_t second_revision = 404u;
    auto first = capture(first_revision, 'A', TUI_COLOR(220, 25, 28));
    auto second = capture(second_revision, 'Q', TUI_COLOR(183, 45, 147));
    flex::tango_opengl::CellPixelMetrics metrics;
    metrics.expected_grid_revision = first_revision;
    metrics.font_family = gl.font_family;
    metrics.font_size = 18.0f;
    metrics.cell_width = static_cast<float>(kCellWidth);
    metrics.cell_height = static_cast<float>(kCellHeight);
    metrics.baseline = 18.0f;
    metrics.pixel_ratio = 1.0f;
    metrics.max_cells = kColumns * kRows;
    metrics.max_draw_ops = 128;
    metrics.max_text_bytes = 1024;

    gpu.present_and_fence(first.get(), metrics);
    const auto first_gpu_ids = gpu.gpu_resource_leases();
    require(first_gpu_ids.size() >= 3u && first_gpu_ids.size() <= 64u,
            "Real gCanvas frame returned no bounded native resource identities");
    const auto has_kind = [](const auto& resources,
                             flex::NativeRendererResourceKind kind) {
        return std::any_of(resources.begin(), resources.end(),
                           [&](const auto& resource) { return resource.kind == kind; });
    };
    require(has_kind(first_gpu_ids, flex::NativeRendererResourceKind::ShaderProgram) &&
            has_kind(first_gpu_ids, flex::NativeRendererResourceKind::GlyphAtlas) &&
            has_kind(first_gpu_ids, flex::NativeRendererResourceKind::InternalTexture),
            "Actual GL shader/atlas/internal texture identities were not captured");
    for (const auto& lease : first_gpu_ids)
        require(lease.native_name != 0u &&
                lease.device_generation == config.device_generation &&
                lease.source_revision == first_revision &&
                lease.submission_id == gpu.submitted_presenter().submission_id() &&
                !lease.gpu_completed,
                "Native GL resource ID does not refer to its exact pending GPU frame");
    require(gpu.in_flight() == 1u && config.max_in_flight == 2u,
            "Serial atlas admission did not retain a real GPU fence below capacity");
    const auto old_submission = gpu.submitted_presenter().submission_id();
    metrics.expected_grid_revision = second_revision;

    bool spare_capacity_denied = false;
    try { gpu.require_capacity(); }
    catch (const std::logic_error&) { spare_capacity_denied = true; }
    require(spare_capacity_denied,
            "Shared gCanvas atlas mutation admitted with GPU fence still pending");

    bool second_draw_denied = false;
    try { gpu.present_and_fence(second.get(), metrics); }
    catch (const std::logic_error&) { second_draw_denied = true; }
    require(second_draw_denied && gpu.in_flight() == 1u &&
            gpu.submitted_presenter().submission_id() == old_submission &&
            gpu.submitted_presenter().last_submitted_plan() &&
            gpu.submitted_presenter().last_submitted_plan()->grid_revision() ==
                first_revision,
            "Rejected overlapping atlas update changed actual submitted GPU geometry");

    // A bounded real client GLsync wait, not an implicit glFinish substitute.
    bool first_drained = false;
    for (int attempt = 0; attempt < 20; ++attempt) {
        const auto polled = gpu.retire_completed(100000000ULL);
        if (polled == flex::tango_opengl::NativeGLFencePoll::Drained) {
            first_drained = true;
            break;
        }
        require(polled == flex::tango_opengl::NativeGLFencePoll::Busy &&
                gpu.in_flight() == 1u,
                "Unsignaled atlas GPU fence was retired or overwritten");
    }
    require(first_drained && gpu.in_flight() == 0u &&
            gpu.last_retired_submission() == old_submission,
            "First shared-atlas GPU fence never signaled safely");
    const auto completed_gpu_ids = gpu.gpu_resource_leases();
    require(completed_gpu_ids.size() == first_gpu_ids.size() &&
            std::all_of(completed_gpu_ids.begin(), completed_gpu_ids.end(),
                        [&](const auto& lease) {
                            return lease.gpu_completed &&
                                   lease.submission_id == old_submission;
                        }),
            "Real native shader/atlas/texture leases did not retire with GLsync");
    gpu.require_capacity();
    gpu.present_and_fence(second.get(), metrics);
    const auto second_gpu_ids = gpu.gpu_resource_leases();
    require(second_gpu_ids.size() == first_gpu_ids.size() &&
            std::all_of(second_gpu_ids.begin(), second_gpu_ids.end(),
                [&](const auto& lease) {
                    return !lease.gpu_completed &&
                           lease.submission_id == old_submission + 1u &&
                           lease.source_revision == second_revision &&
                           lease.device_generation == config.device_generation;
                }),
            "Native resource IDs were not re-leased on the second real GPU submission");
    // A registered gCanvas font atlas/shader is the SAME actual native
    // object across these two serial frames, not a simulated new resource.
    for (std::size_t i = 0; i < first_gpu_ids.size(); ++i)
        require(first_gpu_ids[i].kind == second_gpu_ids[i].kind &&
                first_gpu_ids[i].native_name == second_gpu_ids[i].native_name,
                "Actual gCanvas shader/atlas object identity changed across one renderer");
    require(gpu.in_flight() == 1u &&
            gpu.submitted_presenter().submission_id() == old_submission + 1u &&
            gpu.submitted_presenter().last_submitted_plan() &&
            gpu.submitted_presenter().last_submitted_plan()->grid_revision() ==
                second_revision,
            "Completed prior native fence did not admit the next canonical C11 frame");

    // Check actual gCanvas GPU framebuffer content AFTER the new ASCII glyph
    // and cell background are drawn. The new Q did not exist in frame one.
    const auto pixels = read_back_rgba();
    const Pixel new_background = at(pixels, kCellWidth + 2, kCellHeight + 2);
    require(new_background[0] > 130 && new_background[0] < 220 &&
            new_background[1] < 105 && new_background[2] > 95 &&
            new_background[3] >= 240,
            "Serial gCanvas atlas frame did not replace real red GPU pixels");
    require_rgb_dominance(
        at(pixels, 5 * kCellWidth + 2, kCellHeight + 2), 1,
        "Serial atlas repaint lost the existing real green GPU cell");
    require(has_white_glyph(
                pixels, kCellWidth, kCellHeight,
                2 * kCellWidth, 2 * kCellHeight),
            "Serial atlas update did not rasterize its new native Q glyph");
    if (generation == 2)
        save_ppm(pixels, updated_frame_path);

    bool second_drained = false;
    for (int attempt = 0; attempt < 20; ++attempt) {
        const auto polled = gpu.retire_completed(100000000ULL);
        if (polled == flex::tango_opengl::NativeGLFencePoll::Drained) {
            second_drained = true;
            break;
        }
        require(polled == flex::tango_opengl::NativeGLFencePoll::Busy &&
                gpu.in_flight() == 1u,
                "Second atlas GPU fence was dropped without a real signal");
    }
    require(second_drained && gpu.in_flight() == 0u,
            "Final shared-atlas GPU commands were not completed");
    const auto final_gpu_ids = gpu.gpu_resource_leases();
    require(std::all_of(final_gpu_ids.begin(), final_gpu_ids.end(),
                        [](const auto& lease) { return lease.gpu_completed; }),
            "Last native resource lease was not retired with the second real GLsync");
    gpu.close(); // Only after real GPU completion, original context current.
    require(gpu.closed(),
            "Native gCanvas atlas owner failed to retire after both fences");
}

void exercise_orderly_context_recreation(
    const std::string& regular_font, const std::string& bold_font,
    const std::string& screenshot) {
    GlfwSession glfw;
    std::shared_ptr<flex::tango_opengl::CellHostFrameGate> old_gate;
    Window first_x11_window = None;
    constexpr std::uint64_t kSameRevision = 303u;

    for (int generation = 1; generation <= 2; ++generation) {
        {
            GlfwWindow native;
            require(native.handle != nullptr &&
                    glfwGetCurrentContext() == native.handle,
                    "New TangoGL native context was not created and made current");
            const Window x11_window = glfwGetX11Window(native.handle);
            require(x11_window != None, "New GL window lacks its own real X11 handle");
            if (generation == 1)
                first_x11_window = x11_window;
            else
                require(first_x11_window != x11_window,
                        "Recreated TangoGL is reusing the old live native X11 identity");

            auto source = std::unique_ptr<tui_buffer_t,
                decltype(&tui_buffer_destroy)>(
                    tui_buffer_create(kColumns, kRows), &tui_buffer_destroy);
            require(source != nullptr,
                    "Controlled context qualification failed to allocate Tango C11 grid");
            tui_buffer_clear_color(source.get(), TUI_COLOR(14, 23, 52));
            const tui_color_t red_or_yellow = generation == 1 ?
                TUI_COLOR(220, 25, 28) : TUI_COLOR(245, 215, 38);
            tui_buffer_set(source.get(), 1, 1,
                           tui_cell('A', TUI_BLACK, red_or_yellow));
            tui_buffer_set(source.get(), 5, 1,
                           tui_cell('B', TUI_WHITE, TUI_COLOR(22, 190, 35)));
            tui_grid_snapshot_t* captured = nullptr;
            require(tui_grid_snapshot_capture(
                        source.get(), kSameRevision,
                        kColumns * kRows, 8192, &captured) ==
                    TUI_GRID_SNAPSHOT_OK,
                    "Fresh native GL context did not use a valid C11 snapshot");
            auto grid = std::unique_ptr<tui_grid_snapshot_t,
                decltype(&tui_grid_snapshot_destroy)>(
                    captured, &tui_grid_snapshot_destroy);
            source.reset(); // The real frozen source snapshot owns all its bytes.

            flex::tango_opengl::ExternalGLCellCreateInfo gl;
            gl.get_proc_address = &gl_proc;
            gl.font_family = "tango-pixel-test-mono";
            gl.font_path = regular_font;
            gl.bold_font_path = bold_font;
            // Exact native context and independent device generation. Reuse
            // the current host GL loader; no global/secondary GL backend.
            flex::tango_opengl::NativeGLFenceConfig fence_config{};
            fence_config.get_proc_address = &gl_proc;
            fence_config.is_context_current = &exact_gl_context_is_current;
            fence_config.host_user_data = native.handle;
            fence_config.device_generation =
                static_cast<std::uint64_t>(generation);
            fence_config.max_in_flight = 1u;
            // One combined owner holds the actual gCanvas glyph/shader
            // assets, GPU fences and the original native input gate.
            flex::tango_opengl::NativeGLPresentationOwner gpu(gl, fence_config);
            require(gpu.submitted_presenter().on_owner_thread() &&
                    gpu.device_generation() ==
                        static_cast<std::uint64_t>(generation),
                    "Native GPU owner did not bind the exact GLFW GL device");
            auto gate = gpu.input_gate();
            gate->set_focus(true); // Separate XTest test qualifies real OS focus.
            const flex::tango_opengl::CellWindowExtents observed{
                kWidth, kHeight, kWidth, kHeight};
            require(!gate->key(TUI_KEY_LEFT, TUI_MOD_NONE, observed),
                    "Fresh GL context admitted stale/unpainted native keyboard input");
            if (old_gate)
                require(!old_gate->key(TUI_KEY_LEFT, TUI_MOD_NONE, observed),
                        "Old retired input gate was resurrected by a new GL context");

            flex::tango_opengl::CellPixelMetrics layout;
            layout.expected_grid_revision = kSameRevision;
            layout.font_family = gl.font_family;
            layout.font_size = 18.0f;
            layout.cell_width = static_cast<float>(kCellWidth);
            layout.cell_height = static_cast<float>(kCellHeight);
            layout.baseline = 18.0f;
            layout.pixel_ratio = 1.0f;
            layout.max_cells = kColumns * kRows;
            layout.max_draw_ops = 128;
            layout.max_text_bytes = 1024;
            gpu.require_capacity();
            // Caller cannot obtain mutable presenter/fences from the owner.
            // All native GPU draws must insert a real GLsync atomically.
            gpu.present_and_fence(grid.get(), layout);
            require(gpu.in_flight() == 1u,
                    "Actual GL fence was not retained for the submitted frame");

            bool saturated = false;
            try { gpu.require_capacity(); }
            catch (const std::length_error&) { saturated = true; }
            require(saturated,
                    "TangoGL allowed GPU frame submission beyond its fence budget");

            bool early_close = false;
            try { gpu.close(); }
            catch (const std::logic_error&) { early_close = true; }
            require(early_close,
                    "TangoGL retired GPU resources while a real fence was outstanding");

            // Deliberately switch AWAY from the owner context. No OpenGL
            // function may be called in this state, even on the same thread.
            glfwMakeContextCurrent(nullptr);
            bool wrong_context_denied = false;
            try { (void)gpu.retire_completed(0); }
            catch (const std::logic_error&) { wrong_context_denied = true; }
            glfwMakeContextCurrent(native.handle);
            require(wrong_context_denied,
                    "GLsync retirement incorrectly accepted a foreign/absent GL context");

            bool wrong_thread_denied = false;
            std::thread foreign([&] {
                try { (void)gpu.retire_completed(0); }
                catch (const std::logic_error&) { wrong_thread_denied = true; }
            });
            foreign.join();
            require(wrong_thread_denied,
                    "GLsync was accessible outside its GL/UI owner thread");

            // Each actual GL_CLIENT_WAIT is bounded to 100ms, with at most
            // 20 attempts. Timeout retains the GLsync; only real native
            // GL_ALREADY_SIGNALED/GL_CONDITION_SATISFIED may retire it.
            bool completed = false;
            for (int attempt = 0; attempt != 20; ++attempt) {
                const auto outcome = gpu.retire_completed(100000000ULL);
                if (outcome == flex::tango_opengl::NativeGLFencePoll::Drained) {
                    completed = true;
                    break;
                }
                require(outcome == flex::tango_opengl::NativeGLFencePoll::Busy,
                        "GPU fence poll returned an unknown completion result");
                require(gpu.in_flight() == 1u,
                        "An unsignaled native GL fence was discarded prematurely");
            }
            require(completed && gpu.in_flight() == 0u &&
                    gpu.last_retired_submission() ==
                    gpu.submitted_presenter().submission_id(),
                    "Native GPU fence did not signal and retire before resource teardown");
            // Even with GPU work finished, the owner has NOT destroyed the
            // GL renderer/atlas until an explicit context-owned close().
            require(!gpu.closed() &&
                    gpu.last_retired_submission() != 0u,
                    "Native GPU owner retired before its context cleanup");

            // Even after a signaled GPU fence, the real gCanvas renderer
            // may only be destroyed against the ORIGINAL native GL context.
            glfwMakeContextCurrent(nullptr);
            bool wrong_context_close_denied = false;
            try { gpu.close(); }
            catch (const std::logic_error&) {
                wrong_context_close_denied = true;
            }
            glfwMakeContextCurrent(native.handle);
            require(wrong_context_close_denied && !gpu.closed(),
                    "Native owner destroyed GPU resources without its GL context");

            // Prove native GPU bytes come from a NEW gCanvas renderer/context,
            // not an old framebuffer or a synthesized CPU test image.
            const auto rgba = read_back_rgba();
            const Pixel painted = at(rgba, kCellWidth + 2, kCellHeight + 2);
            const Pixel green = at(rgba, 5 * kCellWidth + 2, kCellHeight + 2);
            if (generation == 1) {
                require_rgb_dominance(painted, 0,
                    "Old real OpenGL context failed to draw its own red pixel");
            } else {
                require(painted[0] > 175 && painted[1] > 160 &&
                        painted[2] < 85 && painted[3] >= 240,
                        "New real GL context retained a stale red frame instead of yellow");
                save_ppm(rgba, screenshot); // Actual second-context pixels.
            }
            require_rgb_dominance(green, 1,
                "New actual GL context failed to create and rasterize green cell");
            glfwSwapBuffers(native.handle);
            gate->accept_presented_frame(observed);
            const auto new_hit = gate->pointer(
                5.0 * kCellWidth + 2.0, 1.0 * kCellHeight + 2.0,
                observed, flex::tango_opengl::CellMouseAction::Press,
                flex::tango_opengl::CellMouseButton::Left, TUI_MOD_NONE);
            require(new_hit && new_hit->event.x == 5 &&
                    new_hit->event.y == 1 &&
                    new_hit->grid_revision == kSameRevision,
                    "Replacement GL context cannot publish its own valid C11 cell hit");

            // glReadPixels above blocks until this submitted frame completes.
            // Do not release GPU resources after destroying the host context.
            require(glfwGetCurrentContext() == native.handle,
                    "GPU resources would retire without their owning GL context");
            // Only this close may actually free the gCanvas renderer and
            // its glyph atlas, after ALL native GLsyncs are complete.
            gpu.close();
            require(gpu.closed(),
                    "Combined owner failed to retire its actual gCanvas renderer");
            bool rejected_reuse = false;
            try { gpu.require_capacity(); }
            catch (const std::logic_error&) { rejected_reuse = true; }
            require(rejected_reuse,
                    "Retired combined GPU owner accepted another submission");
            require(!gate->key(TUI_KEY_LEFT, TUI_MOD_NONE, observed) &&
                    !gate->character('a', observed),
                    "Destroyed GL presenter left input admitted on a stale gate");
            bool rejected_reactivation = false;
            gate->set_focus(true);
            try { gate->accept_presented_frame(observed); }
            catch (const std::logic_error&) { rejected_reactivation = true; }
            require(rejected_reactivation,
                    "Retired host gate was illegally reattached to another GL device");
            if (generation == 1) old_gate = gate;
            // A second native renderer on the SAME live context proves
            // strict atlas mutation gating even with spare fence capacity.
            // It is created only AFTER the previous owner closed.
            exercise_native_serial_atlas_frames(
                gl, fence_config, generation, screenshot + ".serial.ppm");
        } // Destroy the actual GLFW GL context AFTER GPU resource cleanup.
        require(glfwGetCurrentContext() == nullptr,
                "Destroyed native GL context remains bound to the UI owner");
    }
    require(old_gate && !old_gate->character('x', {kWidth, kHeight,
                                                 kWidth, kHeight}),
            "Old context gate accessed GPU resources after the new context shutdown");
    std::cout << "TANGO_GL_CONTEXT_RECREATED=2\n"
              << "TANGO_GL_CONTEXT_OLD_GATE_RETIRED=1\n"
              << "TANGO_GL_CONTEXT_NEW_PIXEL_PASS=1\n"
              << "TANGO_GL_FENCE_NATIVE_GPU_COMPLETION=2\n"
              << "TANGO_GL_FENCE_OVERLOAD_REFUSED=1\n"
              << "TANGO_GL_FENCE_WRONG_CONTEXT_REFUSED=1\n"
              << "TANGO_GL_FENCE_WRONG_THREAD_REFUSED=1\n"
              << "TANGO_GL_FENCED_RESOURCE_OWNER_CLOSE=2\n"
              << "TANGO_GL_FENCED_RESOURCE_OWNER_EARLY_CLOSE_REFUSED=2\n"
              << "TANGO_GL_FENCED_RESOURCE_OWNER_WRONG_CONTEXT_CLOSE_REFUSED=2\n"
              << "TANGO_GL_ATLAS_SERIAL_PENDING_REFUSED=2\n"
              << "TANGO_GL_ATLAS_SERIAL_AFTER_FENCE_REPAINT=2\n"
              << "TANGO_GL_NATIVE_SHADER_ATLAS_TEXTURE_LEASES=2\n"
              << "TANGO_GL_NATIVE_RESOURCE_LEASE_REUSED_AFTER_FENCE=2\n";
}

void exercise_gpu(const std::string& font_path,
                  const std::string& bold_font_path,
                  const std::string& ppm) {
    GlfwSession glfw;
    GlfwWindow window;
    OwnedGrid tango;

    flex::tango_opengl::ExternalGLCellCreateInfo gl;
    gl.get_proc_address = &gl_proc;
    gl.font_family = "tango-pixel-test-mono";
    gl.font_path = font_path;
    gl.bold_font_path = bold_font_path;

    // Destroy the real gCanvas OpenGL resources before the GLFW context dies.
    auto presenter = flex::tango_opengl::create_external_gl_presenter(gl);
    require(presenter && presenter->on_owner_thread(),
            "OpenGL presenter is not owned by the current GUI thread");

    flex::tango_opengl::CellPixelMetrics layout;
    layout.expected_grid_revision = 101;
    layout.font_family = gl.font_family;
    layout.font_size = 18.0f;
    layout.cell_width = static_cast<float>(kCellWidth);
    layout.cell_height = static_cast<float>(kCellHeight);
    layout.baseline = 18.0f;
    layout.pixel_ratio = 1.0f;
    layout.max_cells = kColumns * kRows;
    layout.max_draw_ops = 128;
    layout.max_text_bytes = 1024;

    presenter->present(tango.frame, layout);
    const auto* submitted = presenter->last_submitted_plan();
    require(submitted != nullptr, "TangoGL did not retain its submitted pixel/hit plan");
    require(submitted->grid_revision() == tui_grid_snapshot_revision(tango.frame),
            "TangoGL displayed frame and hit geometry have different revisions");

    int window_width = 0, window_height = 0;
    int framebuffer_width = 0, framebuffer_height = 0;
    glfwGetWindowSize(window.handle, &window_width, &window_height);
    glfwGetFramebufferSize(window.handle, &framebuffer_width, &framebuffer_height);
    require(framebuffer_width == kWidth && framebuffer_height == kHeight,
            "TangoGL hit-test framebuffer extents differ from the actual GL pixels");
    require(window_width > 0 && window_height > 0,
            "TangoGL GLFW host returned an empty window");
    const auto pick_physical_point = [&](int pixel_x, int pixel_y) {
        return presenter->hit_test_window_pointer(
            static_cast<double>(pixel_x) * window_width / framebuffer_width,
            static_cast<double>(pixel_y) * window_height / framebuffer_height,
            window_width, window_height, framebuffer_width, framebuffer_height);
    };
    const auto red_hit = pick_physical_point(
        kCellWidth + 2, kCellHeight + 2);
    const auto green_hit = pick_physical_point(
        5 * kCellWidth + 2, kCellHeight + 2);
    const auto bg_hit = pick_physical_point(4, 4);
    require(red_hit && red_hit->column == 1 && red_hit->row == 1 &&
            red_hit->leading_column == 1,
            "Red framebuffer cell does not match the shared TangoGL hit plan");
    require(green_hit && green_hit->column == 5 && green_hit->row == 1 &&
            green_hit->leading_column == 5,
            "Green framebuffer cell does not match the shared TangoGL hit plan");
    require(bg_hit && bg_hit->column == 0 && bg_hit->row == 0,
            "Background framebuffer cell does not match its hit position");
    require(!pick_physical_point(kWidth, 0),
            "Outside-right framebuffer pointer wrongly selected a Tango cell");

    bool extent_rejected = false;
    try {
        (void)flex::tango_opengl::hit_test_window_pointer(
            *submitted, 10.0, 10.0, window_width, window_height,
            framebuffer_width + 1, framebuffer_height);
    } catch (const std::logic_error&) {
        extent_rejected = true;
    }
    require(extent_rejected,
            "Mismatched live framebuffer extents were silently interpreted as cells");

    // The same native GLFW-observed geometry now yields a *C11 tui_event_t*
    // rather than an independent GLFW cell model. These calls qualify the
    // adapter against real Mesa pixels; OS-injected events remain a later gate.
    const flex::tango_opengl::CellWindowExtents host_extents{
        window_width, window_height, framebuffer_width, framebuffer_height};
    const auto mouse = flex::tango_opengl::cell_input_pointer(
        *presenter,
        static_cast<double>(kCellWidth + 2) * window_width / framebuffer_width,
        static_cast<double>(kCellHeight + 2) * window_height / framebuffer_height,
        host_extents, flex::tango_opengl::CellMouseAction::Press,
        flex::tango_opengl::CellMouseButton::Left, TUI_MOD_NONE);
    require(mouse && mouse->event.type == TUI_EVENT_MOUSE_PRESS &&
            mouse->event.key == TUI_KEY_MOUSE_LEFT &&
            mouse->event.x == red_hit->column &&
            mouse->event.y == red_hit->row &&
            mouse->grid_revision == red_hit->grid_revision,
            "TangoGL C11 mouse event is not based on the real painted red cell");
    const auto key = flex::tango_opengl::cell_input_key(
        *presenter, TUI_KEY_ESCAPE, TUI_MOD_NONE);
    require(key && key->event.type == TUI_EVENT_KEY &&
            key->event.key == TUI_KEY_ESCAPE &&
            key->grid_revision == submitted->grid_revision(),
            "TangoGL native key event is not attached to the submitted revision");
    const auto cjk_text = flex::tango_opengl::cell_input_character(
        *presenter, 0x4e2du);
    require(cjk_text && cjk_text->event.type == TUI_EVENT_KEY &&
            cjk_text->event.ch == 0x4e2du && cjk_text->grid_revision == 101u,
            "TangoGL native committed Unicode does not use the C11 key event");

    const auto first_frame = read_back_rgba();
    // Preserve the actual GPU pixels for audit EVEN when a subsequent color
    // or glyph assertion fails; never synthesize a fallback screenshot.
    save_ppm(first_frame, ppm);
    const Pixel background = at(first_frame, 4, 4);
    const Pixel red_cell = at(first_frame, kCellWidth + 2, kCellHeight + 2);
    const Pixel green_cell = at(first_frame, 5 * kCellWidth + 2,
                                kCellHeight + 2);
    std::cout << "TANGO_GL_BG="
              << unsigned(background[0]) << ','
              << unsigned(background[1]) << ','
              << unsigned(background[2]) << '\n';
    std::cout << "TANGO_GL_RED="
              << unsigned(red_cell[0]) << ','
              << unsigned(red_cell[1]) << ','
              << unsigned(red_cell[2]) << '\n';
    std::cout << "TANGO_GL_GREEN="
              << unsigned(green_cell[0]) << ','
              << unsigned(green_cell[1]) << ','
              << unsigned(green_cell[2]) << '\n';
    require_rgb_dominance(background, 2,
                          "Tango OpenGL background is not blue-dominant");
    require_rgb_dominance(red_cell, 0,
                          "Tango OpenGL red cell was not rasterized");
    require_rgb_dominance(green_cell, 1,
                          "Tango OpenGL green cell was not rasterized");
    require(has_white_glyph(first_frame, kCellWidth, kCellHeight,
                            2 * kCellWidth, 2 * kCellHeight),
            "Tango OpenGL failed to rasterize an actual font glyph");

    // Reject a stale grid BEFORE issuing any GL work, without changing the
    // last completed framebuffer; no implicit TTY or generic GL fallback.
    ++layout.expected_grid_revision;
    bool rejected = false;
    try {
        presenter->present(tango.frame, layout);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    require(rejected, "Tango OpenGL stale cell revision was silently accepted");
    require(presenter->last_submitted_plan() == submitted,
            "Pre-submit rejection unexpectedly replaced the last valid input/hit plan");
    const auto post_rejection_hit = pick_physical_point(
        kCellWidth + 2, kCellHeight + 2);
    require(post_rejection_hit && post_rejection_hit->column == red_hit->column &&
            post_rejection_hit->grid_revision == red_hit->grid_revision,
            "Pre-submit rejection changed the last actually submitted hit geometry");
    require(read_back_rgba() == first_frame,
            "Rejected Tango grid mutation changed an already drawn GL frame");

    // The original pixel contract passed. Now exercise actual X11 server
    // input delivered through GLFW, and record the interactive GPU frame.
    layout.expected_grid_revision = tui_grid_snapshot_revision(tango.frame);
    exercise_real_x11_input(window.handle, *presenter, tango.frame, layout,
                            ppm + ".input.ppm");
    std::cout << "TANGO_GL_PIXEL_PASS=1\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: tango_gl_real_pixel_smoke <regular.ttf> <bold.ttf> <out.ppm>\n";
        return 2;
    }
    try {
        exercise_gpu(argv[1], argv[2], argv[3]);
        exercise_orderly_context_recreation(
            argv[1], argv[2], std::string(argv[3]) + ".recreated.ppm");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TANGO_GL_PIXEL_FAIL=" << error.what() << '\n';
        return 1;
    }
}
