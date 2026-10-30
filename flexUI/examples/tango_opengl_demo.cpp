/*
 * Explicit Tango OpenGL cell-grid host.
 *
 * Unlike the native TTY sample, this owns a GLFW context and asks gCanvas GL
 * to draw a frozen Tango C11 grid. No CSS, alternate Unicode parser, or TTY
 * fallback. The GL presenter/window is deliberately kept separate from
 * RendererBackend::OpenGL until full product/pixel/retirement acceptance.
 */
#include <backends/tango_opengl/gl_presenter.h>
#include <tui_grid_snapshot.h>
#include <GLFW/glfw3.h>

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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
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
        TangoSnapshot frame(grid.buffer, 1);

        CellPixelMetrics layout;
        layout.cell_width = 16.0f;
        layout.cell_height = 24.0f;
        layout.font_size = 15.0f;
        layout.baseline = 18.0f;
        layout.pixel_ratio = 1.0f;
        layout.font_family = gl.font_family;
        layout.expected_grid_revision = 1;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            presenter->present(frame.snapshot, layout);
            glfwSwapBuffers(window);
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
