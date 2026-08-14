#ifndef GCANVAS_FONT_IMPL_OPENGL_HPP
#define GCANVAS_FONT_IMPL_OPENGL_HPP

#include "gcanvas/font.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace gcanvas
{
    class FontImplOpengl : public Font
    {
    public:
        FontImplOpengl(std::string file_path);
        FontImplOpengl(const unsigned char* buffer, size_t size);
        ~FontImplOpengl() override = default;

        bool _loaded = false;
        bool _uploaded = false;

        void upload();

    protected:
        std::unique_ptr<Image> create_atlas(int width, int height, int components,
                                            const unsigned char* data) override;
    };

} // namespace gcanvas

#endif // GCANVAS_FONT_IMPL_OPENGL_HPP
