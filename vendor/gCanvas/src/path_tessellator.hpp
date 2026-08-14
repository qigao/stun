#ifndef GCANVAS_PATH_TESSELLATOR_HPP
#define GCANVAS_PATH_TESSELLATOR_HPP

#include "gcanvas/paint.hpp"
#include "gcanvas/path.hpp"
#include "gcanvas/transform.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace gcanvas::detail
{
    struct MeshPoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct MeshQuad
    {
        std::array<MeshPoint, 4> vertices{};
    };

    enum class PathMaskMode
    {
        EvenOdd,
        Union
    };

    struct PathGeometry
    {
        std::vector<MeshQuad> mask_quads;
        MeshQuad paint_quad;
        MeshPoint paint_uv_min{0.0f, 0.0f};
        MeshPoint paint_uv_max{1.0f, 1.0f};
        PathMaskMode mask_mode = PathMaskMode::EvenOdd;
        color solid_color{0, 0, 0, 0};
        Image* paint_image = nullptr;
        Paint raster_paint;
        MeshPoint raster_minimum{};
        MeshPoint raster_maximum{};
        std::vector<std::uint8_t> paint_pixels;
    };

    PathGeometry tessellate_path(const Path& path, const Paint& paint,
                                 const Transform& device_transform, float line_width,
                                 std::size_t maximum_quads, std::size_t paint_texture_size);

    std::vector<std::uint8_t> rasterize_path_paint(const Paint& paint,
                                                   MeshPoint minimum, MeshPoint maximum,
                                                   std::size_t paint_texture_size);
}

#endif
