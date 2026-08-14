#ifndef GCANVAS_SVG_HPP
#define GCANVAS_SVG_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gcanvas/gcanvas.hpp"

namespace gcanvas
{
    struct GCANVAS_API SvgBitmap
    {
        int width = 0;
        int height = 0;
        std::vector<unsigned char> rgba;

        bool empty() const noexcept;
    };

    /**
     * Owns a parsed PlutoSVG document without exposing PlutoSVG types to callers.
     *
     * The load functions return nullptr for unreadable or invalid SVG input. A
     * rasterization failure returns an empty bitmap. Positive dimensions request
     * an exact output size; -1 preserves the document's intrinsic dimension.
     */
    class GCANVAS_API SvgDocument
    {
    public:
        ~SvgDocument();

        SvgDocument(const SvgDocument&) = delete;
        SvgDocument& operator=(const SvgDocument&) = delete;
        SvgDocument(SvgDocument&&) noexcept;
        SvgDocument& operator=(SvgDocument&&) noexcept;

        static std::unique_ptr<SvgDocument> load_file(const std::string& path);
        static std::unique_ptr<SvgDocument> load_data(std::string_view data);

        float intrinsic_width() const noexcept;
        float intrinsic_height() const noexcept;
        SvgBitmap rasterize(int width = -1, int height = -1) const;

    private:
        struct Impl;
        explicit SvgDocument(std::unique_ptr<Impl> impl) noexcept;

        std::unique_ptr<Impl> impl_;
    };
}

#endif // GCANVAS_SVG_HPP
