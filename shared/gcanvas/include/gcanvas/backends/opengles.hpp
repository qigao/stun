#ifndef GCANVAS_BACKENDS_OPENGLES_HPP
#define GCANVAS_BACKENDS_OPENGLES_HPP

#include "gcanvas/context.hpp"

#include <memory>

namespace gcanvas
{
    namespace opengles
    {
        enum class PresentationMode
        {
            HostManaged,
            External
        };

        struct Host
        {
            void* user_data = nullptr;
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
         * Creates an OpenGL ES 3 canvas using a host-owned current GLES context.
         * HostManaged owns make-current, swap interval, and buffer swapping through
         * synchronous callbacks. External requires only framebuffer_size; the caller
         * keeps the GLES context current, owns presentation, and still calls
         * present_frame() to close submitted frame state.
         */
        GCANVAS_API std::unique_ptr<Context> create_context(const CreateInfo& create_info);
    }
}

#endif // GCANVAS_BACKENDS_OPENGLES_HPP
