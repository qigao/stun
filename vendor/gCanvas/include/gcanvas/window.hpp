#ifndef GCANVAS_WINDOW_HPP
#define GCANVAS_WINDOW_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "gcanvas/context.hpp"
#include "gcanvas/event.hpp"
#include "gcanvas/gcanvas.hpp"
#include "gcanvas/vec2.hpp"

// #define GCANVAS_ICON "./gcanvas_res/gcanvas_icon.png"

namespace gcanvas
{
    namespace detail
    {
        class WindowListenerState;
    }

    /// Move-only ownership of one Window callback registration.
    ///
    /// Registration, reset, and destruction are confined to the Window owner
    /// thread. Destroying a subscription after its Window is safe.
    class GCANVAS_API WindowListenerSubscription
    {
    public:
        WindowListenerSubscription() noexcept = default;
        ~WindowListenerSubscription();

        WindowListenerSubscription(const WindowListenerSubscription&) = delete;
        WindowListenerSubscription& operator=(const WindowListenerSubscription&) = delete;
        WindowListenerSubscription(WindowListenerSubscription&& other) noexcept;
        WindowListenerSubscription& operator=(WindowListenerSubscription&& other) noexcept;

        /// Removes this registration. Repeated calls are no-ops.
        void reset() noexcept;

        /// Returns whether this token still owns a live registration.
        bool active() const noexcept;

    private:
        WindowListenerSubscription(std::weak_ptr<detail::WindowListenerState> state,
                                   std::uint64_t id) noexcept;

        std::weak_ptr<detail::WindowListenerState> _state;
        std::uint64_t _id = 0;

        friend class detail::WindowListenerState;
    };

    extern "C" struct GCANVAS_API WindowConfig
    {
        const char* title = "gCanvas";

        int width = 500;
        int height = 600;

        int position_x = -1;
        int position_y = -1;

        float x_scale = 1;
        float y_scale = 1;

        bool decorated = true;
        bool transparent = false;
        bool resizeable = true;
        bool visible = true;
        bool vsync = false;
        bool native_pixel_size = false;

        Backend backend = Backend::OpenGL;

        const char* icon_file = nullptr;
    };

    enum CURSOR_TYPE
    {
        ARROW_CURSOR = 0x00036001,
        IBEAM_CURSOR = 0x00036002,
        CROSSHAIR_CURSOR = 0x00036003,
        HAND_CURSOR = 0x00036004,
        HRESIZE_CURSOR = 0x00036005,
        VRESIZE_CURSOR = 0x00036006
    };

    extern "C" struct GCANVAS_API Cursor
    {
        int width;
        int height;
        int hot_x;
        int hot_y;
        unsigned char* data;
    };

    class GCANVAS_API Window
    {
    public:
        static std::unique_ptr<Window> create(WindowConfig config);
        virtual ~Window() = default;

        Context& create_context();
        Context* get_context();
        Backend get_backend() const noexcept;
        bool is_running();
        bool get_vsync();
        int get_width();
        int get_height();
        gcanvas::vec2 get_position();
        gcanvas::vec2 get_scale();
        gcanvas::vec2 get_offset();
        float get_dpi_scale();

        void set_title(const std::string& title);
        void set_position(int x, int y);
        void set_size(int width, int height);
        void set_vsync(bool vsync);
        void set_scale(float x, float y);
        void set_scale(float scalar);
        void set_offset(float x, float y);
        void set_fullscreen(bool fullscreen);

        /// Reports whether this platform provides native GUI pointer capture.
        bool supports_pointer_capture() const noexcept;

        /// Reports whether this window currently owns native pointer capture.
        bool has_pointer_capture() const noexcept;

        /// Acquires or releases native GUI pointer capture. Unsupported
        /// platforms throw std::logic_error instead of changing cursor mode.
        void set_pointer_capture(bool captured);

        bool is_fullscreen();

        /// Adds persistent callbacks retained until reset_listener() or Window destruction.
        /// Empty callbacks throw std::invalid_argument when registered.
        void add_resize_listener(std::function<void(resize_event)> callback);
        void add_mouse_move_listener(std::function<void(mouse_move_event)> callback);
        void add_mouse_click_listener(std::function<void(mouse_button_event)> callback);
        void add_key_listener(std::function<void(key_event)> callback);
        void add_char_listener(std::function<void(char_event)> callback);
        void add_scroll_listener(std::function<void(scroll_event)> callback);
        void add_focus_listener(std::function<void(focus_event)> callback);
        void add_close_listener(std::function<void(close_event)> callback);

        /// Adds one owner-scoped callback. The returned token removes only this
        /// registration when reset or destroyed. An empty callback throws
        /// std::invalid_argument. Callbacks added during delivery start at the
        /// next event; removing a pending callback prevents same-event delivery.
        WindowListenerSubscription subscribe_resize_listener(
            std::function<void(resize_event)> callback);
        WindowListenerSubscription subscribe_mouse_move_listener(
            std::function<void(mouse_move_event)> callback);
        WindowListenerSubscription subscribe_mouse_click_listener(
            std::function<void(mouse_button_event)> callback);
        WindowListenerSubscription subscribe_key_listener(std::function<void(key_event)> callback);
        WindowListenerSubscription subscribe_char_listener(
            std::function<void(char_event)> callback);
        WindowListenerSubscription subscribe_scroll_listener(
            std::function<void(scroll_event)> callback);
        WindowListenerSubscription subscribe_focus_listener(
            std::function<void(focus_event)> callback);
        WindowListenerSubscription subscribe_close_listener(
            std::function<void(close_event)> callback);

        /// Clears persistent and scoped registrations in every event category.
        /// Existing scoped tokens become inactive.
        void reset_listener();

        void set_cursor(CURSOR_TYPE cursor_type);
        void set_cursor(Cursor* cursor);

        void minimize();
        void maximize();
        void restore();
        void close();

        void poll_events();
        void wait_events();
        void wait_events(float time);

        static void trigger_events();
        static double now();

    protected:
        Window() = default;

        std::unique_ptr<Context> _context;
        bool _vsync = true;
        Backend _backend = Backend::OpenGL;
    };

} // namespace gcanvas

#endif // GCANVAS_WINDOW_HPP
