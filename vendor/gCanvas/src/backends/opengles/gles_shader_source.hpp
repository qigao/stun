#ifndef GCANVAS_GLES_SHADER_SOURCE_HPP
#define GCANVAS_GLES_SHADER_SOURCE_HPP

#include <string>

namespace gcanvas::detail
{
    struct GlesShaderSources
    {
        std::string vertex;
        std::string fragment;
    };

    const GlesShaderSources& gles_shader_sources();
}

#endif // GCANVAS_GLES_SHADER_SOURCE_HPP
