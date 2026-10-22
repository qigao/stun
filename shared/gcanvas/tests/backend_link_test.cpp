#ifdef GCANVAS_TEST_OPENGL
#include "gcanvas/backends/opengl.hpp"
#endif

#include <stdexcept>
#ifdef GCANVAS_TEST_VULKAN
#include "gcanvas/backends/vulkan.hpp"
#endif

int main()
{
    bool factories_available = true;
#ifdef GCANVAS_TEST_OPENGL
    auto volatile opengl_factory = &gcanvas::opengl::create_context;
    factories_available = factories_available && opengl_factory != nullptr;
    try
    {
        auto context = gcanvas::opengl::create_context({});
        factories_available = false;
    }
    catch (const std::invalid_argument&)
    {
    }
#endif
#ifdef GCANVAS_TEST_VULKAN
    auto volatile vulkan_factory = &gcanvas::vulkan::create_context;
    factories_available = factories_available && vulkan_factory != nullptr;
    try
    {
        auto context = gcanvas::vulkan::create_context({});
        factories_available = false;
    }
    catch (const std::invalid_argument&)
    {
    }
#endif
    return factories_available ? 0 : 1;
}
