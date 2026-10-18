#include "gcanvas/svg.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace
{
    constexpr const char* svg =
        "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='4'>"
        "<rect width='8' height='4' fill='#ff0000'/></svg>";
}

int main()
{
    auto document = gcanvas::SvgDocument::load_data(svg);
    if (!document || std::fabs(document->intrinsic_width() - 8.0f) > 0.01f ||
        std::fabs(document->intrinsic_height() - 4.0f) > 0.01f)
    {
        return 1;
    }

    const gcanvas::SvgBitmap intrinsic = document->rasterize();
    if (intrinsic.empty() || intrinsic.width != 8 || intrinsic.height != 4 ||
        intrinsic.rgba.size() != std::size_t{8 * 4 * 4})
    {
        return 2;
    }
    if (intrinsic.rgba[0] < 250 || intrinsic.rgba[1] > 5 || intrinsic.rgba[2] > 5 ||
        intrinsic.rgba[3] < 250)
    {
        return 3;
    }

    const gcanvas::SvgBitmap scaled = document->rasterize(16, 12);
    if (scaled.empty() || scaled.width != 16 || scaled.height != 12)
    {
        return 4;
    }

    if (gcanvas::SvgDocument::load_data("not an svg document") != nullptr)
    {
        return 5;
    }

    try
    {
        (void)document->rasterize(0, 4);
        return 6;
    }
    catch (const std::invalid_argument&)
    {
    }

    return 0;
}
