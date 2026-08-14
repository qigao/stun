#include "gcanvas/svg.hpp"

#include <plutosvg.h>

#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace gcanvas
{
    namespace
    {
        struct DocumentDeleter
        {
            void operator()(plutosvg_document_t* document) const noexcept
            {
                plutosvg_document_destroy(document);
            }
        };

        struct SurfaceDeleter
        {
            void operator()(plutovg_surface_t* surface) const noexcept
            {
                plutovg_surface_destroy(surface);
            }
        };

        using DocumentHandle = std::unique_ptr<plutosvg_document_t, DocumentDeleter>;
        using SurfaceHandle = std::unique_ptr<plutovg_surface_t, SurfaceDeleter>;

        void validate_requested_dimension(int dimension, const char* name)
        {
            if (dimension == 0 || dimension < -1)
            {
                throw std::invalid_argument(std::string("gCanvas SVG ") + name +
                                            " must be positive or -1");
            }
        }
    }

    struct SvgDocument::Impl
    {
        std::string source;
        DocumentHandle document;
    };

    bool SvgBitmap::empty() const noexcept
    {
        return width <= 0 || height <= 0 || rgba.empty();
    }

    SvgDocument::SvgDocument(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

    SvgDocument::~SvgDocument() = default;
    SvgDocument::SvgDocument(SvgDocument&&) noexcept = default;
    SvgDocument& SvgDocument::operator=(SvgDocument&&) noexcept = default;

    std::unique_ptr<SvgDocument> SvgDocument::load_file(const std::string& path)
    {
        if (path.empty())
        {
            throw std::invalid_argument("gCanvas SVG path must not be empty");
        }

        auto impl = std::make_unique<Impl>();
        impl->document.reset(plutosvg_document_load_from_file(path.c_str(), -1.0f, -1.0f));
        if (!impl->document)
        {
            return nullptr;
        }
        return std::unique_ptr<SvgDocument>(new SvgDocument(std::move(impl)));
    }

    std::unique_ptr<SvgDocument> SvgDocument::load_data(std::string_view data)
    {
        if (data.empty())
        {
            throw std::invalid_argument("gCanvas SVG data must not be empty");
        }
        if (data.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        {
            throw std::length_error("gCanvas SVG data exceeds PlutoSVG input limit");
        }

        auto impl = std::make_unique<Impl>();
        impl->source.assign(data.data(), data.size());
        impl->document.reset(plutosvg_document_load_from_data(
            impl->source.data(), static_cast<int>(impl->source.size()), -1.0f, -1.0f,
            nullptr, nullptr));
        if (!impl->document)
        {
            return nullptr;
        }
        return std::unique_ptr<SvgDocument>(new SvgDocument(std::move(impl)));
    }

    float SvgDocument::intrinsic_width() const noexcept
    {
        return plutosvg_document_get_width(impl_->document.get());
    }

    float SvgDocument::intrinsic_height() const noexcept
    {
        return plutosvg_document_get_height(impl_->document.get());
    }

    SvgBitmap SvgDocument::rasterize(int width, int height) const
    {
        validate_requested_dimension(width, "width");
        validate_requested_dimension(height, "height");

        SurfaceHandle surface(plutosvg_document_render_to_surface(
            impl_->document.get(), nullptr, width, height, nullptr, nullptr, nullptr));
        if (!surface)
        {
            return {};
        }

        const int actual_width = plutovg_surface_get_width(surface.get());
        const int actual_height = plutovg_surface_get_height(surface.get());
        const int source_stride = plutovg_surface_get_stride(surface.get());
        constexpr std::size_t channels = 4;
        if (actual_width <= 0 || actual_height <= 0 || source_stride <= 0 ||
            static_cast<std::size_t>(actual_width) >
                (std::numeric_limits<std::size_t>::max)() / channels)
        {
            return {};
        }

        const std::size_t row_bytes = static_cast<std::size_t>(actual_width) * channels;
        if (static_cast<std::size_t>(source_stride) < row_bytes ||
            static_cast<std::size_t>(actual_height) >
                (std::numeric_limits<std::size_t>::max)() /
                    static_cast<std::size_t>(source_stride))
        {
            return {};
        }

        std::vector<unsigned char> converted(
            static_cast<std::size_t>(source_stride) * static_cast<std::size_t>(actual_height));
        plutovg_convert_argb_to_rgba(converted.data(), plutovg_surface_get_data(surface.get()),
                                     actual_width, actual_height, source_stride);

        SvgBitmap result;
        result.width = actual_width;
        result.height = actual_height;
        result.rgba.resize(row_bytes * static_cast<std::size_t>(actual_height));
        for (int row = 0; row < actual_height; ++row)
        {
            std::memcpy(result.rgba.data() + static_cast<std::size_t>(row) * row_bytes,
                        converted.data() + static_cast<std::size_t>(row) *
                                               static_cast<std::size_t>(source_stride),
                        row_bytes);
        }
        return result;
    }
}
