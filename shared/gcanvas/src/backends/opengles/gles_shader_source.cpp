#include "gles_shader_source.hpp"

#include "../../resources.hpp"
#include "../gl/gl_api.hpp"

#include <stdexcept>
#include <string>

namespace gcanvas::detail
{
    namespace
    {
        void replace_once(std::string& source, const std::string& from,
                          const std::string& to)
        {
            const std::size_t position = source.find(from);
            if (position == std::string::npos)
                throw std::logic_error("gCanvas GLES shader source contract changed");
            source.replace(position, from.size(), to);
        }

        GlesShaderSources build_sources()
        {
            GlesShaderSources result{ogl_vertex_code, ogl_fragment_code};
            const std::string preamble =
                "#version 300 es\n"
                "precision highp float;\n"
                "precision highp int;\n";

            replace_once(result.vertex, "#version 410 core\n", preamble);
            replace_once(result.fragment, "#version 410 core\n", preamble);

            const std::string desktop_payload = "RoundedRectData payload[128];";
            const std::string gles_payload =
                "RoundedRectData payload[" +
                std::to_string(GCANVAS_GL_SHADER_BATCH_CAPACITY) + "];";
            replace_once(result.vertex, desktop_payload, gles_payload);
            replace_once(result.fragment, desktop_payload, gles_payload);

            replace_once(result.vertex,
                         "out gl_PerVertex{\n"
                         "       vec4 gl_Position;\n"
                         "};\n\n",
                         "");

            replace_once(result.vertex,
                         "layout(location = 0) out vec2 uv_varying;",
                         "out vec2 uv_varying;");
            replace_once(result.vertex,
                         "layout(location = 1) out vec2 uv_tex_varying;",
                         "out vec2 uv_tex_varying;");
            replace_once(result.vertex,
                         "layout(location = 2) flat out int instance_index;",
                         "flat out int instance_index;");

            replace_once(result.fragment,
                         "layout(location = 0) in vec2 uv_varying;",
                         "in vec2 uv_varying;");
            replace_once(result.fragment,
                         "layout(location = 1) in vec2 uv_tex_varying;",
                         "in vec2 uv_tex_varying;");
            replace_once(result.fragment,
                         "layout(location = 2) flat in int instance_index;",
                         "flat in int instance_index;");

            return result;
        }
    }

    const GlesShaderSources& gles_shader_sources()
    {
        static const GlesShaderSources sources = build_sources();
        return sources;
    }
}
