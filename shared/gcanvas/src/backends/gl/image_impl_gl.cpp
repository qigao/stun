#include "image_impl_gl.hpp"

#include <algorithm>
#include <iostream>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <exception>
#include <utility>

#include <stb_image.h>

namespace gcanvas
{
namespace GCANVAS_GL_PROFILE_NAMESPACE
{
    ImageImplGl::ImageImplGl(
        std::string source_path, const unsigned char* frozen_encoded,
        std::size_t frozen_size, int inspected_width, int inspected_height,
        ImageConfig imageConfig)
    {
        if (!frozen_encoded || frozen_size == 0 ||
            frozen_size > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
            throw std::invalid_argument("gCanvas requires bounded frozen encoded image bytes");
        if (inspected_width <= 0 || inspected_height <= 0)
            throw std::invalid_argument("gCanvas frozen image metadata dimensions are invalid");

        // Decode from the SAME frozen bytes used by stbi_info_from_memory,
        // never from source_path. The path may already have been replaced
        // and is kept only for useful diagnostics/name.
        _imageConfig = imageConfig;
        stbi_uc* decoded = stbi_load_from_memory(
            frozen_encoded, static_cast<int>(frozen_size),
            &_width, &_height, &_components, STBI_rgb_alpha);
        if (!decoded)
            throw std::runtime_error("Could not decode frozen image: " + source_path);
        try
        {
            if (_width != inspected_width || _height != inspected_height)
                throw std::logic_error(
                    "gCanvas frozen image metadata and decoder disagree");
            _components = 4;
            _name = std::move(source_path);
            assign_pixels(decoded, required_pixel_bytes());
        }
        catch (...)
        {
            stbi_image_free(decoded);
            throw;
        }
        stbi_image_free(decoded);

        if (imageConfig.mipmaps)
            _mipLevels =
                static_cast<uint32_t>(std::floor(
                    std::log2((std::max)(_width, _height)))) + 1;
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
            // No noexcept destructor can return a denied GPU deletion as an
            // exception: terminate BEFORE touching an in-flight GL texture.
            if (mutation_guard_ &&
                !mutation_guard_(mutation_user_data_,
                                 NativeGpuMutation::TextureDelete,
                                 _image, uploaded_bytes_))
                std::terminate();
            glDeleteTextures(1, &_image);
            _uploaded = false;
        }
    }

    void ImageImplGl::admit_mutation(
        NativeGpuMutation operation, GLuint name, std::size_t bytes) const
    {
        if (mutation_guard_ &&
            !mutation_guard_(mutation_user_data_, operation, name, bytes))
            throw std::logic_error(
                "gCanvas native texture upload/update denied by GPU resource owner");
    }

    void ImageImplGl::upload()
    {
        if (_data == nullptr)
            return;

        // The exact CPU-side texture byte extent is already bounded and
        // validated. Admission occurs BEFORE allocating any GL object.
        const auto bytes = required_pixel_bytes();
        admit_mutation(NativeGpuMutation::TextureUpload, 0u, bytes);
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

        
        uploaded_bytes_ = bytes;
        _uploaded = true;
    }

    void ImageImplGl::upload_update()
    {
        if (!_uploaded || !_image || _data == nullptr)
            throw std::logic_error(
                "gCanvas cannot mutate a texture that has no native upload");
        admit_mutation(NativeGpuMutation::TextureUpdate,
                       _image, uploaded_bytes_);
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
            _internal_format = GL_R8;
            break;
        case 2:
            _type = GL_RG;
            _internal_format = GL_RG8;
            break;
        case 3:
            _type = GL_RGB;
            _internal_format = GL_RGB8;
            break;
        case 4:
        default:
            _type = GL_RGBA;
            _internal_format = GL_RGBA8;
            break;
        }
    }    

} // namespace GCANVAS_GL_PROFILE_NAMESPACE
} // namespace gcanvas
