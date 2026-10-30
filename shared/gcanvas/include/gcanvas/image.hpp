#ifndef GCANVAS_IMAGE_HPP
#define GCANVAS_IMAGE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "gcanvas/gcanvas.hpp"

namespace gcanvas
{
    class Context;
    class Font;

    // Producer-assigned, never-reused publication identity. Identical
    // encoded content across separate A -> B -> A publications must retain
    // different generation numbers. This tag is NOT a filesystem inode or
    // hash and its authenticity is supplied by the producing owner.
    struct ImagePublicationIdentity final {
        std::string publisher_id;
        std::string asset_id;
        std::uint64_t generation = 0;
        bool operator==(const ImagePublicationIdentity& other) const {
            return publisher_id == other.publisher_id &&
                   asset_id == other.asset_id &&
                   generation == other.generation;
        }
        bool operator!=(const ImagePublicationIdentity& other) const {
            return !(*this == other);
        }
    };

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
        // Null for legacy/unverified/raw images, and after a successful
        // caller-side pixel mutation. This is producer-provided provenance.
        const ImagePublicationIdentity* source_publication() const noexcept {
            return _source_publication ? &*_source_publication : nullptr;
        }
        void set_name(std::string name);

        // Validate/stage a full candidate image. On a rejected backend
        // update the old CPU pixels and data pointer are restored before
        // propagating the exception. Native driver writes already in flight
        // are NOT rolled back; device failure must be quarantined separately.
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
        std::optional<ImagePublicationIdentity> _source_publication;

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
