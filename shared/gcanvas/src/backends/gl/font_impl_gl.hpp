#ifndef GCANVAS_FONT_IMPL_GL_HPP
#define GCANVAS_FONT_IMPL_GL_HPP

#include "gcanvas/font.hpp"
#include "gl_api.hpp"

#include "gcanvas/context.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace gcanvas
{
namespace GCANVAS_GL_PROFILE_NAMESPACE
{
    class FontImplGl : public Font
    {
    public:
        FontImplGl(std::string file_path, Context& context);
        FontImplGl(const unsigned char* buffer, size_t size, Context& context);
        ~FontImplGl() override = default;

        bool _loaded = false;
        bool _uploaded = false;

        void upload();

    protected:
        void admit_atlas_creation(std::size_t atlas_bytes) const override;
        std::unique_ptr<Image> create_atlas(int width, int height, int components,
                                            const unsigned char* data) override;
    private:
        Context* creation_context_ = nullptr;
    };

} // namespace GCANVAS_GL_PROFILE_NAMESPACE
} // namespace gcanvas

#endif // GCANVAS_FONT_IMPL_GL_HPP
