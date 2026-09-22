#include "gcanvas/backends/opengles.hpp"

#include "../gl/context_impl_gl.hpp"
#include "gles_runtime.hpp"
#include "gles_shader_source.hpp"

namespace gcanvas::opengles
{
    std::unique_ptr<Context> create_context(const CreateInfo& create_info)
    {
        const detail::GlesShaderSources& shaders = detail::gles_shader_sources();

        detail::GlCreateInfo shared{};
        shared.metrics = create_info.metrics;
        shared.resource_limits = create_info.resource_limits;
        shared.host.user_data = create_info.host.user_data;
        shared.host.make_current = create_info.host.make_current;
        shared.host.swap_buffers = create_info.host.swap_buffers;
        shared.host.set_swap_interval = create_info.host.set_swap_interval;
        shared.host.framebuffer_size = create_info.host.framebuffer_size;
        shared.presentation = create_info.presentation == PresentationMode::External
            ? detail::GlPresentationMode::External
            : detail::GlPresentationMode::HostManaged;
        shared.backend = Backend::OpenGLES;
        shared.runtime = &detail::gles_runtime();
        shared.vertex_shader_source = shaders.vertex.c_str();
        shared.fragment_shader_source = shaders.fragment.c_str();
        return std::make_unique<opengles_gl::ContextImplGl>(shared);
    }
}
