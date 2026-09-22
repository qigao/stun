#ifndef GCANVAS_GL_BACKEND_HPP
#define GCANVAS_GL_BACKEND_HPP

#include "gcanvas/context.hpp"

namespace gcanvas::detail
{
    using GlProcAddress = void (*)();
    using GlProcLoader = GlProcAddress (*)(void* user_data, const char* name);

    enum class GlPresentationMode
    {
        HostManaged,
        External
    };

    struct GlHost
    {
        void* user_data = nullptr;
        GlProcLoader get_proc_address = nullptr;
        void (*make_current)(void* user_data) = nullptr;
        void (*swap_buffers)(void* user_data) = nullptr;
        void (*set_swap_interval)(void* user_data, int interval) = nullptr;
        void (*framebuffer_size)(void* user_data, int* width, int* height) = nullptr;
    };

    struct GlCreateInfo
    {
        CanvasMetrics metrics;
        ResourceLimits resource_limits;
        GlHost host;
        GlPresentationMode presentation = GlPresentationMode::HostManaged;
        Backend backend = Backend::OpenGL;
        const char* vertex_shader_source = nullptr;
        const char* fragment_shader_source = nullptr;
    };
}

#endif // GCANVAS_GL_BACKEND_HPP
