#ifndef GCANVAS_PLATFORM_ANDROID_EGL_HPP
#define GCANVAS_PLATFORM_ANDROID_EGL_HPP

#include "gcanvas/backends/opengles.hpp"

#include <memory>

struct ANativeWindow;

namespace gcanvas::android
{
    /**
     * Android EGL presentation host for gCanvas::OpenGLES.
     *
     * The host owns the EGL display/config/context, a persistent fallback pbuffer,
     * and its own reference to the attached ANativeWindow. All operations, callbacks,
     * gCanvas Context use, and destruction are confined to the creating thread.
     *
     * The host must outlive the gCanvas Context created from host_callbacks(). Destroy
     * the gCanvas Context first so GPU resources are released while this host can keep
     * its GLES context current on either the window surface or fallback pbuffer.
     */
    class GCANVAS_API AndroidEglHost
    {
    public:
        static std::unique_ptr<AndroidEglHost> create(ANativeWindow* initial_window);

        ~AndroidEglHost();

        AndroidEglHost(const AndroidEglHost&) = delete;
        AndroidEglHost& operator=(const AndroidEglHost&) = delete;
        AndroidEglHost(AndroidEglHost&&) = delete;
        AndroidEglHost& operator=(AndroidEglHost&&) = delete;

        opengles::Host host_callbacks() noexcept;

        /**
         * Suspends presentation between completed frames.
         *
         * The gCanvas Context becomes headless, the window EGLSurface is destroyed,
         * and the GLES context remains alive/current on the persistent fallback pbuffer.
         */
        void suspend(Context& context);

        /**
         * Attaches a replacement ANativeWindow and resumes the existing gCanvas Context
         * without recreating renderer-owned GPU resources.
         */
        void resume(Context& context, ANativeWindow* window);

        bool has_surface() const noexcept;
        int framebuffer_width() const noexcept;
        int framebuffer_height() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;

        AndroidEglHost();

        static void make_current_callback(void* user_data);
        static void swap_buffers_callback(void* user_data);
        static void set_swap_interval_callback(void* user_data, int interval);
        static void framebuffer_size_callback(void* user_data, int* width, int* height);
    };
}

#endif // GCANVAS_PLATFORM_ANDROID_EGL_HPP
