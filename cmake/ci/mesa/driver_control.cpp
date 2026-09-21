#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <future>
#include <stdexcept>
#include <string>

namespace {
constexpr int extent = 64;
constexpr int worker_frames = 4;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void glfw_error(int code, const char* message) {
    std::fprintf(stderr, "GLFW error %d: %s\n", code, message);
}
struct Session {
    std::array<GLFWwindow*, 2> windows{};
    Session() { require(glfwInit() == GLFW_TRUE, "GLFW initialization failed"); }
    ~Session() {
        glfwMakeContextCurrent(nullptr);
        for (auto* window : windows) if (window) glfwDestroyWindow(window);
        glfwTerminate();
    }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    GLFWwindow* create(std::size_t slot, GLFWwindow* share = nullptr) {
        require(!windows.at(slot), "Window slot already owned");
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_STENCIL_BITS, 8);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        auto* window = glfwCreateWindow(extent, extent, "mesa-lifecycle", nullptr, share);
        require(window != nullptr, "Control window creation failed");
        windows[slot] = window;
        glfwMakeContextCurrent(window);
        glfwSwapInterval(0);
        return window;
    }
    void close(std::size_t slot) {
        glfwMakeContextCurrent(nullptr);
        glfwDestroyWindow(windows.at(slot));
        windows[slot] = nullptr;
    }
};
struct Objects {
    GLuint vertex = 0, fragment = 0, program = 0, vao = 0;
    ~Objects() {
        glUseProgram(0);
        glBindVertexArray(0);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (program) glDeleteProgram(program);
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
    }
};
void compile(GLuint shader, const char* source) {
    require(shader != 0, "Shader allocation failed");
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    require(success == GL_TRUE, "Shader compilation failed");
}
void load_gl() {
    require(gladLoadGL(glfwGetProcAddress) != 0,
            "OpenGL loader failed");
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    require(renderer && std::strstr(renderer, "llvmpipe"), "Expected explicit llvmpipe driver");
    std::printf("renderer=%s\nversion=%s\n", renderer, glGetString(GL_VERSION));
}
void draw_and_verify() {
    Objects objects;
    objects.vertex = glCreateShader(GL_VERTEX_SHADER);
    objects.fragment = glCreateShader(GL_FRAGMENT_SHADER);
    compile(objects.vertex,
        "#version 410 core\n"
        "const vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));\n"
        "void main(){gl_Position=vec4(p[gl_VertexID],0,1);}\n");
    compile(objects.fragment,
        "#version 410 core\nout vec4 color;\n"
        "void main(){color=vec4(1,0,0,1);}\n");
    objects.program = glCreateProgram();
    require(objects.program != 0, "Program allocation failed");
    glAttachShader(objects.program, objects.vertex);
    glAttachShader(objects.program, objects.fragment);
    glLinkProgram(objects.program);
    GLint linked = GL_FALSE;
    glGetProgramiv(objects.program, GL_LINK_STATUS, &linked);
    require(linked == GL_TRUE, "Program link failed");
    glGenVertexArrays(1, &objects.vao);
    glBindVertexArray(objects.vao);
    glUseProgram(objects.program);
    glViewport(0, 0, extent, extent);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    unsigned char pixel[4] = {};
    glReadPixels(extent / 2, extent / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    require(glGetError() == GL_NO_ERROR, "OpenGL command failed");
    require(pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 255,
            "Control pixel readback failed");
    glFinish();
    std::puts("pixel-readback=passed");
}
void verify_module(bool loaded) {
    const char* expected = std::getenv("MESA_EXPECTED_DRIVER");
    const char* expected_glx = std::getenv("MESA_EXPECTED_GLX");
    require(expected && *expected && expected_glx && *expected_glx,
            "Both installed Mesa provider paths are required");
    std::ifstream maps("/proc/self/maps");
    require(maps.is_open(), "Cannot inspect driver mappings");
    bool found = false;
    bool found_glx = false;
    for (std::string line; std::getline(maps, line);) {
        const auto path_start = line.find('/');
        if (path_start == std::string::npos) continue;
        const std::string mapped_path = line.substr(path_start);
        if (line.find("libgallium") != std::string::npos) {
            require(mapped_path == expected, "Unexpected Gallium provider loaded");
            found = true;
        } else if (line.find("libGLX_mesa") != std::string::npos) {
            require(mapped_path == expected_glx, "Unexpected GLX provider loaded");
            found_glx = true;
        } else {
            continue;
        }
        std::printf("driver-map=%s\n", line.c_str());
    }
    require(!maps.bad(), "Cannot finish reading driver mappings");
    require(found == loaded && found_glx == loaded, "GLX/Gallium load/unload contract failed");
    std::printf("driver-module=%s\n", loaded ? "loaded" : "unloaded");
}
void exercise(Session& session, const std::string& mode) {
    if (mode == "init") return;
    auto* first = session.create(0);
    if (mode == "window") return;
    load_gl(); // GLAD's process-global function table is initialized before workers.
    if (mode == "draw") {
        draw_and_verify();
        return;
    }
    if (mode == "overlap") {
        draw_and_verify();
        auto* second = session.create(1, first);
        draw_and_verify();
        session.close(0);
        glfwMakeContextCurrent(second);
        draw_and_verify(); // The shared module must survive the first context's destruction.
        verify_module(true);
        first = session.create(0, second);
        session.close(1);
        glfwMakeContextCurrent(first);
        draw_and_verify();
        return;
    }
    auto* second = session.create(1);
    glfwMakeContextCurrent(nullptr);
    auto worker = [](GLFWwindow* window) {
        glfwMakeContextCurrent(window);
        try {
            for (int frame = 0; frame < worker_frames; ++frame) draw_and_verify();
        } catch (...) {
            glfwMakeContextCurrent(nullptr);
            throw;
        }
        glfwMakeContextCurrent(nullptr);
    };
    // Future destructors join even when construction/get of another future throws.
    auto one = std::async(std::launch::async, worker, first);
    auto two = std::async(std::launch::async, worker, second);
    one.get();
    two.get();
    std::puts("workers=joined");
}
}
int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 3) return 2;
    const std::string mode = argv[1];
    if ((mode != "init" && mode != "window" && mode != "draw" && mode != "overlap" && mode != "threads") ||
        (std::strcmp(argv[2], "1") && std::strcmp(argv[2], "4"))) return 2;
    glfwSetErrorCallback(glfw_error);
    try {
        const int cycles = argv[2][0] - '0';
        for (int cycle = 0; cycle < cycles; ++cycle) {
            {
                Session session;
                exercise(session, mode);
                verify_module(mode != "init");
            }
            verify_module(false);
            std::printf("cycle=%d teardown=completed\n", cycle);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Control failure: %s\n", error.what());
        return 1;
    }
    std::puts("application-checks=passed; process-exit-leak-check-still-required");
    return 0;
}
