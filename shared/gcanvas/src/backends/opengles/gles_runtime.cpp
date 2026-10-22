#include "gles_runtime.hpp"

#include "../gl/gl_api.hpp"

#include <cstring>
#include <stdexcept>

namespace gcanvas::detail
{
    namespace
    {
        void retain_gles(GlProcLoader, void*)
        {
            const auto* version =
                reinterpret_cast<const char*>(glGetString(GL_VERSION));
            if (version == nullptr || std::strstr(version, "OpenGL ES") == nullptr)
                throw std::runtime_error("gCanvas OpenGLES requires an OpenGL ES context");

            GLint major = 0;
            glGetIntegerv(GL_MAJOR_VERSION, &major);
            if (major < 3)
                throw std::runtime_error("gCanvas OpenGLES requires OpenGL ES 3.0 or newer");
        }

        void release_gles() noexcept
        {
        }

        int gles_max_uniform_block_size()
        {
            GLint value = 0;
            glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &value);
            return value;
        }
    }

    const GlRuntime& gles_runtime()
    {
        static const GlRuntime runtime{
            false,
            retain_gles,
            release_gles,
            gles_max_uniform_block_size,
        };
        return runtime;
    }
}
