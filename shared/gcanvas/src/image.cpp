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
        // Stage and validate the complete candidate BEFORE modifying the
        // retained bytes or their data pointer. This also supports input
        // aliasing the current Image::pixels() buffer.
        const std::size_t required = required_pixel_bytes();
        if (data == nullptr || size != required)
            throw std::invalid_argument("gCanvas image pixel span has the wrong size");
        // Must qualify old retained CPU bytes + this extra candidate +
        // actual uploaded native texture base bytes before allocating the
        // candidate or changing either CPU/GPU state.
        if (_owner != nullptr)
            (void)_owner->image_update_peak_usage(*this, required);
        std::vector<std::uint8_t> candidate(data, data + size);

        // The existing backend's update_image() reads Image::_data. Temporarily
        // publish the candidate to that SAME original Context, and make CPU
        // state strongly exception-safe if its real native guard rejects the
        // driver operation before glTexSubImage2D is invoked. No second
        // texture, GL renderer, callback protocol or fallback is introduced.
        _pixels.swap(candidate);
        _data = _pixels.data();
        try
        {
            if (_owner != nullptr)
                _owner->image_pixels_updated(*this);
        }
        catch (...)
        {
            // Both swaps are noexcept; the original byte array (now in
            // candidate) and its owning data pointer are fully restored.
            // A native GL error *after* a partial driver write would still
            // require device quarantine: CPU rollback cannot undo GPU work.
            _pixels.swap(candidate);
            _data = _pixels.data();
            throw;
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
