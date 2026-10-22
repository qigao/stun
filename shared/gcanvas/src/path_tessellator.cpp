#include "path_tessellator.hpp"

#include "gcanvas/image.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace gcanvas::detail
{
    namespace
    {
        constexpr float curve_tolerance = 0.25f;
        constexpr int maximum_curve_depth = 10;
        constexpr std::size_t maximum_flattened_points = Path::max_commands * 8U;
        constexpr int round_join_segments = 12;
        constexpr float two_pi = 6.28318530717958647692f;

        struct Contour
        {
            std::vector<MeshPoint> points;
            bool closed = false;
        };

        struct MeshBounds
        {
            float minimum_x = std::numeric_limits<float>::max();
            float minimum_y = std::numeric_limits<float>::max();
            float maximum_x = std::numeric_limits<float>::lowest();
            float maximum_y = std::numeric_limits<float>::lowest();
        };

        void update_bounds(MeshPoint point, MeshBounds& bounds)
        {
            bounds.minimum_x = std::min(bounds.minimum_x, point.x);
            bounds.minimum_y = std::min(bounds.minimum_y, point.y);
            bounds.maximum_x = std::max(bounds.maximum_x, point.x);
            bounds.maximum_y = std::max(bounds.maximum_y, point.y);
        }

        void append_point(Contour& contour, MeshPoint point, std::size_t& total)
        {
            if (total >= maximum_flattened_points)
                throw std::length_error("gCanvas flattened path point limit reached");
            if (!contour.points.empty())
            {
                const MeshPoint& last = contour.points.back();
                if (std::fabs(last.x - point.x) < 1.0e-6f &&
                    std::fabs(last.y - point.y) < 1.0e-6f)
                    return;
            }
            contour.points.push_back(point);
            ++total;
        }

        MeshPoint mapped(const Transform& transform, float x, float y)
        {
            MeshPoint point;
            transform.map(x, y, point.x, point.y);
            return point;
        }

        void flatten_cubic(Contour& contour, MeshPoint p0, MeshPoint p1, MeshPoint p2,
                           MeshPoint p3, int depth, std::size_t& total)
        {
            const float dx = p3.x - p0.x;
            const float dy = p3.y - p0.y;
            const float d2 = std::fabs((p1.x - p3.x) * dy - (p1.y - p3.y) * dx);
            const float d3 = std::fabs((p2.x - p3.x) * dy - (p2.y - p3.y) * dx);
            if (depth >= maximum_curve_depth ||
                (d2 + d3) * (d2 + d3) <=
                    curve_tolerance * curve_tolerance * (dx * dx + dy * dy))
            {
                append_point(contour, p3, total);
                return;
            }
            const MeshPoint p01{(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
            const MeshPoint p12{(p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f};
            const MeshPoint p23{(p2.x + p3.x) * 0.5f, (p2.y + p3.y) * 0.5f};
            const MeshPoint p012{(p01.x + p12.x) * 0.5f, (p01.y + p12.y) * 0.5f};
            const MeshPoint p123{(p12.x + p23.x) * 0.5f, (p12.y + p23.y) * 0.5f};
            const MeshPoint midpoint{(p012.x + p123.x) * 0.5f,
                                     (p012.y + p123.y) * 0.5f};
            flatten_cubic(contour, p0, p01, p012, midpoint, depth + 1, total);
            flatten_cubic(contour, midpoint, p123, p23, p3, depth + 1, total);
        }

        std::vector<Contour> flatten(const Path& path, const Transform& transform)
        {
            std::vector<Contour> contours;
            std::size_t total = 0;
            MeshPoint current{};
            bool has_current = false;
            for (const Path::Command& command : path.commands())
            {
                switch (command.type)
                {
                case Path::CommandType::MoveTo:
                    contours.emplace_back();
                    current = mapped(transform, command.values[0], command.values[1]);
                    append_point(contours.back(), current, total);
                    has_current = true;
                    break;
                case Path::CommandType::LineTo:
                    if (!has_current)
                        throw std::logic_error("gCanvas tessellator lost current point");
                    if (contours.back().closed)
                    {
                        contours.emplace_back();
                        append_point(contours.back(), current, total);
                    }
                    current = mapped(transform, command.values[0], command.values[1]);
                    append_point(contours.back(), current, total);
                    break;
                case Path::CommandType::CubicTo:
                    if (!has_current)
                        throw std::logic_error("gCanvas tessellator lost current point");
                    {
                        if (contours.back().closed)
                        {
                            contours.emplace_back();
                            append_point(contours.back(), current, total);
                        }
                        const MeshPoint p1 =
                            mapped(transform, command.values[0], command.values[1]);
                        const MeshPoint p2 =
                            mapped(transform, command.values[2], command.values[3]);
                        const MeshPoint p3 =
                            mapped(transform, command.values[4], command.values[5]);
                        flatten_cubic(contours.back(), current, p1, p2, p3, 0, total);
                        current = p3;
                    }
                    break;
                case Path::CommandType::Close:
                    if (!has_current)
                        throw std::logic_error("gCanvas tessellator lost subpath");
                    contours.back().closed = true;
                    current = contours.back().points.front();
                    break;
                }
            }
            return contours;
        }

        MeshQuad triangle(MeshPoint first, MeshPoint second, MeshPoint third)
        {
            return MeshQuad{{first, second, third, third}};
        }

        void append_quad(PathGeometry& geometry, MeshQuad quad, std::size_t maximum_quads,
                         MeshBounds& bounds)
        {
            if (geometry.mask_quads.size() >= maximum_quads)
                throw std::length_error("gCanvas path mesh quad limit reached");
            geometry.mask_quads.push_back(quad);
            for (MeshPoint point : quad.vertices)
                update_bounds(point, bounds);
        }

        void append_fill_geometry(PathGeometry& geometry, const std::vector<Contour>& contours,
                                  std::size_t maximum_quads, MeshBounds& bounds)
        {
            for (const Contour& contour : contours)
            {
                if (contour.points.size() < 3U)
                    continue;
                const MeshPoint origin = contour.points.front();
                for (std::size_t index = 1; index + 1U < contour.points.size(); ++index)
                    append_quad(geometry,
                                triangle(origin, contour.points[index],
                                         contour.points[index + 1U]),
                                maximum_quads, bounds);
            }
        }

        void append_round_join(PathGeometry& geometry, MeshPoint center, float radius,
                               std::size_t maximum_quads, MeshBounds& bounds)
        {
            static const std::array<MeshPoint, round_join_segments + 1> unit_circle = [] {
                std::array<MeshPoint, round_join_segments + 1> points{};
                for (int segment = 0; segment <= round_join_segments; ++segment)
                {
                    const float angle = two_pi * static_cast<float>(segment) /
                                        static_cast<float>(round_join_segments);
                    points[static_cast<std::size_t>(segment)] =
                        MeshPoint{std::cos(angle), std::sin(angle)};
                }
                return points;
            }();
            for (int segment = 0; segment < round_join_segments; ++segment)
            {
                const MeshPoint first_unit = unit_circle[static_cast<std::size_t>(segment)];
                const MeshPoint second_unit =
                    unit_circle[static_cast<std::size_t>(segment + 1)];
                const MeshPoint first{center.x + first_unit.x * radius,
                                      center.y + first_unit.y * radius};
                const MeshPoint second{center.x + second_unit.x * radius,
                                       center.y + second_unit.y * radius};
                append_quad(geometry, triangle(center, first, second), maximum_quads, bounds);
            }
        }

        void append_stroke_geometry(PathGeometry& geometry, const std::vector<Contour>& contours,
                                    float radius, std::size_t maximum_quads, MeshBounds& bounds)
        {
            for (const Contour& contour : contours)
            {
                if (contour.points.empty())
                    continue;
                const std::size_t segment_count = contour.closed ? contour.points.size()
                                                                  : contour.points.size() - 1U;
                for (std::size_t index = 0; index < segment_count; ++index)
                {
                    const MeshPoint first = contour.points[index];
                    const MeshPoint second = contour.points[(index + 1U) % contour.points.size()];
                    const float dx = second.x - first.x;
                    const float dy = second.y - first.y;
                    const float length = std::hypot(dx, dy);
                    if (length <= 1.0e-6f)
                        continue;
                    const float nx = -dy / length * radius;
                    const float ny = dx / length * radius;
                    append_quad(geometry,
                                MeshQuad{{MeshPoint{first.x + nx, first.y + ny},
                                          MeshPoint{second.x + nx, second.y + ny},
                                          MeshPoint{first.x - nx, first.y - ny},
                                          MeshPoint{second.x - nx, second.y - ny}}},
                                maximum_quads, bounds);
                }
                for (MeshPoint point : contour.points)
                    append_round_join(geometry, point, radius, maximum_quads, bounds);
            }
        }

        std::size_t capped_add(std::size_t count, std::size_t additional,
                               std::size_t maximum)
        {
            return count >= maximum || additional >= maximum - count ? maximum
                                                                      : count + additional;
        }

        std::size_t estimated_quad_count(const std::vector<Contour>& contours, bool stroke,
                                         std::size_t maximum_quads)
        {
            std::size_t count = 0;
            for (const Contour& contour : contours)
            {
                if (stroke)
                {
                    const std::size_t segment_count = contour.points.empty()
                                                          ? 0U
                                                          : contour.closed
                                                                ? contour.points.size()
                                                                : contour.points.size() - 1U;
                    count = capped_add(count, segment_count, maximum_quads);
                    count = capped_add(count,
                                       contour.points.size() *
                                           static_cast<std::size_t>(round_join_segments),
                                       maximum_quads);
                }
                else if (contour.points.size() >= 3U)
                {
                    count = capped_add(count, contour.points.size() - 2U, maximum_quads);
                }
                if (count == maximum_quads)
                    return maximum_quads;
            }
            return count;
        }

        color interpolate_stops(const std::vector<ColorStop>& stops, float value)
        {
            value = std::clamp(value, 0.0f, 1.0f);
            if (value <= stops.front().offset)
                return stops.front().value;
            for (std::size_t index = 1; index < stops.size(); ++index)
            {
                if (value <= stops[index].offset)
                {
                    const float span = stops[index].offset - stops[index - 1U].offset;
                    const float amount = span <= 1.0e-8f
                                             ? 0.0f
                                             : (value - stops[index - 1U].offset) / span;
                    return color::color_lerp(stops[index - 1U].value, stops[index].value,
                                             amount);
                }
            }
            return stops.back().value;
        }

        color sample_paint(const Paint& paint, float x, float y)
        {
            if (paint.type() == Paint::Type::LinearGradient)
                return interpolate_stops(paint.stops(), x);
            if (paint.type() == Paint::Type::RadialGradient)
            {
                const float distance = std::hypot(x, y);
                const float amount = (distance - paint.inner_radius()) /
                                     (paint.outer_radius() - paint.inner_radius());
                return interpolate_stops(paint.stops(), amount);
            }
            if (paint.type() == Paint::Type::ImagePattern)
            {
                const Image& image = *paint.image();
                const int width = image.get_width();
                const int height = image.get_height();
                const int channels = image.get_channels();
                const auto repeated_index = [](float coordinate, int extent) {
                    int index = static_cast<int>(std::floor(coordinate * extent)) % extent;
                    return index < 0 ? index + extent : index;
                };
                const int px = repeated_index(x, width);
                const int py = repeated_index(y, height);
                const auto& pixels = image.pixels();
                const std::size_t offset =
                    (static_cast<std::size_t>(py) * width + px) * channels;
                const int red = pixels[offset];
                const int green = channels >= 3 ? pixels[offset + 1U] : red;
                const int blue = channels >= 3 ? pixels[offset + 2U] : red;
                const int alpha = channels == 2 ? pixels[offset + 1U]
                                  : channels == 4 ? pixels[offset + 3U]
                                                  : 255;
                return color(red, green, blue,
                             static_cast<int>(std::lround(alpha * paint.pattern_alpha())));
            }
            return paint.solid_color();
        }

        void update_bounds(float x, float y, float& minimum_x, float& minimum_y,
                           float& maximum_x, float& maximum_y)
        {
            minimum_x = std::min(minimum_x, x);
            minimum_y = std::min(minimum_y, y);
            maximum_x = std::max(maximum_x, x);
            maximum_y = std::max(maximum_y, y);
        }
    }

    PathGeometry tessellate_path(const Path& path, const Paint& paint,
                                 const Transform& device_transform, float line_width,
                                 std::size_t maximum_quads, std::size_t paint_texture_size)
    {
        if (maximum_quads == 0 || paint_texture_size == 0)
            throw std::invalid_argument("gCanvas tessellation limits must be non-zero");
        const std::vector<Contour> contours = flatten(path, device_transform);
        PathGeometry geometry;
        const bool stroke = line_width > 0.0f;
        geometry.mask_quads.reserve(estimated_quad_count(contours, stroke, maximum_quads));
        MeshBounds bounds;
        if (stroke)
        {
            const float scale_x = std::hypot(device_transform.a, device_transform.b);
            const float scale_y = std::hypot(device_transform.c, device_transform.d);
            const float radius = line_width * std::max(scale_x, scale_y) * 0.5f;
            geometry.mask_mode = PathMaskMode::Union;
            append_stroke_geometry(geometry, contours, radius, maximum_quads, bounds);
        }
        else
        {
            geometry.mask_mode = PathMaskMode::EvenOdd;
            append_fill_geometry(geometry, contours, maximum_quads, bounds);
        }
        if (geometry.mask_quads.empty())
            return geometry;

        geometry.solid_color = paint.type() == Paint::Type::Solid
                                   ? paint.solid_color()
                                   : color(255, 255, 255, 255);
        if (paint.type() == Paint::Type::Solid)
        {
            geometry.paint_quad = MeshQuad{{MeshPoint{bounds.minimum_x, bounds.minimum_y},
                                             MeshPoint{bounds.maximum_x, bounds.minimum_y},
                                             MeshPoint{bounds.minimum_x, bounds.maximum_y},
                                             MeshPoint{bounds.maximum_x, bounds.maximum_y}}};
            return geometry;
        }

        const Transform paint_to_device = device_transform * paint.paint_transform();
        Transform device_to_paint;
        if (!paint_to_device.invert(device_to_paint))
            throw std::invalid_argument("gCanvas paint-to-device transform is singular");
        float paint_minimum_x = std::numeric_limits<float>::max();
        float paint_minimum_y = std::numeric_limits<float>::max();
        float paint_maximum_x = std::numeric_limits<float>::lowest();
        float paint_maximum_y = std::numeric_limits<float>::lowest();
        for (const MeshQuad& quad : geometry.mask_quads)
        {
            for (MeshPoint point : quad.vertices)
            {
                float x = 0.0f;
                float y = 0.0f;
                device_to_paint.map(point.x, point.y, x, y);
                update_bounds(x, y, paint_minimum_x, paint_minimum_y, paint_maximum_x,
                              paint_maximum_y);
            }
        }
        if (paint_maximum_x - paint_minimum_x <= 1.0e-8f ||
            paint_maximum_y - paint_minimum_y <= 1.0e-8f)
            throw std::invalid_argument("gCanvas path paint bounds are degenerate");

        geometry.paint_quad.vertices[0] =
            mapped(paint_to_device, paint_minimum_x, paint_minimum_y);
        geometry.paint_quad.vertices[1] =
            mapped(paint_to_device, paint_maximum_x, paint_minimum_y);
        geometry.paint_quad.vertices[2] =
            mapped(paint_to_device, paint_minimum_x, paint_maximum_y);
        geometry.paint_quad.vertices[3] =
            mapped(paint_to_device, paint_maximum_x, paint_maximum_y);

        if (paint.type() == Paint::Type::ImagePattern && paint.pattern_alpha() >= 1.0f)
        {
            geometry.paint_image = paint.image();
            geometry.paint_uv_min = {paint_minimum_x, paint_minimum_y};
            geometry.paint_uv_max = {paint_maximum_x, paint_maximum_y};
            return geometry;
        }

        if (paint.type() == Paint::Type::LinearGradient ||
            paint.type() == Paint::Type::RadialGradient)
        {
            geometry.raster_paint = paint;
            geometry.raster_minimum = {paint_minimum_x, paint_minimum_y};
            geometry.raster_maximum = {paint_maximum_x, paint_maximum_y};
            return geometry;
        }

        geometry.paint_pixels = rasterize_path_paint(
            paint, {paint_minimum_x, paint_minimum_y},
            {paint_maximum_x, paint_maximum_y}, paint_texture_size);
        return geometry;
    }

    std::vector<std::uint8_t> rasterize_path_paint(const Paint& paint,
                                                   MeshPoint minimum, MeshPoint maximum,
                                                   std::size_t paint_texture_size)
    {
        if (paint.type() != Paint::Type::LinearGradient &&
            paint.type() != Paint::Type::RadialGradient &&
            paint.type() != Paint::Type::ImagePattern)
            throw std::invalid_argument("gCanvas raster paint type is invalid");
        if (paint_texture_size == 0 ||
            paint_texture_size > std::numeric_limits<std::size_t>::max() / paint_texture_size ||
            paint_texture_size * paint_texture_size >
                std::numeric_limits<std::size_t>::max() / 4U)
            throw std::overflow_error("gCanvas path paint texture size overflow");

        std::vector<std::uint8_t> pixels(paint_texture_size * paint_texture_size * 4U);
        if (paint.type() == Paint::Type::LinearGradient)
        {
            for (std::size_t x = 0; x < paint_texture_size; ++x)
            {
                const float paint_x = minimum.x +
                                      (static_cast<float>(x) + 0.5f) /
                                          static_cast<float>(paint_texture_size) *
                                          (maximum.x - minimum.x);
                const color sampled = sample_paint(paint, paint_x, minimum.y);
                for (std::size_t y = 0; y < paint_texture_size; ++y)
                {
                    const std::size_t offset = (y * paint_texture_size + x) * 4U;
                    pixels[offset] = sampled.r();
                    pixels[offset + 1U] = sampled.g();
                    pixels[offset + 2U] = sampled.b();
                    pixels[offset + 3U] = sampled.a();
                }
            }
            return pixels;
        }

        for (std::size_t y = 0; y < paint_texture_size; ++y)
        {
            const float paint_y = minimum.y +
                                  (static_cast<float>(y) + 0.5f) /
                                      static_cast<float>(paint_texture_size) *
                                      (maximum.y - minimum.y);
            for (std::size_t x = 0; x < paint_texture_size; ++x)
            {
                const float paint_x = minimum.x +
                                      (static_cast<float>(x) + 0.5f) /
                                          static_cast<float>(paint_texture_size) *
                                          (maximum.x - minimum.x);
                const color sampled = sample_paint(paint, paint_x, paint_y);
                const std::size_t offset = (y * paint_texture_size + x) * 4U;
                pixels[offset] = sampled.r();
                pixels[offset + 1U] = sampled.g();
                pixels[offset + 2U] = sampled.b();
                pixels[offset + 3U] = sampled.a();
            }
        }
        return pixels;
    }
}
