#include "gcanvas/image.hpp"
#include "gcanvas/context.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <iostream>
#include <limits>
#include <stdexcept>

namespace gcanvas
{
    int Image::get_width() const noexcept
    {
        return _width;
    }

    int Image::get_height() const noexcept
    {
        return _height;
    }

    int Image::get_channels() const noexcept
    {
        return _components;
    }

    void Image::set_name(std::string name)
    {
        _name = name;
    }

    std::size_t Image::required_pixel_bytes() const
    {
        if (_width <= 0 || _height <= 0 || _components <= 0 || _components > 4)
        {
            throw std::invalid_argument("invalid gCanvas image dimensions or components");
        }
        const auto width = static_cast<std::size_t>(_width);
        const auto height = static_cast<std::size_t>(_height);
        const auto components = static_cast<std::size_t>(_components);
        if (width > std::numeric_limits<std::size_t>::max() / height ||
            width * height > std::numeric_limits<std::size_t>::max() / components)
        {
            throw std::overflow_error("gCanvas image byte count overflow");
        }
        return width * height * components;
    }

    void Image::assign_pixels(const unsigned char* data, std::size_t size)
    {
        const std::size_t required = required_pixel_bytes();
        if (data == nullptr || size != required)
        {
            throw std::invalid_argument("gCanvas image pixel span has the wrong size");
        }
        _pixels.assign(data, data + size);
        _data = _pixels.data();
    }

    void Image::update_data(const unsigned char* data, std::size_t size)
    {
        assign_pixels(data, size);
        if (_owner != nullptr)
        {
            _owner->image_pixels_updated(*this);
        }
    }

    void Image::write_to_file(std::string file_path)
    {
        if (stbi_write_png(file_path.c_str(), _width, _height, _components, _data, 0) == 0)
        {
            std::cerr << "Failed to write image to " << file_path << std::endl;
        }
    }

} // namespace gcanvas
