#ifndef GCANVAS_OPENGL_SHARED_INFO_HPP
#define GCANVAS_OPENGL_SHARED_INFO_HPP

#include <glad/glad.h>
#include "gcanvas/backends/opengl.hpp"

#include <string>


namespace gcanvas
{
    class OpenglSharedInfo
    {
    public:
        static int MAX_UNIFORM_BLOCK_SIZE;

        static void retain(opengl::ProcLoader loader, void* user_data);
        static void release() noexcept;

    private:
        static OpenglSharedInfo* _instance;
        static std::size_t _reference_count;

        OpenglSharedInfo(opengl::ProcLoader loader, void* user_data);
        OpenglSharedInfo(const OpenglSharedInfo&)
        {}
        ~OpenglSharedInfo();

        void load_opengl(opengl::ProcLoader loader, void* user_data);
    };

} // namespace gcanvas

#endif // GCANVAS_VULKAN_SHARED_INFO_HPP
