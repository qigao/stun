#ifndef GCANVAS_BACKENDS_OPENGL_HPP
#define GCANVAS_BACKENDS_OPENGL_HPP

#include "gcanvas/context.hpp"

#include <memory>

namespace gcanvas
{
    namespace opengl
    {
        using ProcAddress = void (*)();
        using ProcLoader = ProcAddress (*)(void* user_data, const char* name);

        enum class PresentationMode
        {
            HostManaged,
            External
        };

        struct Host
        {
            void* user_data = nullptr;
            ProcLoader get_proc_address = nullptr;
            void (*make_current)(void* user_data) = nullptr;
            void (*swap_buffers)(void* user_data) = nullptr;
            void (*set_swap_interval)(void* user_data, int interval) = nullptr;
            void (*framebuffer_size)(void* user_data, int* width, int* height) = nullptr;
        };

        struct CreateInfo
        {
            CanvasMetrics metrics;
            ResourceLimits resource_limits;
            Host host;
            PresentationMode presentation = PresentationMode::HostManaged;
        };

        /**
         * Creates an OpenGL canvas using borrowed synchronous host callbacks.
         * HostManaged requires every callback and swaps in present_frame(). External
         * requires get_proc_address and framebuffer_size; the caller keeps the context
         * current, owns buffer swapping, and calls present_frame() to close each submitted
         * frame without swapping. host.user_data must outlive the returned context.
         */
        GCANVAS_API std::unique_ptr<Context> create_context(const CreateInfo& create_info);
    }
}

#endif
