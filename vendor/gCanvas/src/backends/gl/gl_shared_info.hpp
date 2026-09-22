#ifndef GCANVAS_GL_SHARED_INFO_HPP
#define GCANVAS_GL_SHARED_INFO_HPP

#include "gl_api.hpp"
#include "gl_backend.hpp"

#include <string>


namespace gcanvas
{
    class GlSharedInfo
    {
    public:
        static int MAX_UNIFORM_BLOCK_SIZE;

        static void retain(detail::GlProcLoader loader, void* user_data);
        static void release() noexcept;

    private:
        static GlSharedInfo* _instance;
        static std::size_t _reference_count;

        GlSharedInfo(detail::GlProcLoader loader, void* user_data);
        GlSharedInfo(const GlSharedInfo&)
        {}
        ~GlSharedInfo();

        void load_opengl(detail::GlProcLoader loader, void* user_data);
    };

} // namespace gcanvas

#endif // GCANVAS_GL_SHARED_INFO_HPP
