#include "gcanvas/backends/opengl.hpp"

#include "../gl/context_impl_opengl.hpp"
#include "../../resources.hpp"

namespace gcanvas::opengl
{
    std::unique_ptr<Context> create_context(const CreateInfo& create_info)
    {
        detail::GlCreateInfo shared{};
        shared.metrics = create_info.metrics;
        shared.resource_limits = create_info.resource_limits;
        shared.host.user_data = create_info.host.user_data;
        shared.host.get_proc_address = create_info.host.get_proc_address;
        shared.host.make_current = create_info.host.make_current;
        shared.host.swap_buffers = create_info.host.swap_buffers;
        shared.host.set_swap_interval = create_info.host.set_swap_interval;
        shared.host.framebuffer_size = create_info.host.framebuffer_size;
        shared.presentation = create_info.presentation == PresentationMode::External
            ? detail::GlPresentationMode::External
            : detail::GlPresentationMode::HostManaged;
        shared.backend = Backend::OpenGL;
        shared.vertex_shader_source = ogl_vertex_code;
        shared.fragment_shader_source = ogl_fragment_code;
        return std::make_unique<ContextImplOpengl>(shared);
    }
}
