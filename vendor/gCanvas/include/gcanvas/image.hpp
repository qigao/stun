#ifndef GCANVAS_IMAGE_HPP
#define GCANVAS_IMAGE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "gcanvas/gcanvas.hpp"

namespace gcanvas
{
    class Context;
    class Font;
    enum ImageFiltering
    {
        NEAREST,
        LINEAR
    };
    struct ImageConfig
    {
        bool mipmaps = false;
        ImageFiltering imagefiltering = LINEAR;
    };

    class GCANVAS_API Image
    {
    public:
        virtual ~Image() = default;
        Image(const Image&) = delete;
        Image& operator=(const Image&) = delete;

        int get_width() const noexcept;
        int get_height() const noexcept;
        int get_channels() const noexcept;
        const std::vector<std::uint8_t>& pixels() const noexcept { return _pixels; }
        void set_name(std::string name);

        void update_data(const unsigned char* data, std::size_t size);
        // Writes the retained CPU-side pixels; GPU-side updates must first be synchronized by the
        // owning backend.
        void write_to_file(std::string file_path);

    protected:
        int _width = -1;
        int _height = -1;
        int _components = -1;
        ImageConfig _imageConfig = {};

        std::string _name;

        std::vector<std::uint8_t> _pixels;
        unsigned char* _data = nullptr;
        Context* _owner = nullptr;

        Image() = default;
        void assign_pixels(const unsigned char* data, std::size_t size);
        std::size_t required_pixel_bytes() const;

    private:
        friend class Context;
        friend class Font;
    };

} // namespace gcanvas

#endif // GCANVAS_IMAGE_HPP
