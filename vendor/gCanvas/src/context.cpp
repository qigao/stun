#include "gcanvas/context.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "path_tessellator.hpp"
#include "utf8.hpp"

namespace gcanvas
{
    namespace
    {
        constexpr int shadow_kernel_radius = 2;
        constexpr float maximum_sampled_shadow_alpha = 0.98f;
        constexpr std::size_t maximum_shadow_samples = 25U;

        bool same_color(const color& lhs, const color& rhs) noexcept
        {
            return lhs.r() == rhs.r() && lhs.g() == rhs.g() &&
                   lhs.b() == rhs.b() && lhs.a() == rhs.a();
        }

        bool same_transform(const Transform& lhs, const Transform& rhs) noexcept
        {
            return lhs.a == rhs.a && lhs.b == rhs.b && lhs.c == rhs.c &&
                   lhs.d == rhs.d && lhs.e == rhs.e && lhs.f == rhs.f;
        }

        bool same_gradient(const Paint& lhs, const Paint& rhs) noexcept
        {
            if (lhs.type() != rhs.type() ||
                !same_transform(lhs.paint_transform(), rhs.paint_transform()) ||
                lhs.inner_radius() != rhs.inner_radius() ||
                lhs.outer_radius() != rhs.outer_radius() ||
                lhs.stops().size() != rhs.stops().size())
                return false;
            for (std::size_t index = 0; index < lhs.stops().size(); ++index)
            {
                if (lhs.stops()[index].offset != rhs.stops()[index].offset ||
                    !same_color(lhs.stops()[index].value, rhs.stops()[index].value))
                    return false;
            }
            return true;
        }

        void validate_shadow_filter(float blur_radius, float spread_radius)
        {
            constexpr float maximum_radius =
                (std::numeric_limits<float>::max)() * 0.5f;
            if (!std::isfinite(blur_radius) || !std::isfinite(spread_radius) ||
                blur_radius < 0.0f || std::fabs(spread_radius) > maximum_radius ||
                (spread_radius > 0.0f &&
                 blur_radius > (std::numeric_limits<float>::max)() - spread_radius))
            {
                throw std::invalid_argument(
                    "gCanvas sampled shadow radii are outside the finite supported range");
            }
        }

        Transform offset_transform(const Transform& transform, float x, float y)
        {
            return Transform::translation(x, y) * transform;
        }

        float maximum_paint_alpha(const Paint& paint)
        {
            switch (paint.type())
            {
            case Paint::Type::None:
                return 0.0f;
            case Paint::Type::Solid:
                return paint.solid_color().af();
            case Paint::Type::LinearGradient:
            case Paint::Type::RadialGradient:
            {
                float result = 0.0f;
                for (const ColorStop& stop : paint.stops())
                    result = (std::max)(result, stop.value.af());
                return result;
            }
            case Paint::Type::ImagePattern:
                // Pattern alpha is the API-level opacity fact source. Per-pixel source alpha is
                // still sampled by the texture and multiplied by each normalized blur tap.
                return paint.pattern_alpha();
            }
            throw std::logic_error("gCanvas paint type is invalid");
        }
    }

    Context::Context(CanvasMetrics metrics, ResourceLimits limits) : _resource_limits(limits)
    {
        if (_resource_limits.max_images == 0 || _resource_limits.max_fonts == 0 ||
            _resource_limits.max_path_surfaces == 0 ||
            _resource_limits.max_path_surface_pixels == 0 ||
            _resource_limits.max_path_mesh_quads == 0 ||
            _resource_limits.path_paint_texture_size == 0 ||
            _resource_limits.max_clip_vertices < 3 || _resource_limits.max_shadow_samples == 0 ||
            _resource_limits.max_shadow_samples > maximum_shadow_samples ||
            _resource_limits.max_blur_samples == 0 ||
            _resource_limits.max_blur_samples > maximum_shadow_samples ||
            _resource_limits.max_blur_commands == 0 ||
            _resource_limits.path_paint_texture_size > 4096)
        {
            throw std::invalid_argument("gCanvas resource limits are invalid");
        }
        _owned_images.reserve(_resource_limits.max_images);
        _owned_fonts.reserve(_resource_limits.max_fonts);
        _cached_path_paints.reserve(_resource_limits.max_path_surfaces);
        set_metrics(metrics);
    }

    Context::~Context() = default;

    Context::ShadowKernel Context::make_filter_kernel(float blur_radius, float spread_radius,
                                                       float target_alpha,
                                                       std::size_t maximum_samples,
                                                       const char* limit_message) const
        {
            validate_shadow_filter(blur_radius, spread_radius);
            Context::ShadowKernel result;
            if (target_alpha <= 0.0f)
                return result;
            const float positive_spread = (std::max)(spread_radius, 0.0f);
            if (blur_radius == 0.0f && positive_spread == 0.0f)
            {
                result.samples[0] = {0.0f, 0.0f, target_alpha};
                result.count = 1;
                return result;
            }

            const float extent = blur_radius + positive_spread;
            const float step = extent / static_cast<float>(shadow_kernel_radius);
            const float sigma = (std::max)(blur_radius * 0.5f, step * 0.5f);
            float total_weight = 0.0f;
            for (int y = -shadow_kernel_radius; y <= shadow_kernel_radius; ++y)
            {
                for (int x = -shadow_kernel_radius; x <= shadow_kernel_radius; ++x)
                {
                    const float sample_x = static_cast<float>(x) * step;
                    const float sample_y = static_cast<float>(y) * step;
                    const float distance = std::hypot(sample_x, sample_y);
                    if (distance > extent + step * 0.25f)
                        continue;
                    const float blurred_distance = (std::max)(0.0f, distance - positive_spread);
                    const float weight =
                        std::exp(-0.5f * blurred_distance * blurred_distance / (sigma * sigma));
                    if (result.count >= maximum_samples)
                        throw std::length_error(limit_message);
                    result.samples[result.count++] = {sample_x, sample_y, weight};
                    total_weight += weight;
                }
            }
            if (result.count == 0 || total_weight <= 0.0f)
                throw std::logic_error("gCanvas sampled filter kernel is empty");

            // Source-over normalization keeps the combined opacity independent of tap count.
            const float transparency =
                1.0f - (std::min)(target_alpha, maximum_sampled_shadow_alpha);
            for (std::size_t index = 0; index < result.count; ++index)
            {
                const float normalized_weight = result.samples[index].alpha / total_weight;
                result.samples[index].alpha =
                    1.0f - std::pow(transparency, normalized_weight);
            }
            return result;
        }

    Context::ShadowKernel Context::make_shadow_kernel(float blur_radius, float spread_radius,
                                                       float target_alpha) const
    {
        return make_filter_kernel(blur_radius, spread_radius, target_alpha,
                                  _resource_limits.max_shadow_samples,
                                  "gCanvas sampled shadow limit reached");
    }

    Context::ShadowKernel Context::make_blur_kernel(float blur_radius,
                                                     float target_alpha) const
    {
        return make_filter_kernel(blur_radius, 0.0f, target_alpha,
                                  _resource_limits.max_blur_samples,
                                  "gCanvas sampled blur limit reached");
    }

    void Context::draw_text(float x, float y, std::string text, const Transform& transform)
    {
        if (!std::isfinite(x) || !std::isfinite(y))
            throw std::invalid_argument("gCanvas affine text position must be finite");
        validate_transform(transform);
        if (transform.a == 1.0f && transform.b == 0.0f && transform.c == 0.0f &&
            transform.d == 1.0f && transform.e == 0.0f && transform.f == 0.0f)
        {
            draw_text(x, y, text);
            return;
        }
        throw std::logic_error("gCanvas backend does not support affine text drawing");
    }

    void Context::draw_text(float x, float y, std::u32string text, const Transform& transform)
    {
        if (!std::isfinite(x) || !std::isfinite(y))
            throw std::invalid_argument("gCanvas affine text position must be finite");
        validate_transform(transform);
        if (transform.a == 1.0f && transform.b == 0.0f && transform.c == 0.0f &&
            transform.d == 1.0f && transform.e == 0.0f && transform.f == 0.0f)
        {
            draw_text(x, y, text);
            return;
        }
        throw std::logic_error("gCanvas backend does not support affine text drawing");
    }

    void Context::validate_transform(const Transform& transform) const
    {
        if (!std::isfinite(transform.a) || !std::isfinite(transform.b) ||
            !std::isfinite(transform.c) || !std::isfinite(transform.d) ||
            !std::isfinite(transform.e) || !std::isfinite(transform.f))
        {
            throw std::invalid_argument("gCanvas affine transform must be finite");
        }
    }

    void Context::validate_convex_mask(const std::vector<vec2>& vertices) const
    {
        if (vertices.size() == 1U || vertices.size() == 2U)
            throw std::invalid_argument("gCanvas convex mask requires zero or at least three vertices");
        if (vertices.size() > _resource_limits.max_clip_vertices)
            throw std::length_error("gCanvas convex mask vertex limit reached");

        float winding = 0.0f;
        for (std::size_t index = 0; index < vertices.size(); ++index)
        {
            const vec2& current = vertices[index];
            const vec2& next = vertices[(index + 1U) % vertices.size()];
            if (!std::isfinite(current.get_x()) || !std::isfinite(current.get_y()))
                throw std::invalid_argument("gCanvas convex mask vertices must be finite");
            winding += current.get_x() * next.get_y() - current.get_y() * next.get_x();
        }
        if (!vertices.empty() && !std::isfinite(winding))
            throw std::invalid_argument("gCanvas convex mask geometry overflow");

        float direction = 0.0f;
        for (std::size_t index = 0; index < vertices.size(); ++index)
        {
            const vec2& first = vertices[index];
            const vec2& second = vertices[(index + 1U) % vertices.size()];
            const vec2& third = vertices[(index + 2U) % vertices.size()];
            const float cross = (second.get_x() - first.get_x()) *
                                    (third.get_y() - second.get_y()) -
                                (second.get_y() - first.get_y()) *
                                    (third.get_x() - second.get_x());
            if (!std::isfinite(cross))
                throw std::invalid_argument("gCanvas convex mask geometry overflow");
            if (cross == 0.0f)
                continue;
            if (direction == 0.0f)
                direction = cross;
            else if ((direction < 0.0f) != (cross < 0.0f))
                throw std::invalid_argument("gCanvas mask vertices must form a convex polygon");
        }
        if (!vertices.empty() && (winding == 0.0f || direction == 0.0f))
            throw std::invalid_argument("gCanvas convex mask must have non-zero area");
    }

    void Context::validate_rect_mask(float x, float y, float width, float height) const
    {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
            !std::isfinite(height) || width < 0.0f || height < 0.0f ||
            !std::isfinite(x + width) || !std::isfinite(y + height))
        {
            throw std::invalid_argument(
                "gCanvas rectangular mask bounds must be finite with non-negative size");
        }
    }

    void Context::set_convex_mask(const std::vector<vec2>& vertices)
    {
        validate_convex_mask(vertices);
        if (vertices.empty())
        {
            set_rect_mask(0.0f, 0.0f, 0.0f, 0.0f);
            return;
        }
        throw std::logic_error("gCanvas backend does not support convex masks");
    }

    void Context::draw_image(float x, float y, float width, float height, Image& image,
                             const Transform& transform, bool tint)
    {
        validate_resource(image);
        make_image_quad(x, y, width, height, transform);
        if (width == 0.0f || height == 0.0f)
            return;
        if (transform.a == 1.0f && transform.b == 0.0f && transform.c == 0.0f &&
            transform.d == 1.0f && transform.e == 0.0f && transform.f == 0.0f)
        {
            draw_image(x, y, width, height, image, tint);
            return;
        }
        throw std::logic_error("gCanvas backend does not support affine image drawing");
    }

    Context::ImageQuad Context::make_image_quad(float x, float y, float width, float height,
                                                 const Transform& transform) const
    {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
            !std::isfinite(height) || width < 0.0f || height < 0.0f)
        {
            throw std::invalid_argument(
                "gCanvas affine image bounds must be finite and non-negative");
        }
        validate_transform(transform);
        const float right = x + width;
        const float bottom = y + height;
        if (!std::isfinite(right) || !std::isfinite(bottom))
            throw std::invalid_argument("gCanvas affine image bounds overflow");

        const Transform device_transform =
            Transform::scaling(_metrics.scale_x * _metrics.dpi_scale,
                               _metrics.scale_y * _metrics.dpi_scale) *
            Transform::translation(_metrics.offset_x, _metrics.offset_y) * transform;
        const auto map_to_clip = [&](float source_x, float source_y) {
            float mapped_x = 0.0f;
            float mapped_y = 0.0f;
            device_transform.map(source_x, source_y, mapped_x, mapped_y);
            if (!std::isfinite(mapped_x) || !std::isfinite(mapped_y))
                throw std::invalid_argument("gCanvas affine image transform overflow");
            return vec2(mapped_x / static_cast<float>(_width) * 2.0f - 1.0f,
                        mapped_y / static_cast<float>(_height) * 2.0f - 1.0f);
        };

        ImageQuad quad;
        quad.top_left = map_to_clip(x, y);
        quad.top_right = map_to_clip(right, y);
        quad.bottom_left = map_to_clip(x, bottom);
        quad.bottom_right = map_to_clip(right, bottom);

        float edge_x = 0.0f;
        float edge_y = 0.0f;
        device_transform.map(right, y, edge_x, edge_y);
        float origin_x = 0.0f;
        float origin_y = 0.0f;
        device_transform.map(x, y, origin_x, origin_y);
        const float device_width = std::hypot(edge_x - origin_x, edge_y - origin_y);
        device_transform.map(x, bottom, edge_x, edge_y);
        const float device_height = std::hypot(edge_x - origin_x, edge_y - origin_y);
        quad.resolution = vec2(device_width, device_height);
        return quad;
    }

    Context::ShadowQuad Context::make_shadow_quad(float x, float y, float width, float height,
                                                   float blur_radius) const
    {
        if (!std::isfinite(blur_radius) || blur_radius < 0.0f)
            throw std::invalid_argument(
                "gCanvas shadow blur radius must be finite and non-negative");

        const ImageQuad content = make_image_quad(x, y, width, height, Transform{});
        if (!std::isfinite(x - blur_radius) || !std::isfinite(y - blur_radius) ||
            !std::isfinite(width + blur_radius * 2.0f) ||
            !std::isfinite(height + blur_radius * 2.0f))
        {
            throw std::invalid_argument("gCanvas shadow bounds overflow");
        }

        ShadowQuad result;
        result.geometry = make_image_quad(x - blur_radius, y - blur_radius,
                                          width + blur_radius * 2.0f,
                                          height + blur_radius * 2.0f, Transform{});
        const float shader_scale = _metrics.scale_x * _metrics.dpi_scale;
        result.geometry.resolution = vec2(content.resolution.get_x(), height * shader_scale);
        result.blur_radius = vec2(blur_radius * shader_scale);
        return result;
    }

    void Context::validate_shadow_radii(float radius_nw, float radius_ne, float radius_se,
                                         float radius_sw) const
    {
        if (!std::isfinite(radius_nw) || !std::isfinite(radius_ne) ||
            !std::isfinite(radius_se) || !std::isfinite(radius_sw) || radius_nw < 0.0f ||
            radius_ne < 0.0f || radius_se < 0.0f || radius_sw < 0.0f)
        {
            throw std::invalid_argument(
                "gCanvas shadow corner radii must be finite and non-negative");
        }
    }

    void Context::validate_inset_shadow(float offset_x, float offset_y, float blur_radius,
                                        float spread_radius) const
    {
        if (!std::isfinite(offset_x) || !std::isfinite(offset_y) ||
            !std::isfinite(blur_radius) || !std::isfinite(spread_radius) || blur_radius < 0.0f)
        {
            throw std::invalid_argument(
                "gCanvas inset shadow values must be finite with non-negative blur");
        }
    }

    void Context::draw_rect_inset_shadow(float x, float y, float width, float height,
                                         float offset_x, float offset_y, float blur_radius,
                                         float spread_radius)
    {
        make_image_quad(x, y, width, height, Transform{});
        validate_inset_shadow(offset_x, offset_y, blur_radius, spread_radius);
        if (width == 0.0f || height == 0.0f || _fill_color.af() <= 0.0f)
            return;
        draw_inset_shadow({x, y, width, height, 0.0f, offset_x, offset_y, blur_radius,
                           spread_radius, false});
    }

    void Context::draw_rounded_rect_inset_shadow(
        float x, float y, float width, float height, float border_radius, float offset_x,
        float offset_y, float blur_radius, float spread_radius)
    {
        make_image_quad(x, y, width, height, Transform{});
        validate_shadow_radii(border_radius, border_radius, border_radius, border_radius);
        validate_inset_shadow(offset_x, offset_y, blur_radius, spread_radius);
        if (width == 0.0f || height == 0.0f || _fill_color.af() <= 0.0f)
            return;
        draw_inset_shadow({x, y, width, height, border_radius, offset_x, offset_y,
                           blur_radius, spread_radius, false});
    }

    void Context::draw_circle_inset_shadow(float x, float y, float radius, float offset_x,
                                           float offset_y, float blur_radius,
                                           float spread_radius)
    {
        if (!std::isfinite(radius) || radius < 0.0f)
            throw std::invalid_argument("gCanvas inset shadow circle radius is invalid");
        validate_inset_shadow(offset_x, offset_y, blur_radius, spread_radius);
        if (radius == 0.0f || _fill_color.af() <= 0.0f)
            return;
        draw_inset_shadow({x - radius, y - radius, radius * 2.0f, radius * 2.0f,
                           0.0f, offset_x, offset_y, blur_radius, spread_radius, true});
    }

    void Context::draw_ellipse_inset_shadow(float x, float y, float radius_x, float radius_y,
                                            float offset_x, float offset_y, float blur_radius,
                                            float spread_radius)
    {
        if (!std::isfinite(radius_x) || !std::isfinite(radius_y) || radius_x < 0.0f ||
            radius_y < 0.0f)
            throw std::invalid_argument("gCanvas inset shadow ellipse radii are invalid");
        validate_inset_shadow(offset_x, offset_y, blur_radius, spread_radius);
        if (radius_x == 0.0f || radius_y == 0.0f || _fill_color.af() <= 0.0f)
            return;
        draw_inset_shadow({x - radius_x, y - radius_y, radius_x * 2.0f,
                           radius_y * 2.0f, 0.0f, offset_x, offset_y, blur_radius,
                           spread_radius, true});
    }

    void Context::fill_path(const Path& path, const Paint& paint, const Transform& transform)
    {
        if (path.empty() || paint.type() == Paint::Type::None)
            return;
        validate_path_paint(paint);
        draw_tessellated_path(path, paint, 0.0f, transform);
    }

    void Context::stroke_path(const Path& path, const Paint& paint, float line_width,
                              const Transform& transform)
    {
        if (!std::isfinite(line_width) || line_width < 0.0f)
            throw std::invalid_argument("gCanvas path line width must be finite and non-negative");
        if (line_width == 0.0f || path.empty() || paint.type() == Paint::Type::None)
            return;
        validate_path_paint(paint);
        draw_tessellated_path(path, paint, line_width, transform);
    }

    void Context::draw_path_blur(const Path& path, const Paint& paint, float line_width,
                                 float blur_radius, const Transform& transform)
    {
        if (!std::isfinite(line_width) || line_width < 0.0f)
            throw std::invalid_argument("gCanvas blur path line width must be non-negative");
        validate_transform(transform);
        validate_shadow_filter(blur_radius, 0.0f);
        if (path.empty() || paint.type() == Paint::Type::None)
            return;
        validate_path_paint(paint);
        const float source_alpha = maximum_paint_alpha(paint);
        const ShadowKernel kernel = make_blur_kernel(blur_radius, source_alpha);
        if (kernel.count == 0)
            return;
        draw_tessellated_path_blur(path, paint, line_width, kernel, source_alpha,
                                   transform);
    }

    void Context::draw_text_blur(float x, float y, std::string text, float blur_radius,
                                 const Transform& transform)
    {
        if (!std::isfinite(x) || !std::isfinite(y))
            throw std::invalid_argument("gCanvas blur text position must be finite");
        validate_transform(transform);
        validate_shadow_filter(blur_radius, 0.0f);
        if (text.empty() || _fill_color.af() <= 0.0f)
            return;

        const std::u32string codepoints = detail::decode_utf8(text);
        const color base = _fill_color;
        const ShadowKernel kernel = make_blur_kernel(blur_radius, base.af());
        try
        {
            for (std::size_t index = 0; index < kernel.count; ++index)
            {
                const ShadowSample& sample = kernel.samples[index];
                _fill_color = color(base.rf(), base.gf(), base.bf(), sample.alpha);
                draw_text(x, y, codepoints,
                          offset_transform(transform, sample.x, sample.y));
            }
        }
        catch (...)
        {
            _fill_color = base;
            throw;
        }
        _fill_color = base;
    }

    void Context::draw_image_blur(float x, float y, float width, float height, Image& image,
                                  float blur_radius, const Transform& transform)
    {
        validate_resource(image);
        make_image_quad(x, y, width, height, transform);
        validate_shadow_filter(blur_radius, 0.0f);
        if (width == 0.0f || height == 0.0f || _fill_color.af() <= 0.0f)
            return;

        const color base = _fill_color;
        const ShadowKernel kernel = make_blur_kernel(blur_radius, base.af());
        try
        {
            for (std::size_t index = 0; index < kernel.count; ++index)
            {
                const ShadowSample& sample = kernel.samples[index];
                _fill_color = color(1.0f, 1.0f, 1.0f, sample.alpha);
                draw_image(x, y, width, height, image,
                           offset_transform(transform, sample.x, sample.y), true);
            }
        }
        catch (...)
        {
            _fill_color = base;
            throw;
        }
        _fill_color = base;
    }

    void Context::draw_path_shadow(const Path& path, float line_width, float blur_radius,
                                   float spread_radius, const Transform& transform)
    {
        if (!std::isfinite(line_width) || line_width < 0.0f)
            throw std::invalid_argument("gCanvas shadow path line width must be non-negative");
        validate_transform(transform);
        validate_shadow_filter(blur_radius, spread_radius);
        if (path.empty() || _fill_color.af() <= 0.0f)
            return;

        const color base = _fill_color;
        const ShadowKernel kernel =
            make_shadow_kernel(blur_radius, (std::max)(spread_radius, 0.0f), base.af());
        if (line_width == 0.0f && spread_radius < 0.0f)
        {
            draw_tessellated_path_eroded_shadow(path, -spread_radius, kernel, transform);
            return;
        }
        for (std::size_t index = 0; index < kernel.count; ++index)
        {
            const ShadowSample& sample = kernel.samples[index];
            const Paint paint = Paint::solid(
                color(base.rf(), base.gf(), base.bf(), sample.alpha));
            const Transform sampled = offset_transform(transform, sample.x, sample.y);
            if (line_width > 0.0f)
            {
                const float spread_width = spread_radius * 2.0f;
                if (spread_width > 0.0f &&
                    line_width > (std::numeric_limits<float>::max)() - spread_width)
                    throw std::invalid_argument("gCanvas spread path line width overflows");
                const float spread_line_width = line_width + spread_width;
                if (spread_line_width > 0.0f)
                    draw_tessellated_path(path, paint, spread_line_width, sampled);
            }
            else
            {
                draw_tessellated_path(path, paint, 0.0f, sampled);
                if (spread_radius > 0.0f)
                    draw_tessellated_path(path, paint, spread_radius * 2.0f, sampled);
            }
        }
    }

    void Context::draw_text_shadow(float x, float y, std::string text, float blur_radius,
                                   float spread_radius, const Transform& transform)
    {
        if (!std::isfinite(x) || !std::isfinite(y))
            throw std::invalid_argument("gCanvas shadow text position must be finite");
        validate_transform(transform);
        validate_shadow_filter(blur_radius, spread_radius);
        if (text.empty() || _fill_color.af() <= 0.0f)
            return;

        const std::u32string codepoints = detail::decode_utf8(text);
        const color base = _fill_color;
        const ShadowKernel kernel = make_shadow_kernel(blur_radius, spread_radius, base.af());
        const float erosion_radius = (std::max)(-spread_radius, 0.0f);
        try
        {
            for (std::size_t index = 0; index < kernel.count; ++index)
            {
                const ShadowSample& sample = kernel.samples[index];
                _fill_color = color(base.rf(), base.gf(), base.bf(), sample.alpha);
                draw_text_alpha_mask(x, y, codepoints, erosion_radius,
                                     offset_transform(transform, sample.x, sample.y));
            }
        }
        catch (...)
        {
            _fill_color = base;
            throw;
        }
        _fill_color = base;
    }

    void Context::draw_image_shadow(float x, float y, float width, float height, Image& image,
                                    float blur_radius, float spread_radius,
                                    const Transform& transform)
    {
        validate_resource(image);
        make_image_quad(x, y, width, height, transform);
        validate_shadow_filter(blur_radius, spread_radius);
        if (width == 0.0f || height == 0.0f || _fill_color.af() <= 0.0f)
            return;

        const color base = _fill_color;
        const ShadowKernel kernel = make_shadow_kernel(blur_radius, spread_radius, base.af());
        const float erosion_radius = (std::max)(-spread_radius, 0.0f);
        try
        {
            for (std::size_t index = 0; index < kernel.count; ++index)
            {
                const ShadowSample& sample = kernel.samples[index];
                _fill_color = color(base.rf(), base.gf(), base.bf(), sample.alpha);
                draw_image_alpha_mask(x, y, width, height, image, erosion_radius,
                                      offset_transform(transform, sample.x, sample.y));
            }
        }
        catch (...)
        {
            _fill_color = base;
            throw;
        }
        _fill_color = base;
    }

    void Context::draw_path_inset_shadow(const Path& path, float line_width, float offset_x,
                                          float offset_y, float blur_radius,
                                          float spread_radius, const Transform& transform)
    {
        if (!std::isfinite(line_width) || line_width < 0.0f)
            throw std::invalid_argument("gCanvas inset path line width must be non-negative");
        validate_transform(transform);
        validate_inset_shadow(offset_x, offset_y, blur_radius, spread_radius);
        validate_shadow_filter(blur_radius, spread_radius);
        if (path.empty() || _fill_color.af() <= 0.0f)
            return;
        const ShadowKernel kernel =
            make_shadow_kernel(blur_radius, spread_radius, _fill_color.af());
        const float dilation_radius = (std::max)(-spread_radius, 0.0f);
        draw_tessellated_path_inset_shadow(path, line_width, offset_x, offset_y, kernel,
                                            dilation_radius, transform);
    }

    void Context::draw_text_inset_shadow(float x, float y, std::string text, float offset_x,
                                          float offset_y, float blur_radius,
                                          float spread_radius, const Transform& transform)
    {
        if (!std::isfinite(x) || !std::isfinite(y))
            throw std::invalid_argument("gCanvas inset text position must be finite");
        validate_transform(transform);
        validate_inset_shadow(offset_x, offset_y, blur_radius, spread_radius);
        validate_shadow_filter(blur_radius, spread_radius);
        if (text.empty() || _fill_color.af() <= 0.0f)
            return;

        const std::u32string codepoints = detail::decode_utf8(text);
        const color base = _fill_color;
        const ShadowKernel kernel = make_shadow_kernel(blur_radius, spread_radius, base.af());
        const float dilation_radius = (std::max)(-spread_radius, 0.0f);
        try
        {
            for (std::size_t index = 0; index < kernel.count; ++index)
            {
                const ShadowSample& sample = kernel.samples[index];
                _fill_color = color(base.rf(), base.gf(), base.bf(), sample.alpha);
                draw_text_inset_alpha_mask(x, y, codepoints, offset_x + sample.x,
                                            offset_y + sample.y, dilation_radius, transform);
            }
        }
        catch (...)
        {
            _fill_color = base;
            throw;
        }
        _fill_color = base;
    }

    void Context::draw_image_inset_shadow(float x, float y, float width, float height,
                                           Image& image, float offset_x, float offset_y,
                                           float blur_radius, float spread_radius,
                                           const Transform& transform)
    {
        validate_resource(image);
        make_image_quad(x, y, width, height, transform);
        validate_inset_shadow(offset_x, offset_y, blur_radius, spread_radius);
        validate_shadow_filter(blur_radius, spread_radius);
        if (width == 0.0f || height == 0.0f || _fill_color.af() <= 0.0f)
            return;

        const color base = _fill_color;
        const ShadowKernel kernel = make_shadow_kernel(blur_radius, spread_radius, base.af());
        const float dilation_radius = (std::max)(-spread_radius, 0.0f);
        try
        {
            for (std::size_t index = 0; index < kernel.count; ++index)
            {
                const ShadowSample& sample = kernel.samples[index];
                _fill_color = color(base.rf(), base.gf(), base.bf(), sample.alpha);
                draw_image_inset_alpha_mask(x, y, width, height, image,
                                             offset_x + sample.x, offset_y + sample.y,
                                             dilation_radius, transform);
            }
        }
        catch (...)
        {
            _fill_color = base;
            throw;
        }
        _fill_color = base;
    }

    void Context::validate_path_paint(const Paint& paint) const
    {
        if (paint.type() == Paint::Type::ImagePattern)
        {
            if (paint.image() == nullptr)
                throw std::invalid_argument("gCanvas image pattern has no image");
            validate_resource(*paint.image());
        }
    }

    Image& Context::acquire_path_paint_texture(const std::vector<std::uint8_t>& pixels)
    {
        const std::size_t texture_size = _resource_limits.path_paint_texture_size;
        if (texture_size > std::numeric_limits<std::size_t>::max() / texture_size ||
            texture_size * texture_size > std::numeric_limits<std::size_t>::max() / 4U ||
            pixels.size() != texture_size * texture_size * 4U)
            throw std::invalid_argument("gCanvas path paint texture span has the wrong size");
        if (_active_path_paint_textures >= _resource_limits.max_path_surfaces)
            throw std::length_error("gCanvas path paint texture count limit reached");

        Image* texture = nullptr;
        if (_transient_path_paint_texture_cursor < _transient_path_paint_textures.size())
        {
            texture = _transient_path_paint_textures[_transient_path_paint_texture_cursor];
            texture->update_data(pixels.data(), pixels.size());
        }
        else
        {
            const int dimension = static_cast<int>(texture_size);
            texture = &create_image(dimension, dimension, 4, pixels.data(), pixels.size(),
                                    ImageConfig{false, LINEAR});
            _transient_path_paint_textures.push_back(texture);
        }
        ++_transient_path_paint_texture_cursor;
        ++_active_path_paint_textures;
        return *texture;
    }

    Image& Context::acquire_cached_path_paint_texture(const Paint& paint, vec2 minimum,
                                                      vec2 maximum)
    {
        if (paint.type() != Paint::Type::LinearGradient &&
            paint.type() != Paint::Type::RadialGradient)
            throw std::invalid_argument("gCanvas cached path paint must be a gradient");

        for (CachedPathPaint& entry : _cached_path_paints)
        {
            if (!entry.occupied || entry.minimum != minimum || entry.maximum != maximum ||
                !same_gradient(entry.paint, paint))
                continue;
            if (entry.frame_generation != _path_paint_frame_generation)
            {
                if (_active_path_paint_textures >= _resource_limits.max_path_surfaces)
                    throw std::length_error("gCanvas path paint texture count limit reached");
                entry.frame_generation = _path_paint_frame_generation;
                ++_active_path_paint_textures;
            }
            return *entry.texture;
        }

        if (_active_path_paint_textures >= _resource_limits.max_path_surfaces)
            throw std::length_error("gCanvas path paint texture count limit reached");
        const std::vector<std::uint8_t> pixels = detail::rasterize_path_paint(
            paint, {minimum.get_x(), minimum.get_y()}, {maximum.get_x(), maximum.get_y()},
            _resource_limits.path_paint_texture_size);

        CachedPathPaint* selected = nullptr;
        if (_cached_path_paints.size() < _resource_limits.max_path_surfaces)
        {
            _cached_path_paints.emplace_back();
            selected = &_cached_path_paints.back();
        }
        else
        {
            for (std::size_t offset = 0; offset < _cached_path_paints.size(); ++offset)
            {
                const std::size_t index =
                    (_path_paint_eviction_cursor + offset) % _cached_path_paints.size();
                if (_cached_path_paints[index].frame_generation !=
                    _path_paint_frame_generation)
                {
                    selected = &_cached_path_paints[index];
                    _path_paint_eviction_cursor =
                        (index + 1U) % _cached_path_paints.size();
                    break;
                }
            }
        }
        if (selected == nullptr)
            throw std::length_error("gCanvas path paint cache has no evictable slot");

        if (selected->texture == nullptr)
        {
            const int dimension = static_cast<int>(_resource_limits.path_paint_texture_size);
            selected->texture = &create_image(dimension, dimension, 4, pixels.data(),
                                              pixels.size(), ImageConfig{false, LINEAR});
        }
        else
        {
            selected->texture->update_data(pixels.data(), pixels.size());
        }
        selected->paint = paint;
        selected->minimum = minimum;
        selected->maximum = maximum;
        selected->frame_generation = _path_paint_frame_generation;
        selected->occupied = true;
        ++_active_path_paint_textures;
        return *selected->texture;
    }

    void Context::reset_transient_path_resources() noexcept
    {
        _transient_path_paint_texture_cursor = 0;
        _active_path_paint_textures = 0;
        if (_path_paint_frame_generation == (std::numeric_limits<std::size_t>::max)())
        {
            _path_paint_frame_generation = 1;
            for (CachedPathPaint& entry : _cached_path_paints)
                entry.frame_generation = 0;
        }
        else
        {
            ++_path_paint_frame_generation;
        }
    }

    void Context::set_fill_color(color color)
    {
        _fill_color = color;
    }

    void Context::set_stroke_color(color color)
    {
        _stroke_color = color;
    }

    void Context::set_line_width(float width)
    {
        _line_width = width;
    }

    void Context::set_font(Font& font)
    {
        validate_resource(font);
        _font = &font;
    }

    void Context::use_default_font()
    {
        _font = _default_font;
    }

    void Context::set_font_size(int size)
    {
        _font_size = size;
    }

    int Context::get_width()
    {
        return _width;
    }

    int Context::get_height()
    {
        return _height;
    }

    Font& Context::get_default_font()
    {
        if (_default_font == nullptr)
        {
            throw std::logic_error("gCanvas default font is not initialized");
        }
        return *_default_font;
    }

    const CanvasMetrics& Context::metrics() const noexcept
    {
        return _metrics;
    }

    void Context::set_metrics(const CanvasMetrics& metrics)
    {
        if (metrics.width <= 0 || metrics.height <= 0 || metrics.scale_x <= 0.0f ||
            metrics.scale_y <= 0.0f || metrics.dpi_scale <= 0.0f)
        {
            throw std::invalid_argument("gCanvas canvas metrics must be positive");
        }
        _metrics = metrics;
        _width = metrics.width;
        _height = metrics.height;
    }

    Image& Context::own_image(std::unique_ptr<Image> image)
    {
        if (image == nullptr)
        {
            throw std::invalid_argument("gCanvas cannot own a null image");
        }
        if (_owned_images.size() >= _resource_limits.max_images)
        {
            throw std::length_error("gCanvas image limit reached");
        }
        image->_owner = this;
        Image& result = *image;
        _owned_images.push_back(std::move(image));
        return result;
    }

    Font& Context::own_font(std::unique_ptr<Font> font)
    {
        if (font == nullptr)
        {
            throw std::invalid_argument("gCanvas cannot own a null font");
        }
        if (_owned_fonts.size() >= _resource_limits.max_fonts)
        {
            throw std::length_error("gCanvas font limit reached");
        }
        font->_owner = this;
        if (font->_texture_atlas != nullptr)
        {
            font->_texture_atlas->_owner = this;
        }
        Font& result = *font;
        _owned_fonts.push_back(std::move(font));
        return result;
    }

    void Context::release_owned_resources() noexcept
    {
        _font = nullptr;
        _default_font = nullptr;
        _owned_fonts.clear();
        _cached_path_paints.clear();
        _owned_images.clear();
        _transient_path_paint_textures.clear();
        _transient_path_paint_texture_cursor = 0;
        _active_path_paint_textures = 0;
    }

    void Context::validate_resource(const Image& image) const
    {
        if (image._owner != this)
        {
            throw std::invalid_argument("image belongs to a different gCanvas context");
        }
    }

    void Context::validate_resource(const Font& font) const
    {
        if (font._owner != this)
        {
            throw std::invalid_argument("font belongs to a different gCanvas context");
        }
    }

    void Context::image_pixels_updated(Image& image)
    {
        validate_resource(image);
        update_image(&image);
    }
} // namespace gcanvas
