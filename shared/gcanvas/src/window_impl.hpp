#ifndef GCANVAS_WINDOW_IMPL_HPP
#define GCANVAS_WINDOW_IMPL_HPP

#include "gcanvas/window.hpp"
#include "native_callback_error_state.hpp"
#include "window_coordinates.hpp"
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
        float _content_scale_x = 1;
        float _content_scale_y = 1;
        float _dpi_scale = 1;
        bool _native_pixel_size = false;
        int _logical_width = 1;
        int _logical_height = 1;
        int _framebuffer_width = -1;
        int _framebuffer_height = -1;
        int _synced_framebuffer_width = -1;
        int _synced_framebuffer_height = -1;
        int _last_x_pos = -1;
        int _last_y_pos = -1;
        int _last_x_scale = -1;
        int _last_y_scale = -1;

        std::shared_ptr<detail::WindowListenerState> _listeners =
            std::make_shared<detail::WindowListenerState>();
        detail::NativeCallbackErrorState _native_callback_errors;

        std::unordered_map<Cursor*, GLFWcursor*> _cursors;
        std::unordered_map<CURSOR_TYPE, GLFWcursor*> _default_cursors;

        GLFWwindow* getGLFWWindow();
        CanvasMetrics canvas_metrics() const;
        bool refresh_window_metrics();
        gcanvas::vec2 window_to_logical_scale() const;
        detail::LogicalPointerPosition window_position_to_logical(double x, double y) const;
        void sync_context_metrics();
        void sync_framebuffer_extent(int width, int height);
        void create_window(const WindowConfig& config);
        void load_icon(const WindowConfig& config);
    };

} // namespace gcanvas

#endif // GCANVAS_WINDOW_IMPL_HPP
