#include "desktop_gl_runtime.hpp"

#include "../gl/gl_api.hpp"

#include <cstddef>
#include <stdexcept>

namespace gcanvas::detail
{
    namespace
    {
        thread_local GlProcLoader active_loader = nullptr;
        thread_local void* active_loader_user_data = nullptr;
        std::size_t reference_count = 0;
        int max_uniform_block_size_value = -1;

        void* load_gl_proc(const char* name)
        {
            return reinterpret_cast<void*>(
                active_loader(active_loader_user_data, name));
        }

        void retain_desktop_gl(GlProcLoader loader, void* user_data)
        {
            if (reference_count == 0)
            {
                if (loader == nullptr)
                    throw std::invalid_argument("desktop OpenGL requires a procedure loader");

                active_loader = loader;
                active_loader_user_data = user_data;
                const int loaded = gladLoadGLLoader(load_gl_proc);
                active_loader = nullptr;
                active_loader_user_data = nullptr;
                if (loaded == 0)
                    throw std::runtime_error("Could not load the OpenGL context");

                glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &max_uniform_block_size_value);
                if (max_uniform_block_size_value <= 0)
                    throw std::runtime_error("OpenGL uniform block capacity is unavailable");
            }
            ++reference_count;
        }

        void release_desktop_gl() noexcept
        {
            if (reference_count == 0)
                return;
            --reference_count;
            if (reference_count == 0)
                max_uniform_block_size_value = -1;
        }

        int desktop_gl_max_uniform_block_size()
        {
            return max_uniform_block_size_value;
        }
    }

    const GlRuntime& desktop_gl_runtime()
    {
        static const GlRuntime runtime{
            true,
            retain_desktop_gl,
            release_desktop_gl,
            desktop_gl_max_uniform_block_size,
        };
        return runtime;
    }
}
