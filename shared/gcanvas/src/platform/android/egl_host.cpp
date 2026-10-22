#include "gcanvas/platform/android_egl.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <android/native_window.h>

#include <stdexcept>
#include <string>
#include <thread>

namespace gcanvas::android
{
    struct AndroidEglHost::Impl
    {
        EGLDisplay display = EGL_NO_DISPLAY;
        EGLConfig config = nullptr;
        EGLContext context = EGL_NO_CONTEXT;
        EGLSurface fallback_surface = EGL_NO_SURFACE;
        EGLSurface window_surface = EGL_NO_SURFACE;
        ANativeWindow* window = nullptr;
        int width = 0;
        int height = 0;
        std::thread::id owner_thread = std::this_thread::get_id();

        ~Impl()
        {
            cleanup();
        }

        void require_owner_thread() const
        {
            if (std::this_thread::get_id() != owner_thread)
                throw std::logic_error(
                    "Android EGL host operations are confined to the creating thread");
        }

        [[noreturn]] static void fail(const char* operation)
        {
            const EGLint error = eglGetError();
            if (error == EGL_CONTEXT_LOST)
            {
                throw std::runtime_error(
                    std::string(operation) +
                    " failed with EGL_CONTEXT_LOST; destroy and recreate the gCanvas "
                    "Context and AndroidEglHost");
            }
            throw std::runtime_error(
                std::string(operation) + " failed with EGL error " +
                std::to_string(static_cast<unsigned int>(error)));
        }

        void initialize()
        {
            require_owner_thread();

            display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            if (display == EGL_NO_DISPLAY)
                fail("eglGetDisplay");

            EGLint egl_major = 0;
            EGLint egl_minor = 0;
            if (eglInitialize(display, &egl_major, &egl_minor) != EGL_TRUE)
                fail("eglInitialize");
            if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE)
                fail("eglBindAPI");

#if defined(EGL_OPENGL_ES3_BIT)
            constexpr EGLint es3_bit = EGL_OPENGL_ES3_BIT;
#else
            constexpr EGLint es3_bit = EGL_OPENGL_ES3_BIT_KHR;
#endif

            const EGLint config_attributes[] = {
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                EGL_RENDERABLE_TYPE, es3_bit,
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_ALPHA_SIZE, 8,
                EGL_STENCIL_SIZE, 8,
                EGL_NONE,
            };
            EGLint config_count = 0;
            if (eglChooseConfig(display, config_attributes, &config, 1, &config_count) != EGL_TRUE ||
                config_count != 1)
            {
                throw std::runtime_error(
                    "Android EGL requires a GLES3 RGBA8/stencil8 window+pbuffer config");
            }

            const EGLint context_attributes[] = {
                EGL_CONTEXT_CLIENT_VERSION, 3,
                EGL_NONE,
            };
            context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
            if (context == EGL_NO_CONTEXT)
                fail("eglCreateContext");

            const EGLint pbuffer_attributes[] = {
                EGL_WIDTH, 1,
                EGL_HEIGHT, 1,
                EGL_NONE,
            };
            fallback_surface = eglCreatePbufferSurface(display, config, pbuffer_attributes);
            if (fallback_surface == EGL_NO_SURFACE)
                fail("eglCreatePbufferSurface");

            make_fallback_current();
        }

        void make_fallback_current()
        {
            require_owner_thread();
            if (eglMakeCurrent(display, fallback_surface, fallback_surface, context) != EGL_TRUE)
                fail("eglMakeCurrent(fallback)");
        }

        void make_active_current()
        {
            require_owner_thread();
            const EGLSurface surface =
                window_surface != EGL_NO_SURFACE ? window_surface : fallback_surface;
            if (eglMakeCurrent(display, surface, surface, context) != EGL_TRUE)
                fail("eglMakeCurrent");
        }

        void attach_window(ANativeWindow* new_window)
        {
            require_owner_thread();
            if (new_window == nullptr)
                throw std::invalid_argument("Android EGL requires a non-null ANativeWindow");
            if (window_surface != EGL_NO_SURFACE)
                throw std::logic_error("Android EGL window surface is already attached");

            ANativeWindow_acquire(new_window);

            EGLint native_format = 0;
            if (eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &native_format) != EGL_TRUE)
            {
                ANativeWindow_release(new_window);
                fail("eglGetConfigAttrib(EGL_NATIVE_VISUAL_ID)");
            }
            if (ANativeWindow_setBuffersGeometry(new_window, 0, 0, native_format) != 0)
            {
                ANativeWindow_release(new_window);
                throw std::runtime_error("ANativeWindow_setBuffersGeometry failed");
            }

            EGLSurface new_surface =
                eglCreateWindowSurface(display, config, new_window, nullptr);
            if (new_surface == EGL_NO_SURFACE)
            {
                ANativeWindow_release(new_window);
                fail("eglCreateWindowSurface");
            }

            if (eglMakeCurrent(display, new_surface, new_surface, context) != EGL_TRUE)
            {
                eglDestroySurface(display, new_surface);
                ANativeWindow_release(new_window);
                fail("eglMakeCurrent(window)");
            }

            EGLint new_width = 0;
            EGLint new_height = 0;
            if (eglQuerySurface(display, new_surface, EGL_WIDTH, &new_width) != EGL_TRUE ||
                eglQuerySurface(display, new_surface, EGL_HEIGHT, &new_height) != EGL_TRUE ||
                new_width <= 0 || new_height <= 0)
            {
                make_fallback_current();
                eglDestroySurface(display, new_surface);
                ANativeWindow_release(new_window);
                throw std::runtime_error("Android EGL window surface has invalid dimensions");
            }

            window = new_window;
            window_surface = new_surface;
            width = new_width;
            height = new_height;
        }

        void detach_window()
        {
            require_owner_thread();
            if (window_surface == EGL_NO_SURFACE)
            {
                make_fallback_current();
                width = 0;
                height = 0;
                return;
            }

            make_fallback_current();

            eglDestroySurface(display, window_surface);
            window_surface = EGL_NO_SURFACE;
            if (window != nullptr)
            {
                ANativeWindow_release(window);
                window = nullptr;
            }
            width = 0;
            height = 0;
        }

        void cleanup() noexcept
        {
            if (display == EGL_NO_DISPLAY)
                return;

            if (context != EGL_NO_CONTEXT && fallback_surface != EGL_NO_SURFACE)
                eglMakeCurrent(display, fallback_surface, fallback_surface, context);

            if (window_surface != EGL_NO_SURFACE)
            {
                eglDestroySurface(display, window_surface);
                window_surface = EGL_NO_SURFACE;
            }
            if (window != nullptr)
            {
                ANativeWindow_release(window);
                window = nullptr;
            }

            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

            if (fallback_surface != EGL_NO_SURFACE)
            {
                eglDestroySurface(display, fallback_surface);
                fallback_surface = EGL_NO_SURFACE;
            }
            if (context != EGL_NO_CONTEXT)
            {
                eglDestroyContext(display, context);
                context = EGL_NO_CONTEXT;
            }

            eglTerminate(display);
            display = EGL_NO_DISPLAY;
            config = nullptr;
            width = 0;
            height = 0;
        }
    };

    AndroidEglHost::AndroidEglHost() : _impl(std::make_unique<Impl>())
    {
    }

    std::unique_ptr<AndroidEglHost> AndroidEglHost::create(ANativeWindow* initial_window)
    {
        if (initial_window == nullptr)
            throw std::invalid_argument("Android EGL requires an initial ANativeWindow");

        auto host = std::unique_ptr<AndroidEglHost>(new AndroidEglHost());
        host->_impl->initialize();
        host->_impl->attach_window(initial_window);
        return host;
    }

    AndroidEglHost::~AndroidEglHost() = default;

    opengles::Host AndroidEglHost::host_callbacks() noexcept
    {
        return {
            this,
            make_current_callback,
            swap_buffers_callback,
            set_swap_interval_callback,
            framebuffer_size_callback,
        };
    }

    void AndroidEglHost::suspend(Context& context)
    {
        if (!_impl)
            throw std::logic_error("Android EGL host is not initialized");
        _impl->require_owner_thread();
        if (context.backend() != Backend::OpenGLES)
            throw std::invalid_argument("Android EGL host requires an OpenGLES context");

        _impl->make_active_current();
        context.resize_context(0, 0);
        _impl->detach_window();
    }

    void AndroidEglHost::resume(Context& context, ANativeWindow* window)
    {
        if (!_impl)
            throw std::logic_error("Android EGL host is not initialized");
        _impl->require_owner_thread();
        if (context.backend() != Backend::OpenGLES)
            throw std::invalid_argument("Android EGL host requires an OpenGLES context");

        _impl->attach_window(window);
        try
        {
            context.resize_context(_impl->width, _impl->height);
        }
        catch (...)
        {
            _impl->detach_window();
            throw;
        }
    }

    bool AndroidEglHost::has_surface() const noexcept
    {
        return _impl && _impl->window_surface != EGL_NO_SURFACE;
    }

    int AndroidEglHost::framebuffer_width() const noexcept
    {
        return _impl ? _impl->width : 0;
    }

    int AndroidEglHost::framebuffer_height() const noexcept
    {
        return _impl ? _impl->height : 0;
    }

    void AndroidEglHost::make_current_callback(void* user_data)
    {
        static_cast<AndroidEglHost*>(user_data)->_impl->make_active_current();
    }

    void AndroidEglHost::swap_buffers_callback(void* user_data)
    {
        auto* host = static_cast<AndroidEglHost*>(user_data);
        host->_impl->require_owner_thread();
        if (host->_impl->window_surface == EGL_NO_SURFACE)
            throw std::logic_error("Android EGL cannot swap without a window surface");
        if (eglSwapBuffers(host->_impl->display, host->_impl->window_surface) != EGL_TRUE)
            Impl::fail("eglSwapBuffers");
    }

    void AndroidEglHost::set_swap_interval_callback(void* user_data, int interval)
    {
        auto* host = static_cast<AndroidEglHost*>(user_data);
        host->_impl->require_owner_thread();
        host->_impl->make_active_current();
        if (eglSwapInterval(host->_impl->display, interval) != EGL_TRUE)
            Impl::fail("eglSwapInterval");
    }

    void AndroidEglHost::framebuffer_size_callback(void* user_data, int* width, int* height)
    {
        auto* host = static_cast<AndroidEglHost*>(user_data);
        host->_impl->require_owner_thread();
        *width = host->_impl->width;
        *height = host->_impl->height;
    }
}
