#ifndef GCANVAS_NATIVE_WINDOW_CALLBACKS_HPP
#define GCANVAS_NATIVE_WINDOW_CALLBACKS_HPP

struct GLFWwindow;

namespace gcanvas
{
    void on_window_resize(GLFWwindow* window, int width, int height) noexcept;
    void on_framebuffer_resize(GLFWwindow* window, int width, int height) noexcept;
    void on_content_scale(GLFWwindow* window, float x_scale, float y_scale) noexcept;
    void mouse_position_callback(GLFWwindow* window, double x, double y) noexcept;
    void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) noexcept;
    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept;
    void char_callback(GLFWwindow* window, unsigned int codepoint) noexcept;
    void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) noexcept;
    void focus_callback(GLFWwindow* window, int focused) noexcept;
    void close_callback(GLFWwindow* window) noexcept;
} // namespace gcanvas

#endif // GCANVAS_NATIVE_WINDOW_CALLBACKS_HPP
