#ifndef GCANVAS_IMAGE_IMPL_GL_HPP
#define GCANVAS_IMAGE_IMPL_GL_HPP

#include "gcanvas/image.hpp"

#include <map>
#include <string>
#include "gl_api.hpp"


namespace gcanvas
{
    class ImageImplGl : public Image
    {
    public:
        ImageImplGl(std::string file_path, ImageConfig imageConfig);
        ImageImplGl(int width, int height, int components, const unsigned char* data,
                        std::size_t size, ImageConfig imageConfig);
        ~ImageImplGl();

        GLuint _image;
        int _sampler_index = -1;
        uint32_t _mipLevels = 1;

        GLenum _type;
        GLint _internal_format;

        bool _uploaded = false;

        void upload();
        void upload_update();
        void bind(GLuint texture_unit);

    private:
        void init_format();
    };

} // namespace gcanvas

#endif // GCANVAS_IMAGE_IMPL_GL_HPP
