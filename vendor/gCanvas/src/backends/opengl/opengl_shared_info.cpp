#include "opengl_shared_info.hpp"

#include <utility>
#include <stdexcept>

#include <glad/glad.h>

#include "gcanvas/gcanvas.hpp"
namespace gcanvas
{
    namespace
    {
        thread_local opengl::ProcLoader active_loader = nullptr;
        thread_local void* active_loader_user_data = nullptr;

        void* load_gl_proc(const char* name)
        {
            return reinterpret_cast<void*>(
                active_loader(active_loader_user_data, name));
        }
    }

    int OpenglSharedInfo::MAX_UNIFORM_BLOCK_SIZE = -1;
    OpenglSharedInfo* OpenglSharedInfo::_instance = nullptr;
    std::size_t OpenglSharedInfo::_reference_count = 0;

    OpenglSharedInfo::OpenglSharedInfo(opengl::ProcLoader loader, void* user_data)
    {
        load_opengl(loader, user_data);
    }

    OpenglSharedInfo::~OpenglSharedInfo()
    {
        
    }

    void OpenglSharedInfo::retain(opengl::ProcLoader loader, void* user_data)
    {
        if (_instance == nullptr)
        {
            _instance = new OpenglSharedInfo(loader, user_data);
        }
        ++_reference_count;
    }

    void OpenglSharedInfo::release() noexcept
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

    void OpenglSharedInfo::load_opengl(opengl::ProcLoader loader, void* user_data)
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
