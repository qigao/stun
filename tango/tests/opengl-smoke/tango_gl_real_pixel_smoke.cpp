/*
 * Real desktop OpenGL/Mesa cell-grid pixel acceptance.
 *
 * The test uses the ACTUAL production Tango C11 snapshot, gCanvas shader
 * renderer and a real GLFW GL4.1 framebuffer. No fake renderer, no CSS,
 * no TTY fallback, and no CPU-generated replacement image.
 */
#include <backends/tango_opengl/gl_presenter.h>
#include <tui_grid_snapshot.h>
#include <GLFW/glfw3.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
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
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
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

std::vector<std::uint8_t> read_back_rgba() {
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(kWidth) * kHeight * 4u);
    glFinish();
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels.data());
    require(glGetError() == GL_NO_ERROR,
            "Native glReadPixels failed; GPU framebuffer cannot be verified");
    return pixels;
}

Pixel at(const std::vector<std::uint8_t>& image, int x, int top_y) {
    require(x >= 0 && x < kWidth && top_y >= 0 && top_y < kHeight,
            "Invalid Tango GPU pixel sampling coordinates");
    const auto offset =
        static_cast<std::size_t>(((kHeight - 1 - top_y) * kWidth + x) * 4);
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
              const std::string& destination) {
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    require(output.is_open(), "Cannot create exact-head Tango GL pixel artifact");
    output << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const Pixel p = at(rgba, x, y);
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
    require(read_back_rgba() == first_frame,
            "Rejected Tango grid mutation changed an already drawn GL frame");

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
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TANGO_GL_PIXEL_FAIL=" << error.what() << '\n';
        return 1;
    }
}
