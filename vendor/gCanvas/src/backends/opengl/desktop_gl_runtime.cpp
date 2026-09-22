#include "../gl/gl_shared_info.hpp"

#include <utility>
#include <stdexcept>

#include <glad/glad.h>

#include "gcanvas/gcanvas.hpp"
namespace gcanvas
{
    namespace
    {
        thread_local detail::GlProcLoader active_loader = nullptr;
        thread_local void* active_loader_user_data = nullptr;

        void* load_gl_proc(const char* name)
        {
            return reinterpret_cast<void*>(
                active_loader(active_loader_user_data, name));
        }
    }

    int GlSharedInfo::MAX_UNIFORM_BLOCK_SIZE = -1;
    GlSharedInfo* GlSharedInfo::_instance = nullptr;
    std::size_t GlSharedInfo::_reference_count = 0;

    GlSharedInfo::GlSharedInfo(detail::GlProcLoader loader, void* user_data)
    {
        load_api(loader, user_data);
    }

    GlSharedInfo::~GlSharedInfo()
    {
        
    }

    void GlSharedInfo::retain(detail::GlProcLoader loader, void* user_data)
    {
        if (_instance == nullptr)
        {
            _instance = new GlSharedInfo(loader, user_data);
        }
        ++_reference_count;
    }

    void GlSharedInfo::release() noexcept
    {
        if (_reference_count == 0)
        {
            return;
        }
        --_reference_count;
        if (_reference_count == 0)
        {
            delete _instance;
            _instance = nullptr;
        }
    }

    void GlSharedInfo::load_api(detail::GlProcLoader loader, void* user_data)
    {
        // --------------- Load Opengl ---------------

        active_loader = loader;
        active_loader_user_data = user_data;
        const int loaded = gladLoadGLLoader(load_gl_proc);
        active_loader = nullptr;
        active_loader_user_data = nullptr;
        if (loaded == 0)
        {
            throw std::runtime_error("Could not load the OpenGL context");
        }

        glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &MAX_UNIFORM_BLOCK_SIZE);

    }
} // namespace gcanvas
