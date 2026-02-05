/*
 * Flex Engine - GLFW Application Template
 *
 * Simple base class for creating ThorVG + OpenGL demos with GLFW3.
 */

#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <thorvg.h>
#include <flex/bridge/renderer.h>
#include <iostream>
#include <fstream>
#include <vector>
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
        glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);

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

    // Accessors
    Renderer* renderer() { return renderer_.get(); }
    tvg::GlCanvas* canvas() { return canvas_.get(); }
    GLFWwindow* window() { return window_; }
    int width() const { return width_; }
    int height() const { return height_; }

protected:
    virtual bool on_init() { return true; }
    virtual void on_update(float dt) {}
    virtual void on_render() {}
    virtual void on_key(int key, int action, int mods) {}
    virtual void on_mouse_button(int button, int action, int mods) {}
    virtual void on_cursor_pos(double x, double y) {}
    virtual void on_scroll(double dx, double dy) {}
    virtual void on_resize(int w, int h) {
        width_ = w;
        height_ = h;
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
        static_cast<GlfwApp*>(glfwGetWindowUserPointer(w))->on_cursor_pos(x, y);
    }

    static void scroll_callback(GLFWwindow* w, double dx, double dy) {
        static_cast<GlfwApp*>(glfwGetWindowUserPointer(w))->on_scroll(dx, dy);
    }

    static void framebuffer_size_callback(GLFWwindow* w, int width, int height) {
        static_cast<GlfwApp*>(glfwGetWindowUserPointer(w))->on_resize(width, height);
    }

    const char* title_;
    int width_, height_;
    GLFWwindow* window_ = nullptr;
    std::unique_ptr<tvg::GlCanvas> canvas_;
    std::unique_ptr<Renderer> renderer_;
};

} // namespace flex
