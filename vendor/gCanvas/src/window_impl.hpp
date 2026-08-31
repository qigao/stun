#ifndef GCANVAS_WINDOW_IMPL_HPP
#define GCANVAS_WINDOW_IMPL_HPP

#include "gcanvas/window.hpp"
#include "window_listener_state.hpp"
#include <unordered_map>

struct GLFWwindow;
struct GLFWcursor;

namespace gcanvas
{
    class WindowImpl : public Window
    {
    public:
        WindowImpl(const WindowConfig& config);
        ~WindowImpl();

        GLFWwindow* _glfw_window = nullptr;
        float _x_scale = 1;
        float _y_scale = 1;
        float _x_offset = 0;
        float _y_offset = 0;
        float _dpi_scale = 1;
        int _last_x_pos = -1;
        int _last_y_pos = -1;
        int _last_x_scale = -1;
        int _last_y_scale = -1;

        std::shared_ptr<detail::WindowListenerState> _listeners =
            std::make_shared<detail::WindowListenerState>();

        std::unordered_map<Cursor*, GLFWcursor*> _cursors;
        std::unordered_map<CURSOR_TYPE, GLFWcursor*> _default_cursors;

        GLFWwindow* getGLFWWindow();
        CanvasMetrics canvas_metrics() const;
        void sync_context_metrics();
        void create_window(const WindowConfig& config);
        void load_icon(const WindowConfig& config);
    };

} // namespace gcanvas

#endif // GCANVAS_WINDOW_IMPL_HPP
