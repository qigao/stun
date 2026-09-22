#ifndef GCANVAS_FONT_IMPL_GL_HPP
#define GCANVAS_FONT_IMPL_GL_HPP

#include "gcanvas/font.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace gcanvas
{
    class FontImplGl : public Font
    {
    public:
        FontImplGl(std::string file_path);
        FontImplGl(const unsigned char* buffer, size_t size);
        ~FontImplGl() override = default;

        bool _loaded = false;
        bool _uploaded = false;

        void upload();

    protected:
        std::unique_ptr<Image> create_atlas(int width, int height, int components,
                                            const unsigned char* data) override;
    };

} // namespace gcanvas

#endif // GCANVAS_FONT_IMPL_GL_HPP
