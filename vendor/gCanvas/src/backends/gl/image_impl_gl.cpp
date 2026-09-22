#include "image_impl_gl.hpp"

#include <algorithm>
#include <iostream>
#include <cstring>
#include <cmath>
#include <stdexcept>

#include <stb_image.h>

namespace gcanvas
{
namespace GCANVAS_GL_PROFILE_NAMESPACE
{
    ImageImplGl::ImageImplGl(std::string file_path, ImageConfig imageConfig)
    {
        _imageConfig = imageConfig;
        stbi_uc* data =
            stbi_load(file_path.c_str(), &_width, &_height, &_components, STBI_rgb_alpha);
        if (data != nullptr)
        {
            _components = 4;
            _name = file_path;
            try
            {
                assign_pixels(data, required_pixel_bytes());
            }
            catch (...)
            {
                stbi_image_free(data);
                throw;
            }
            stbi_image_free(data);
            

            if (imageConfig.mipmaps)
            {
                _mipLevels =
                    static_cast<uint32_t>(std::floor(std::log2(std::max(_width, _height)))) + 1;
            }

        }
        else
        {
            throw std::runtime_error("Could not load image: " + file_path);
        }

        init_format();
    }

    ImageImplGl::ImageImplGl(int width, int height, int components,
                                     const unsigned char* data,
                                     std::size_t size, ImageConfig imageConfig)
    {
        _imageConfig = imageConfig;
        _width = width;
        _height = height;
        _components = components;
        _name = "noname_" + std::to_string(rand() % 10000);
        assign_pixels(data, size);

        if (imageConfig.mipmaps)
        {
            _mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
        }

        init_format();
    }

    ImageImplGl::~ImageImplGl()
    {        
        if (_uploaded)
        {               
            glDeleteTextures(1, &_image);
            _uploaded = false;
        }
    }

    void ImageImplGl::upload()
    {
        if (_data == nullptr)
            return;

        glGenTextures(1, &_image);
        glBindTexture(GL_TEXTURE_2D, _image);

        glTexImage2D(GL_TEXTURE_2D, 0, _internal_format, _width, _height, 0, _type,
                     GL_UNSIGNED_BYTE, _data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        
        if (_mipLevels > 1)
        {
            switch (_imageConfig.imagefiltering)
            {
            case ImageFiltering::NEAREST:
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                break;
            case ImageFiltering::LINEAR:
            default:
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;
            }
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        else
        {
            switch (_imageConfig.imagefiltering)
            {
            case ImageFiltering::NEAREST:
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                break;
            case ImageFiltering::LINEAR:
            default:
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;
            }
        }      

        
        _uploaded = true;
    }

    void ImageImplGl::upload_update()
    {
        glBindTexture(GL_TEXTURE_2D, _image);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height, _type, GL_UNSIGNED_BYTE, _data);
    }

    void ImageImplGl::bind(GLuint texture_unit)
    {
        glActiveTexture(GL_TEXTURE0 + texture_unit);
        glBindTexture(GL_TEXTURE_2D, _image);
    }

    void ImageImplGl::init_format()
    {
        switch (_components)
        {
        case 1:
            _type = GL_RED;
            _internal_format = GL_R16F;
            break;
        case 2:
            _type = GL_RG;
            _internal_format = GL_RG16F;
            break;
        case 3:
            _type = GL_RGB;
            _internal_format = GL_RGB16F;
            break;
        case 4:
        default:
            _type = GL_RGBA;
            _internal_format = GL_RGBA16F;
            break;
        }
    }    

} // namespace GCANVAS_GL_PROFILE_NAMESPACE
} // namespace gcanvas
