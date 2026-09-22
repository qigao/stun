#include "gcanvas/backends/opengles.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
    constexpr int width = 32;
    constexpr int height = 32;

    struct HostState
    {
        EGLDisplay display = EGL_NO_DISPLAY;
        EGLSurface surface = EGL_NO_SURFACE;
        EGLContext context = EGL_NO_CONTEXT;
    };

    void make_current(void* user_data)
    {
        auto& host = *static_cast<HostState*>(user_data);
        if (eglMakeCurrent(host.display, host.surface, host.surface, host.context) != EGL_TRUE)
            throw std::runtime_error("eglMakeCurrent failed");
    }

    void swap_buffers(void* user_data)
    {
        auto& host = *static_cast<HostState*>(user_data);
        if (eglSwapBuffers(host.display, host.surface) != EGL_TRUE)
            throw std::runtime_error("eglSwapBuffers failed");
    }

    void set_swap_interval(void* user_data, int interval)
    {
        auto& host = *static_cast<HostState*>(user_data);
        if (eglSwapInterval(host.display, interval) != EGL_TRUE)
            throw std::runtime_error("eglSwapInterval failed");
    }

    void framebuffer_size(void*, int* out_width, int* out_height)
    {
        *out_width = width;
        *out_height = height;
    }

    void destroy_egl(HostState& host)
    {
        if (host.display == EGL_NO_DISPLAY)
            return;
        eglMakeCurrent(host.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (host.context != EGL_NO_CONTEXT)
            eglDestroyContext(host.display, host.context);
        if (host.surface != EGL_NO_SURFACE)
            eglDestroySurface(host.display, host.surface);
        eglTerminate(host.display);
        host = {};
    }
}

int main()
{
    HostState host{};
    host.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (host.display == EGL_NO_DISPLAY)
    {
        std::cerr << "eglGetDisplay failed\n";
        return 1;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (eglInitialize(host.display, &major, &minor) != EGL_TRUE)
    {
        std::cerr << "eglInitialize failed\n";
        destroy_egl(host);
        return 2;
    }
    if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE)
    {
        std::cerr << "eglBindAPI failed\n";
        destroy_egl(host);
        return 3;
    }

#if defined(EGL_OPENGL_ES3_BIT)
    constexpr EGLint es3_renderable_bit = EGL_OPENGL_ES3_BIT;
#else
    constexpr EGLint es3_renderable_bit = EGL_OPENGL_ES3_BIT_KHR;
#endif

    const EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, es3_renderable_bit,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE,
    };

    EGLConfig config = nullptr;
    EGLint config_count = 0;
    if (eglChooseConfig(host.display, config_attributes, &config, 1, &config_count) != EGL_TRUE ||
        config_count != 1)
    {
        std::cerr << "no GLES3 RGBA8/stencil8 EGL config\n";
        destroy_egl(host);
        return 4;
    }

    const EGLint pbuffer_attributes[] = {
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_NONE,
    };
    host.surface = eglCreatePbufferSurface(host.display, config, pbuffer_attributes);
    if (host.surface == EGL_NO_SURFACE)
    {
        std::cerr << "eglCreatePbufferSurface failed\n";
        destroy_egl(host);
        return 5;
    }

    const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE,
    };
    host.context =
        eglCreateContext(host.display, config, EGL_NO_CONTEXT, context_attributes);
    if (host.context == EGL_NO_CONTEXT)
    {
        std::cerr << "eglCreateContext GLES3 failed\n";
        destroy_egl(host);
        return 6;
    }

    try
    {
        make_current(&host);

        gcanvas::opengles::CreateInfo create_info{};
        create_info.metrics.width = width;
        create_info.metrics.height = height;
        create_info.host = {
            &host,
            make_current,
            swap_buffers,
            set_swap_interval,
            framebuffer_size,
        };

        std::unique_ptr<gcanvas::Context> canvas =
            gcanvas::opengles::create_context(create_info);
        if (!canvas || canvas->backend() != gcanvas::Backend::OpenGLES)
            throw std::runtime_error("OpenGLES backend identity mismatch");

        canvas->set_vsync(false);
        canvas->set_clear_color(gcanvas::color(0, 0, 0, 255));
        canvas->set_fill_color(gcanvas::color(255, 0, 0, 255));
        canvas->fill_rect(0.0f, 0.0f, static_cast<float>(width),
                          static_cast<float>(height));
        canvas->draw_frame();

        const std::vector<std::uint8_t> pixels = canvas->read_pixels();
        const std::size_t center =
            (static_cast<std::size_t>(height / 2) * width + width / 2) * 4U;
        if (pixels.size() != static_cast<std::size_t>(width * height * 4) ||
            pixels[center] < 200U || pixels[center + 1U] > 32U ||
            pixels[center + 2U] > 32U || pixels[center + 3U] < 200U)
            throw std::runtime_error("OpenGLES pixel readback mismatch");

        canvas->present_frame();
        canvas.reset();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        destroy_egl(host);
        return 7;
    }

    destroy_egl(host);
    return 0;
}
