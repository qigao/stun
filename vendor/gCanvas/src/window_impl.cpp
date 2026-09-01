#include "window_impl.hpp"

#include "stb_image.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

#include "gcanvas/context.hpp"
#ifdef GCANVAS_HAS_OPENGL
#include "gcanvas/backends/opengl.hpp"
#endif
#ifdef GCANVAS_HAS_VULKAN
#include "gcanvas/backends/vulkan.hpp"
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace gcanvas
{
    namespace
    {
        std::uint32_t window_count = 0;

#ifdef GCANVAS_HAS_OPENGL
        opengl::ProcAddress glfw_get_proc_address(void*, const char* name)
        {
            return reinterpret_cast<opengl::ProcAddress>(glfwGetProcAddress(name));
        }

        void glfw_make_current(void* user_data)
        {
            glfwMakeContextCurrent(static_cast<GLFWwindow*>(user_data));
        }

        void glfw_swap_buffers(void* user_data)
        {
            glfwSwapBuffers(static_cast<GLFWwindow*>(user_data));
        }

        void glfw_set_swap_interval(void*, int interval)
        {
            glfwSwapInterval(interval);
        }

        void glfw_framebuffer_size(void* user_data, int* width, int* height)
        {
            glfwGetFramebufferSize(static_cast<GLFWwindow*>(user_data), width, height);
        }
#endif

#ifdef GCANVAS_HAS_VULKAN
        const char* const* glfw_required_vulkan_extensions(void*, std::uint32_t* count)
        {
            return glfwGetRequiredInstanceExtensions(count);
        }

        int glfw_create_vulkan_surface(void* user_data, std::uintptr_t instance,
                                       std::uint64_t* surface)
        {
            VkSurfaceKHR created_surface = VK_NULL_HANDLE;
            const VkResult result = glfwCreateWindowSurface(reinterpret_cast<VkInstance>(instance),
                                                            static_cast<GLFWwindow*>(user_data),
                                                            nullptr, &created_surface);
            static_assert(sizeof(created_surface) <= sizeof(*surface));
            *surface = 0;
            std::memcpy(surface, &created_surface, sizeof(created_surface));
            return static_cast<int>(result);
        }
#endif
    } // namespace

    /* ------------------------ DOWNCAST ------------------------ */

    inline WindowImpl* getImpl(Window* ptr)
    {
        return (WindowImpl*)ptr;
    }
    inline const WindowImpl* getImpl(const Window* ptr)
    {
        return (const WindowImpl*)ptr;
    }

    void release_pointer_capture_on_window_loss(WindowImpl* impl) noexcept
    {
#ifdef _WIN32
        HWND native_window = glfwGetWin32Window(impl->_glfw_window);
        if (GetCapture() == native_window)
            ReleaseCapture();
#else
        (void)impl;
#endif
    }

    /* ------------------------ FUNCTION DECLARATION ------------------------ */

    void on_window_resize(GLFWwindow* window, int width, int height);
    void on_framebuffer_resize(GLFWwindow* window, int width, int height);
    void on_content_scale(GLFWwindow* window, float x_scale, float y_scale);
    void mouse_position_callback(GLFWwindow* window, double x, double y);
    void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void char_callback(GLFWwindow* window, unsigned int codepoint);
    void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    void focus_callback(GLFWwindow* window, int focused);
    void close_callback(GLFWwindow* window);

    /* ------------------------ PUBLIC IMPLEMENTATION ------------------------ */

    std::unique_ptr<Window> Window::create(WindowConfig config)
    {
        return std::make_unique<WindowImpl>(config);
    }

    WindowListenerSubscription::WindowListenerSubscription(
        std::weak_ptr<detail::WindowListenerState> state, std::uint64_t id) noexcept
        : _state(std::move(state)), _id(id)
    {
    }

    WindowListenerSubscription::~WindowListenerSubscription()
    {
        reset();
    }

    WindowListenerSubscription::WindowListenerSubscription(
        WindowListenerSubscription&& other) noexcept
        : _state(std::move(other._state)), _id(std::exchange(other._id, 0))
    {
    }

    WindowListenerSubscription& WindowListenerSubscription::operator=(
        WindowListenerSubscription&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            _state = std::move(other._state);
            _id = std::exchange(other._id, 0);
        }
        return *this;
    }

    void WindowListenerSubscription::reset() noexcept
    {
        const std::uint64_t id = std::exchange(_id, 0);
        if (id == 0)
            return;
        if (const auto state = _state.lock())
            state->remove(id);
        _state.reset();
    }

    bool WindowListenerSubscription::active() const noexcept
    {
        if (_id == 0)
            return false;
        const auto state = _state.lock();
        return state != nullptr && state->contains(_id);
    }

    void Window::set_title(const std::string& title)
    {
        WindowImpl* impl = getImpl(this);
        glfwSetWindowTitle(impl->_glfw_window, title.c_str());
    }

    void Window::set_position(int x, int y)
    {
        WindowImpl* impl = getImpl(this);
        glfwSetWindowPos(impl->_glfw_window, x, y);
    }

    void Window::set_size(int width, int height)
    {
        if (width <= 0 || height <= 0)
        {
            throw std::invalid_argument("gCanvas logical window dimensions must be positive");
        }
        WindowImpl* impl = getImpl(this);
        const gcanvas::vec2 to_logical = impl->window_to_logical_scale();
        const double window_width = static_cast<double>(width) / to_logical.get_x();
        const double window_height = static_cast<double>(height) / to_logical.get_y();
        constexpr double maximum_window_dimension =
            static_cast<double>((std::numeric_limits<int>::max)());
        if (!std::isfinite(window_width) || !std::isfinite(window_height) ||
            window_width > maximum_window_dimension || window_height > maximum_window_dimension)
        {
            throw std::overflow_error("gCanvas logical window dimensions exceed platform limits");
        }
        glfwSetWindowSize(impl->_glfw_window, static_cast<int>(std::lround(window_width)),
                          static_cast<int>(std::lround(window_height)));
        impl->refresh_window_metrics();
        impl->sync_context_metrics();
        if (_context != nullptr)
        {
            impl->sync_framebuffer_extent(impl->_framebuffer_width,
                                          impl->_framebuffer_height);
        }
    }

    void Window::set_vsync(bool vsync)
    {
        _vsync = vsync;
        if (_context != nullptr)
        {
            _context->set_vsync(vsync);
        }
    }

    void Window::set_scale(float x, float y)
    {
        WindowImpl* impl = getImpl(this);
        impl->_x_scale = x;
        impl->_y_scale = y;
        impl->sync_context_metrics();
    }

    void Window::set_scale(float scalar)
    {
        WindowImpl* impl = getImpl(this);
        impl->_x_scale = scalar;
        impl->_y_scale = scalar;
        impl->sync_context_metrics();
    }

    void Window::set_offset(float x, float y)
    {
        WindowImpl* impl = getImpl(this);
        impl->_x_offset = x; // * impl->_dpi_scale;
        impl->_y_offset = y; // * impl->_dpi_scale;
        impl->sync_context_metrics();
    }

    void Window::set_fullscreen(bool fullscreen)
    {
        WindowImpl* impl = getImpl(this);

        if (is_fullscreen() == fullscreen)
            return;

        if (fullscreen)
        {
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);

            glfwGetWindowPos(impl->_glfw_window, &impl->_last_x_pos, &impl->_last_y_pos);
            glfwGetWindowSize(impl->_glfw_window, &impl->_last_x_scale, &impl->_last_y_scale);

            glfwSetWindowMonitor(impl->_glfw_window, monitor, 0, 0, mode->width, mode->height, 0);
        }
        else
        {
            glfwSetWindowMonitor(impl->_glfw_window, nullptr, impl->_last_x_pos, impl->_last_y_pos,
                                 impl->_last_x_scale, impl->_last_y_scale, 0);
        }
    }

    bool Window::is_fullscreen()
    {
        WindowImpl* impl = getImpl(this);
        return glfwGetWindowMonitor(impl->_glfw_window) != nullptr;
    }

    void Window::add_resize_listener(std::function<void(resize_event)> callback)
    {
        WindowImpl* impl = getImpl(this);
        impl->_listeners->add_resize(std::move(callback));
    }

    void Window::add_mouse_move_listener(std::function<void(mouse_move_event)> callback)
    {
        WindowImpl* impl = getImpl(this);
        impl->_listeners->add_mouse_move(std::move(callback));
    }

    void Window::add_mouse_click_listener(std::function<void(mouse_button_event)> callback)
    {
        WindowImpl* impl = getImpl(this);
        impl->_listeners->add_mouse_button(std::move(callback));
    }

    void Window::add_key_listener(std::function<void(key_event)> callback)
    {
        WindowImpl* impl = getImpl(this);
        impl->_listeners->add_key(std::move(callback));
    }

    void Window::add_char_listener(std::function<void(char_event)> callback)
    {
        WindowImpl* impl = getImpl(this);
        impl->_listeners->add_char(std::move(callback));
    }

    void Window::add_scroll_listener(std::function<void(scroll_event)> callback)
    {
        WindowImpl* impl = getImpl(this);
        impl->_listeners->add_scroll(std::move(callback));
    }

    bool Window::supports_pointer_capture() const noexcept
    {
#ifdef _WIN32
        return true;
#else
        return false;
#endif
    }

    bool Window::has_pointer_capture() const noexcept
    {
#ifdef _WIN32
        const WindowImpl* impl = getImpl(this);
        return GetCapture() == glfwGetWin32Window(impl->_glfw_window);
#else
        return false;
#endif
    }

    void Window::set_pointer_capture(bool captured)
    {
#ifdef _WIN32
        WindowImpl* impl = getImpl(this);
        HWND native_window = glfwGetWin32Window(impl->_glfw_window);
        if (captured)
        {
            if (GetCapture() != native_window)
                SetCapture(native_window);
            if (GetCapture() != native_window)
                throw std::runtime_error("failed to acquire Win32 pointer capture");
            return;
        }

        if (GetCapture() == native_window)
        {
            ReleaseCapture();
            if (GetCapture() == native_window)
                throw std::runtime_error("failed to release Win32 pointer capture");
        }
#else
        (void)captured;
        throw std::logic_error("native pointer capture is not supported on this platform");
#endif
    }

    void Window::add_focus_listener(std::function<void(focus_event)> callback)
    {
        WindowImpl* impl = getImpl(this);
        impl->_listeners->add_focus(std::move(callback));
    }

    void Window::add_close_listener(std::function<void(close_event)> callback)
    {
        WindowImpl* impl = getImpl(this);
        impl->_listeners->add_close(std::move(callback));
    }

    WindowListenerSubscription Window::subscribe_resize_listener(
        std::function<void(resize_event)> callback)
    {
        return getImpl(this)->_listeners->subscribe_resize(std::move(callback));
    }

    WindowListenerSubscription Window::subscribe_mouse_move_listener(
        std::function<void(mouse_move_event)> callback)
    {
        return getImpl(this)->_listeners->subscribe_mouse_move(std::move(callback));
    }

    WindowListenerSubscription Window::subscribe_mouse_click_listener(
        std::function<void(mouse_button_event)> callback)
    {
        return getImpl(this)->_listeners->subscribe_mouse_button(std::move(callback));
    }

    WindowListenerSubscription Window::subscribe_key_listener(
        std::function<void(key_event)> callback)
    {
        return getImpl(this)->_listeners->subscribe_key(std::move(callback));
    }

    WindowListenerSubscription Window::subscribe_char_listener(
        std::function<void(char_event)> callback)
    {
        return getImpl(this)->_listeners->subscribe_char(std::move(callback));
    }

    WindowListenerSubscription Window::subscribe_scroll_listener(
        std::function<void(scroll_event)> callback)
    {
        return getImpl(this)->_listeners->subscribe_scroll(std::move(callback));
    }

    WindowListenerSubscription Window::subscribe_focus_listener(
        std::function<void(focus_event)> callback)
    {
        return getImpl(this)->_listeners->subscribe_focus(std::move(callback));
    }

    WindowListenerSubscription Window::subscribe_close_listener(
        std::function<void(close_event)> callback)
    {
        return getImpl(this)->_listeners->subscribe_close(std::move(callback));
    }

    void Window::reset_listener()
    {
        WindowImpl* impl = getImpl(this);
        impl->_listeners->reset();
    }

    void Window::set_cursor(CURSOR_TYPE cursor_type)
    {
        WindowImpl* impl = getImpl(this);

        if (impl->_default_cursors.find(cursor_type) != impl->_default_cursors.end())
        {
            impl->_default_cursors[cursor_type] = glfwCreateStandardCursor(cursor_type);
        }

        glfwSetCursor(impl->_glfw_window, impl->_default_cursors[cursor_type]);
    }

    void Window::set_cursor(Cursor* cursor)
    {
        WindowImpl* impl = getImpl(this);
        if (cursor == nullptr)
        {
            glfwSetCursor(impl->_glfw_window, NULL);
            return;
        }

        if (impl->_cursors.find(cursor) == impl->_cursors.end())
        {
            GLFWimage image;
            image.width = 16;
            image.height = 16;
            image.pixels = cursor->data;

            impl->_cursors[cursor] = glfwCreateCursor(&image, cursor->hot_x, cursor->hot_y);
        }

        glfwSetCursor(impl->_glfw_window, impl->_cursors[cursor]);
    }

    void Window::minimize()
    {
        WindowImpl* impl = getImpl(this);
        glfwIconifyWindow(impl->_glfw_window);
    }

    void Window::maximize()
    {
        WindowImpl* impl = getImpl(this);
        glfwMaximizeWindow(impl->_glfw_window);
    }

    void Window::restore()
    {
        WindowImpl* impl = getImpl(this);
        glfwRestoreWindow(impl->_glfw_window);
    }

    void Window::close()
    {
        WindowImpl* impl = getImpl(this);
        glfwSetWindowShouldClose(impl->_glfw_window, GLFW_TRUE);
    }

    bool Window::get_vsync()
    {
        return _vsync;
    }

    int Window::get_width()
    {
        WindowImpl* impl = getImpl(this);
        return impl->_logical_width;
    }

    int Window::get_height()
    {
        WindowImpl* impl = getImpl(this);
        return impl->_logical_height;
    }

    gcanvas::vec2 Window::get_content_scale()
    {
        WindowImpl* impl = getImpl(this);
        return gcanvas::vec2(impl->_content_scale_x, impl->_content_scale_y);
    }

    gcanvas::vec2 Window::get_position()
    {
        WindowImpl* impl = getImpl(this);
        int x, y;
        glfwGetWindowPos(impl->_glfw_window, &x, &y);
        return gcanvas::vec2(x, y);
    }

    bool Window::is_running()
    {
        WindowImpl* impl = getImpl(this);
        return !glfwWindowShouldClose(impl->_glfw_window);
    }

    gcanvas::vec2 Window::get_scale()
    {
        WindowImpl* impl = getImpl(this);
        return gcanvas::vec2(impl->_x_scale, impl->_y_scale);
    }

    gcanvas::vec2 Window::get_offset()
    {
        WindowImpl* impl = getImpl(this);
        return gcanvas::vec2(impl->_x_offset, impl->_y_offset);
    }

    float Window::get_dpi_scale()
    {
        WindowImpl* impl = getImpl(this);
        return impl->_dpi_scale;
    }

    double Window::now()
    {
        return glfwGetTime();
    }

    void Window::poll_events()
    {
        glfwPollEvents();
    }

    void Window::wait_events()
    {
        glfwWaitEvents();
    }

    void Window::wait_events(float time)
    {
        glfwWaitEventsTimeout(time);
    }

    void Window::trigger_events()
    {
        glfwPostEmptyEvent();
    }

    Context& Window::create_context()
    {
        WindowImpl* impl = getImpl(this);
        if (_context != nullptr)
        {
            return *_context;
        }

        std::unique_ptr<Context> context;
        switch (_backend)
        {
        case Backend::OpenGL:
#ifdef GCANVAS_HAS_OPENGL
            context = opengl::create_context(
                {impl->canvas_metrics(),
                 {},
                 {impl->_glfw_window, glfw_get_proc_address, glfw_make_current, glfw_swap_buffers,
                  glfw_set_swap_interval, glfw_framebuffer_size}});
            break;
#else
            throw std::logic_error("gCanvas was built without the OpenGL backend");
#endif
        case Backend::Vulkan:
#ifdef GCANVAS_HAS_VULKAN
            context = vulkan::create_context(
                {impl->canvas_metrics(),
                 {},
                 {impl->_glfw_window, glfw_required_vulkan_extensions, glfw_create_vulkan_surface},
                 _vsync});
            break;
#else
            throw std::logic_error("gCanvas was built without the Vulkan backend");
#endif
        }
        _context = std::move(context);

        glfwSetWindowUserPointer(impl->_glfw_window, impl);
        glfwSetWindowSizeCallback(impl->_glfw_window, on_window_resize);
        glfwSetFramebufferSizeCallback(impl->_glfw_window, on_framebuffer_resize);
        glfwSetWindowContentScaleCallback(impl->_glfw_window, on_content_scale);
        glfwSetCursorPosCallback(impl->_glfw_window, mouse_position_callback);
        glfwSetMouseButtonCallback(impl->_glfw_window, mouse_button_callback);
        glfwSetKeyCallback(impl->_glfw_window, key_callback);
        glfwSetCharCallback(impl->_glfw_window, char_callback);
        glfwSetScrollCallback(impl->_glfw_window, scroll_callback);
        glfwSetWindowFocusCallback(impl->_glfw_window, focus_callback);
        glfwSetWindowCloseCallback(impl->_glfw_window, close_callback);

        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(impl->_glfw_window, &framebuffer_width, &framebuffer_height);
        impl->sync_framebuffer_extent(framebuffer_width, framebuffer_height);

        return *_context;
    }

    Context* Window::get_context()
    {
        return _context.get();
    }

    Backend Window::get_backend() const noexcept
    {
        return _backend;
    }

    /* ------------------------ PRIVATE IMPLEMENTATION ------------------------ */

    WindowImpl::WindowImpl(const WindowConfig& config)
    {
        if (config.width <= 0 || config.height <= 0)
        {
            throw std::invalid_argument("gCanvas window dimensions must be positive");
        }
        _native_pixel_size = config.native_pixel_size;
        _backend = config.backend;
#ifndef GCANVAS_HAS_OPENGL
        if (_backend == Backend::OpenGL)
        {
            throw std::invalid_argument("OpenGL was not enabled in this gCanvas build");
        }
#endif
#ifndef GCANVAS_HAS_VULKAN
        if (_backend == Backend::Vulkan)
        {
            throw std::invalid_argument("Vulkan was not enabled in this gCanvas build");
        }
#endif
        if (window_count == 0)
        {
            if (!glfwInit())
            {
                throw std::runtime_error("Failed to initialize GLFW");
            }
        }

        glfwDefaultWindowHints();
        if (_backend == Backend::Vulkan)
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }
        else
        {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_STENCIL_BITS, 8);
#ifdef __APPLE__
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
        }
        glfwWindowHint(GLFW_DECORATED, config.decorated);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, config.transparent);
        glfwWindowHint(GLFW_RESIZABLE, config.resizeable);
        glfwWindowHint(GLFW_VISIBLE, config.visible);
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, !config.native_pixel_size);

        create_window(config);
    }

    WindowImpl::~WindowImpl()
    {
        release_pointer_capture_on_window_loss(this);
        if (_context != nullptr)
        {
            if (_backend == Backend::OpenGL)
            {
                glfwMakeContextCurrent(_glfw_window);
            }
            _context.reset();
        }

        for (auto& cursor : _default_cursors)
        {
            glfwDestroyCursor(cursor.second);
        }

        glfwDestroyWindow(_glfw_window);
        --window_count;
        for (auto& cursor : _cursors)
        {
            glfwDestroyCursor(cursor.second);
        }

        if (window_count == 0)
        {
            glfwTerminate();
        }
    }

    GLFWwindow* WindowImpl::getGLFWWindow()
    {
        return _glfw_window;
    }

    CanvasMetrics WindowImpl::canvas_metrics() const
    {
        return {_logical_width, _logical_height, _x_scale, _y_scale,
                _x_offset, _y_offset, _dpi_scale};
    }

    bool WindowImpl::refresh_window_metrics()
    {
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(_glfw_window, &framebuffer_width, &framebuffer_height);

        float content_scale_x = 1.0f;
        float content_scale_y = 1.0f;
        glfwGetWindowContentScale(_glfw_window, &content_scale_x, &content_scale_y);
        if (!std::isfinite(content_scale_x) || !std::isfinite(content_scale_y) ||
            content_scale_x <= 0.0f || content_scale_y <= 0.0f)
        {
            throw std::runtime_error("GLFW returned an invalid window content scale");
        }

        _framebuffer_width = framebuffer_width;
        _framebuffer_height = framebuffer_height;
        _content_scale_x = content_scale_x;
        _content_scale_y = content_scale_y;
        // CanvasMetrics has a source-compatible uniform DPI scale. GLFW platforms used by
        // gCanvas report equal axes in normal desktop configurations; retain the historical
        // y-axis projection while exposing both raw values through get_content_scale().
        _dpi_scale = _native_pixel_size ? 1.0f : content_scale_y;

        if (framebuffer_width == 0 || framebuffer_height == 0)
        {
            return false;
        }

        const int logical_width = _native_pixel_size
                                      ? framebuffer_width
                                      : static_cast<int>(std::lround(
                                            framebuffer_width / _dpi_scale));
        const int logical_height = _native_pixel_size
                                       ? framebuffer_height
                                       : static_cast<int>(std::lround(
                                             framebuffer_height / _dpi_scale));
        if (logical_width <= 0 || logical_height <= 0)
        {
            throw std::runtime_error("GLFW window metrics produced an empty logical viewport");
        }
        const bool changed = logical_width != _logical_width || logical_height != _logical_height;
        _logical_width = logical_width;
        _logical_height = logical_height;
        return changed;
    }

    gcanvas::vec2 WindowImpl::window_to_logical_scale() const
    {
        int window_width = 0;
        int window_height = 0;
        glfwGetWindowSize(_glfw_window, &window_width, &window_height);
        const detail::WindowCoordinateScale scale = detail::window_to_logical_scale(
            _logical_width, _logical_height, window_width, window_height);
        return gcanvas::vec2(static_cast<float>(scale.x), static_cast<float>(scale.y));
    }

    detail::LogicalPointerPosition WindowImpl::window_position_to_logical(double x,
                                                                           double y) const
    {
        int window_width = 0;
        int window_height = 0;
        glfwGetWindowSize(_glfw_window, &window_width, &window_height);
        return detail::map_window_position_to_logical(
            x, y, _logical_width, _logical_height, window_width, window_height);
    }

    void WindowImpl::sync_context_metrics()
    {
        if (_context != nullptr)
        {
            _context->set_metrics(canvas_metrics());
        }
    }

    void WindowImpl::sync_framebuffer_extent(int width, int height)
    {
        if (_context == nullptr)
        {
            return;
        }
        if (width == _synced_framebuffer_width && height == _synced_framebuffer_height)
        {
            return;
        }
        _context->resize_context(width, height);
        _synced_framebuffer_width = width;
        _synced_framebuffer_height = height;
    }

    void WindowImpl::create_window(const WindowConfig& config)
    {
        std::string full_title = config.title;
#ifndef NDEBUG
        full_title += _backend == Backend::Vulkan ? " - [Vulkan]" : " - [OpenGL]";
#endif

        _glfw_window =
            glfwCreateWindow(config.width, config.height, full_title.c_str(), NULL, NULL);
        if (!_glfw_window)
        {
            const char* errorText = NULL;
            glfwGetError(&errorText);
            if (window_count == 0)
            {
                glfwTerminate();
            }
            throw std::runtime_error(errorText != nullptr ? errorText
                                                          : "Failed to create GLFW window");
        }
        ++window_count;
        load_icon(config);

        if (config.position_x != -1 && config.position_y != -1)
        {
            set_position(config.position_x, config.position_y);
        }

        if (_backend == Backend::OpenGL)
        {
            glfwMakeContextCurrent(_glfw_window);
        }
        refresh_window_metrics();

        _vsync = config.vsync;
        if (_backend == Backend::OpenGL)
        {
            glfwSwapInterval(_vsync ? 1 : 0);
        }

        //_x_scale = config.x_scale;
        //_y_scale = config.y_scale;
    }

    void WindowImpl::load_icon(const WindowConfig& config)
    {
        if (config.icon_file == nullptr)
            return;

        GLFWimage icon[1];
        int numComponents;
        icon[0].pixels =
            stbi_load(config.icon_file, &icon[0].width, &icon[0].height, &numComponents, 4);

        if (icon[0].pixels == NULL)
        {
            std::cerr << "Error: Unable to load Icon: " << config.icon_file << "\n";
            return;
        }

        glfwSetWindowIcon(_glfw_window, 1, icon);
        stbi_image_free(icon[0].pixels);
    }

    /* ------------------------ EVENTS ------------------------ */

    void on_window_resize(GLFWwindow* window, int width, int height)
    {
        WindowImpl* winImpl = (WindowImpl*)glfwGetWindowUserPointer(window);
        winImpl->refresh_window_metrics();
        if (winImpl->get_context() != nullptr)
        {
            winImpl->sync_context_metrics();
            winImpl->sync_framebuffer_extent(winImpl->_framebuffer_width,
                                              winImpl->_framebuffer_height);
        }

#ifdef DEBUG
        std::cout << "event: resize "
                  << "width: " << width << " height: " << height << std::endl;
#endif

        winImpl->_listeners->publish(
            resize_event{winImpl->_logical_width, winImpl->_logical_height});
    }

    void on_framebuffer_resize(GLFWwindow* window, int, int)
    {
        WindowImpl* winImpl = (WindowImpl*)glfwGetWindowUserPointer(window);
        const bool logical_size_changed = winImpl->refresh_window_metrics();
        if (winImpl->get_context() != nullptr)
        {
            winImpl->sync_context_metrics();
            winImpl->sync_framebuffer_extent(winImpl->_framebuffer_width,
                                              winImpl->_framebuffer_height);
        }
        if (logical_size_changed)
        {
            winImpl->_listeners->publish(
                resize_event{winImpl->_logical_width, winImpl->_logical_height});
        }
    }

    void on_content_scale(GLFWwindow* window, float, float)
    {
        WindowImpl* winImpl = (WindowImpl*)glfwGetWindowUserPointer(window);
        winImpl->refresh_window_metrics();
        if (winImpl->get_context() != nullptr)
        {
            winImpl->sync_context_metrics();
            winImpl->sync_framebuffer_extent(winImpl->_framebuffer_width,
                                              winImpl->_framebuffer_height);
        }
        winImpl->_listeners->publish(
            resize_event{winImpl->_logical_width, winImpl->_logical_height});
    }

    void mouse_position_callback(GLFWwindow* window, double x, double y)
    {
        WindowImpl* winImpl = (WindowImpl*)glfwGetWindowUserPointer(window);

        /*
        if (winImpl->buttonEvent == 1)
        {
            winImpl->offset_cpx = x - winImpl->cp_x;
            winImpl->offset_cpy = y - winImpl->cp_y;
        }
        */
#ifdef DEBUG
        std::cout << "event: mouse_position "
                  << "x: " << x << " y: " << y << std::endl;
#endif
        const detail::LogicalPointerPosition logical =
            winImpl->window_position_to_logical(x, y);
        winImpl->_listeners->publish(mouse_move_event{logical.x, logical.y});
    }

    void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
    {
        WindowImpl* winImpl = (WindowImpl*)glfwGetWindowUserPointer(window);

        /*
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        {
            winImpl->buttonEvent = 1;
            double x, y;
            glfwGetCursorPos(window, &x, &y);
            winImpl->cp_x = floor(x);
            winImpl->cp_y = floor(y);
        }
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
        {
            winImpl->buttonEvent = 0;
            winImpl->cp_x = 0;
            winImpl->cp_y = 0;
        }
        */
#ifdef DEBUG
        std::cout << "event: mouse_button "
                  << "button: " << button << " action: " << action << " mods: " << mods
                  << std::endl;
#endif
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        const detail::LogicalPointerPosition logical =
            winImpl->window_position_to_logical(x, y);

        winImpl->_listeners->publish(mouse_button_event{(mouse_button)button, (input_action)action,
                                                        (mouse_mod)mods, logical.x, logical.y});
    }

    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        WindowImpl* winImpl = (WindowImpl*)glfwGetWindowUserPointer(window);
        const char* key_code = glfwGetKeyName(key, scancode);

        std::string key_name = (key_code == NULL) ? " " : key_code;

#ifdef DEBUG
        std::cout << "event: key "
                  << "key: " << key << " scancode: " << scancode << " action: " << action
                  << " mods: " << mods << " key name: " << key_name << std::endl;
#endif

        winImpl->_listeners->publish(key_event{(keyboard_key)key, scancode, (input_action)action,
                                               (keyboard_mod)mods, key_code});
    }

    void char_callback(GLFWwindow* window, unsigned int codepoint)
    {
        WindowImpl* winImpl = (WindowImpl*)glfwGetWindowUserPointer(window);

        std::string utf8 = Font::UnicodeToUTF8(codepoint);

#ifdef DEBUG
        std::cout << "event: char "
                  << "char: " << codepoint << std::endl;
#endif

        winImpl->_listeners->publish(char_event{codepoint, utf8.c_str()});
    }

    void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
    {
        WindowImpl* winImpl = (WindowImpl*)glfwGetWindowUserPointer(window);

#ifdef DEBUG
        std::cout << "event: scroll "
                  << "xoffset: " << xoffset << " yoffset: " << yoffset << std::endl;
#endif
        winImpl->_listeners->publish(scroll_event{xoffset, yoffset});
    }

    void focus_callback(GLFWwindow* window, int focused)
    {
        WindowImpl* winImpl = (WindowImpl*)glfwGetWindowUserPointer(window);
        if (focused != GLFW_TRUE)
            release_pointer_capture_on_window_loss(winImpl);
        winImpl->_listeners->publish(focus_event{focused == GLFW_TRUE});
    }

    void close_callback(GLFWwindow* window)
    {
        WindowImpl* winImpl = (WindowImpl*)glfwGetWindowUserPointer(window);
        winImpl->_listeners->publish(close_event{});
    }

} // namespace gcanvas
