#include "context_impl_vulkan.hpp"
#include "gcanvas/backends/vulkan.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "../../resources.hpp"
#include "../../path_tessellator.hpp"
#include "../../utf8.hpp"
#include "font_impl_vulkan.hpp"
#include "vulkan_shared_info.hpp"

namespace gcanvas
{
    namespace
    {
        constexpr float analytic_ellipse_fill_effect = 5.0f;
        constexpr float analytic_ellipse_stroke_effect = 6.0f;

        template <typename Vector>
        void reserve_geometric(Vector& values, std::size_t required_capacity,
                               std::size_t maximum_capacity)
        {
            if (required_capacity <= values.capacity())
                return;
            const std::size_t growth = values.capacity() / 2U;
            const std::size_t geometric_capacity =
                values.capacity() > maximum_capacity - (std::max)(growth, std::size_t{1U})
                    ? maximum_capacity
                    : values.capacity() + (std::max)(growth, std::size_t{1U});
            values.reserve((std::min)(maximum_capacity,
                                      (std::max)(required_capacity, geometric_capacity)));
        }

        void reserve_sample_offsets(ContextImplVulkan& context, std::size_t additional)
        {
            if (additional > ContextImplVulkan::uniform_rect_capacity ||
                context.path_sample_offsets.size() >
                    ContextImplVulkan::uniform_rect_capacity - additional)
                throw std::length_error("Vulkan path sample offset storage limit reached");
            reserve_geometric(context.path_sample_offsets,
                              context.path_sample_offsets.size() + additional,
                              ContextImplVulkan::uniform_rect_capacity);
        }

        vec2 inset_uv_delta(const vec2& top_left, const vec2& top_right,
                            const vec2& bottom_left, int canvas_width, int canvas_height,
                            float device_offset_x, float device_offset_y,
                            const vec2& uv_minimum, const vec2& uv_maximum)
        {
            const vec2 edge_x = top_right - top_left;
            const vec2 edge_y = bottom_left - top_left;
            const float delta_x = device_offset_x * 2.0f / static_cast<float>(canvas_width);
            const float delta_y = device_offset_y * 2.0f / static_cast<float>(canvas_height);
            const float determinant = edge_x.get_x() * edge_y.get_y() -
                                      edge_x.get_y() * edge_y.get_x();
            if (!std::isfinite(determinant) || std::fabs(determinant) <= 1.0e-8f)
                throw std::invalid_argument("Vulkan inset alpha transform is singular");
            const float local_x =
                (delta_x * edge_y.get_y() - delta_y * edge_y.get_x()) / determinant;
            const float local_y =
                (edge_x.get_x() * delta_y - edge_x.get_y() * delta_x) / determinant;
            return vec2(local_x * (uv_maximum.get_x() - uv_minimum.get_x()),
                        local_y * (uv_maximum.get_y() - uv_minimum.get_y()));
        }

        ContextImplVulkan::uniform_rect path_uniform(
            const detail::MeshQuad& quad, int width, int height, color value,
            int sampler_index = -1, detail::MeshPoint uv_min = {0.0f, 0.0f},
            detail::MeshPoint uv_max = {1.0f, 1.0f})
        {
            if (width <= 0 || height <= 0)
                throw std::logic_error("Vulkan path draw requires a non-empty canvas");
            ContextImplVulkan::uniform_rect result{};
            result.color = value;
            for (std::size_t index = 0; index < quad.vertices.size(); ++index)
            {
                result.vertices[index] =
                    vec2(quad.vertices[index].x / static_cast<float>(width),
                         quad.vertices[index].y / static_cast<float>(height)) *
                        2.0f -
                    vec2(1.0f);
            }
            result.sampler_index = static_cast<float>(sampler_index);
            result.use_tint = sampler_index >= 0 ? 2.0f : 0.0f;
            result.resolution = vec2(static_cast<float>(width), static_cast<float>(height));
            result.uvs[0] = vec2(uv_min.x, uv_min.y);
            result.uvs[1] = vec2(uv_max.x, uv_max.y);
            return result;
        }

        void shift_uniform_vertices(ContextImplVulkan::uniform_rect& value, float x, float y)
        {
            for (vec2& vertex : value.vertices)
            {
                vertex.x() += x;
                vertex.y() += y;
            }
        }

        ContextImplVulkan::uniform_rect mask_triangle_uniform(
            const vec2& origin, const vec2& second, const vec2& third,
            const Transform& transform, int width, int height)
        {
            detail::MeshQuad quad{};
            const vec2 vertices[] = {origin, second, third, third};
            for (std::size_t index = 0; index < quad.vertices.size(); ++index)
            {
                transform.map(vertices[index].get_x(), vertices[index].get_y(),
                              quad.vertices[index].x, quad.vertices[index].y);
                if (!std::isfinite(quad.vertices[index].x) ||
                    !std::isfinite(quad.vertices[index].y))
                    throw std::invalid_argument("Vulkan convex mask transform overflow");
            }
            return path_uniform(quad, width, height, color(255, 255, 255, 255));
        }
    }

    /* ------------------------ DOWNCAST ------------------------ */

    static inline ContextImplVulkan* getImpl(Context* ptr)
    {
        return (ContextImplVulkan*)ptr;
    }
    static inline const ContextImplVulkan* getImpl(const Context* ptr)
    {
        return (const ContextImplVulkan*)ptr;
    }

    /* ------------------------ PUBLIC IMPLEMENTATION ------------------------ */

    std::unique_ptr<Context> vulkan::create_context(const CreateInfo& create_info)
    {
        return std::make_unique<ContextImplVulkan>(create_info);
    }

    std::unique_ptr<Context> vulkan::create_external_context(
        const ExternalCreateInfo& create_info)
    {
        return std::make_unique<ContextImplVulkan>(create_info);
    }

    void ContextImplVulkan::stroke_rect(float x, float y, float width, float height)
    {
        ContextImplVulkan* impl = getImpl(this);

        float lw = _line_width * impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        x += impl->_metrics.offset_x;
        y += impl->_metrics.offset_y;
        x = x * impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        y = y * impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        width = width * impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        height = height * impl->_metrics.scale_y * impl->_metrics.dpi_scale;

        float xf = (x / _width);
        float yf = (y / _height);
        float widthf = (width / _width);
        float heightf = (height / _height);

        impl->append_uniform({
            _stroke_color,                                                          // color
            {vec2(xf, yf) * 2.0f - vec2(1), vec2(xf + widthf, yf) * 2.0f - vec2(1), //
             vec2(xf, yf + heightf) * 2.0f - vec2(1),                               //
             vec2(xf + widthf, yf + heightf) * 2.0f - vec2(1)},                     // vertices
            {0, 0, 0, 0},                                                           // border_radius
            -1,                                                                     // sampler_index
            0,                                                                      // use_tint
            vec2(width, height),                                                    // resolution
            {vec2(0), vec2(1)},                                                     // uvs
            {lw, lw, lw, lw},                                                       // line_width
            0,                                                                      // shadow_size
            {0, 0, 0},                                                              // is_msdf
        });
    }

    void ContextImplVulkan::stroke_rounded_rect(float x, float y, float width, float height,
                                      float border_radius)
    {
        stroke_rounded_rect(x, y, width, height, border_radius, border_radius, border_radius,
                            border_radius);
    }

    void ContextImplVulkan::stroke_rounded_rect(float x, float y, float width, float height, float radius_nw,
                                      float radius_ne, float radius_se, float radius_sw)
    {
        ContextImplVulkan* impl = getImpl(this);

        float lw = _line_width * impl->_metrics.scale_x * impl->_metrics.dpi_scale;

        x += impl->_metrics.offset_x;
        y += impl->_metrics.offset_y;
        x *= impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        y *= impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        width *= impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        height *= impl->_metrics.scale_y * impl->_metrics.dpi_scale;

        float xf = (x / _width);
        float yf = (y / _height);
        float widthf = (width / _width);
        float heightf = (height / _height);

        float nwf = radius_nw * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float nef = radius_ne * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float sef = radius_se * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float swf = radius_sw * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);

        impl->append_uniform({
            _stroke_color,                                                          // color
            {vec2(xf, yf) * 2.0f - vec2(1), vec2(xf + widthf, yf) * 2.0f - vec2(1), //
             vec2(xf, yf + heightf) * 2.0f - vec2(1),                               //
             vec2(xf + widthf, yf + heightf) * 2.0f - vec2(1)},                     // vertices
            {nef, sef, nwf, swf},                                                   // border_radius
            -1,                                                                     // sampler_index
            0,                                                                      // use_tint
            vec2(width, height),                                                    // resolution
            {vec2(0), vec2(1)},                                                     // uvs
            {lw, lw, lw, lw},                                                       // line_width
            0,                                                                      // shadow_size
            {0, 0, 0},                                                              // is_msdf
        });
    }

    void ContextImplVulkan::stroke_circle(float x, float y, float radius)
    {
        ContextImplVulkan* impl = getImpl(this);

        float lw = _line_width * impl->_metrics.scale_x * impl->_metrics.dpi_scale;

        x += impl->_metrics.offset_x;
        y += impl->_metrics.offset_y;
        x *= impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        y *= impl->_metrics.scale_y * impl->_metrics.dpi_scale;

        float xf = ((x - radius * impl->_metrics.scale_x * impl->_metrics.dpi_scale) / _width);
        float yf = ((y - radius * impl->_metrics.scale_y * impl->_metrics.dpi_scale) / _height);
        float widthf =
            ((radius * 2 * impl->_metrics.scale_x * impl->_metrics.dpi_scale) / _width);
        float heightf =
            ((radius * 2 * impl->_metrics.scale_y * impl->_metrics.dpi_scale) / _height);
        float width = (radius * 2 * impl->_metrics.scale_x * impl->_metrics.dpi_scale);

        float radf = width / 2.0f;

        impl->append_uniform({
            _stroke_color,                                                          // color
            {vec2(xf, yf) * 2.0f - vec2(1), vec2(xf + widthf, yf) * 2.0f - vec2(1), //
             vec2(xf, yf + heightf) * 2.0f - vec2(1),                               //
             vec2(xf + widthf, yf + heightf) * 2.0f - vec2(1)},                     // vertices
            {radf, radf, radf, radf},                                               // border_radius
            -1,                                                                     // sampler_index
            0,                                                                      // use_tint
            vec2(width, width),                                                     // resolution
            {vec2(0), vec2(1)},                                                     // uvs
            {lw, lw, lw, lw},                                                       // line_width
            0,                                                                      // shadow_size
            {0, 0, 0},                                                              // is_msdf
        });
    }

    void ContextImplVulkan::stroke_ellipse(float x, float y, float radius_x, float radius_y)
    {
        ContextImplVulkan* impl = getImpl(this);
        const float metric_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        const float metric_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        const float line_width = _line_width * (std::max)(metric_x, metric_y);
        x = (x + impl->_metrics.offset_x) * metric_x;
        y = (y + impl->_metrics.offset_y) * metric_y;
        const float width = radius_x * 2.0f * metric_x;
        const float height = radius_y * 2.0f * metric_y;
        const float left = (x - width * 0.5f) / _width;
        const float top = (y - height * 0.5f) / _height;
        const float normalized_width = width / _width;
        const float normalized_height = height / _height;
        impl->append_uniform({
            _stroke_color,
            {vec2(left, top) * 2.0f - vec2(1),
             vec2(left + normalized_width, top) * 2.0f - vec2(1),
             vec2(left, top + normalized_height) * 2.0f - vec2(1),
             vec2(left + normalized_width, top + normalized_height) * 2.0f - vec2(1)},
            {0, 0, 0, 0}, -1, 0, vec2(width, height), {vec2(0), vec2(1)},
            {line_width, line_width, line_width, line_width}, 0,
            {0, 0, analytic_ellipse_stroke_effect},
        });
    }

    void ContextImplVulkan::fill_rect(float x, float y, float width, float height)
    {
        ContextImplVulkan* impl = getImpl(this);

        x += impl->_metrics.offset_x;
        y += impl->_metrics.offset_y;
        x *= impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        y *= impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        width *= impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        height *= impl->_metrics.scale_y * impl->_metrics.dpi_scale;

        float xf = (x / _width);
        float yf = (y / _height);
        float widthf = (width / _width);
        float heightf = (height / _height);

        impl->append_uniform({
            _fill_color,                                                            // color
            {vec2(xf, yf) * 2.0f - vec2(1), vec2(xf + widthf, yf) * 2.0f - vec2(1), //
             vec2(xf, yf + heightf) * 2.0f - vec2(1),                               //
             vec2(xf + widthf, yf + heightf) * 2.0f - vec2(1)},                     // vertices
            {0, 0, 0, 0},                                                           // border_radius
            -1,                                                                     // sampler_index
            0,                                                                      // use_tint
            vec2(width, height),                                                    // resolution
            {vec2(0), vec2(1)},                                                     // uvs
            {0, 0, 0, 0},                                                           // line_width
            0,                                                                      // shadow_size
            {0, 0, 0},                                                              // is_msdf
        });
    }

    void ContextImplVulkan::fill_rounded_rect(float x, float y, float width, float height,
                                    float border_radius)
    {
        fill_rounded_rect(x, y, width, height, border_radius, border_radius, border_radius,
                          border_radius);
    }

    void ContextImplVulkan::fill_rounded_rect(float x, float y, float width, float height, float radius_nw,
                                    float radius_ne, float radius_se, float radius_sw)
    {
        ContextImplVulkan* impl = getImpl(this);

        x += impl->_metrics.offset_x;
        y += impl->_metrics.offset_y;
        x *= impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        y *= impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        width *= impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        height *= impl->_metrics.scale_y * impl->_metrics.dpi_scale;

        float xf = (x / _width);
        float yf = (y / _height);
        float widthf = (width / _width);
        float heightf = (height / _height);

        float nwf = radius_nw * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float nef = radius_ne * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float sef = radius_se * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float swf = radius_sw * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);

        impl->append_uniform({
            _fill_color,                                                            // color
            {vec2(xf, yf) * 2.0f - vec2(1), vec2(xf + widthf, yf) * 2.0f - vec2(1), //
             vec2(xf, yf + heightf) * 2.0f - vec2(1),                               //
             vec2(xf + widthf, yf + heightf) * 2.0f - vec2(1)},                     // vertices
            {nef, sef, nwf, swf},                                                   // border_radius
            -1,                                                                     // sampler_index
            0,                                                                      // use_tint
            vec2(width, height),                                                    // resolution
            {vec2(0), vec2(1)},                                                     // uvs
            {0, 0, 0, 0},                                                           // line_width
            0,                                                                      // shadow_size
            {0, 0, 0},                                                              // is_msdf
        });
    }

    void ContextImplVulkan::fill_circle(float x, float y, float radius)
    {
        ContextImplVulkan* impl = getImpl(this);

        x += impl->_metrics.offset_x;
        y += impl->_metrics.offset_y;
        x *= impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        y *= impl->_metrics.scale_y * impl->_metrics.dpi_scale;

        float xf = ((x - radius * impl->_metrics.scale_x * impl->_metrics.dpi_scale) / _width);
        float yf = ((y - radius * impl->_metrics.scale_y * impl->_metrics.dpi_scale) / _height);
        float widthf =
            ((radius * 2 * impl->_metrics.scale_x * impl->_metrics.dpi_scale) / _width);
        float heightf =
            ((radius * 2 * impl->_metrics.scale_y * impl->_metrics.dpi_scale) / _height);

        float width = (radius * 2 * impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float radf = width / 2.0f;

        impl->append_uniform({
            _fill_color,                                                            // color
            {vec2(xf, yf) * 2.0f - vec2(1), vec2(xf + widthf, yf) * 2.0f - vec2(1), //
             vec2(xf, yf + heightf) * 2.0f - vec2(1),                               //
             vec2(xf + widthf, yf + heightf) * 2.0f - vec2(1)},                     // vertices
            {radf, radf, radf, radf},                                               // border_radius
            -1,                                                                     // sampler_index
            0,                                                                      // use_tint
            vec2(width, width),                                                     // resolution
            {vec2(0), vec2(1)},                                                     // uvs
            {0, 0, 0, 0},                                                           // line_width
            0,                                                                      // shadow_size
            {0, 0, 0},                                                              // is_msdf
        });
    }

    void ContextImplVulkan::fill_ellipse(float x, float y, float radius_x, float radius_y)
    {
        ContextImplVulkan* impl = getImpl(this);
        const float metric_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        const float metric_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        x = (x + impl->_metrics.offset_x) * metric_x;
        y = (y + impl->_metrics.offset_y) * metric_y;
        const float width = radius_x * 2.0f * metric_x;
        const float height = radius_y * 2.0f * metric_y;
        const float left = (x - width * 0.5f) / _width;
        const float top = (y - height * 0.5f) / _height;
        const float normalized_width = width / _width;
        const float normalized_height = height / _height;
        impl->append_uniform({
            _fill_color,
            {vec2(left, top) * 2.0f - vec2(1),
             vec2(left + normalized_width, top) * 2.0f - vec2(1),
             vec2(left, top + normalized_height) * 2.0f - vec2(1),
             vec2(left + normalized_width, top + normalized_height) * 2.0f - vec2(1)},
            {0, 0, 0, 0}, -1, 0, vec2(width, height), {vec2(0), vec2(1)},
            {0, 0, 0, 0}, 0, {0, 0, analytic_ellipse_fill_effect},
        });
    }

    void ContextImplVulkan::draw_text(float x, float y, std::string text)
    {
        std::u32string unicode_codepoints = detail::decode_utf8(text);
        draw_text(x, y, unicode_codepoints, Transform{});
    }

    void ContextImplVulkan::draw_text(float x, float y, std::string text,
                                      const Transform& transform)
    {
        std::u32string unicode_codepoints = detail::decode_utf8(text);
        draw_text(x, y, unicode_codepoints, transform);
    }

    void ContextImplVulkan::draw_text(float x, float y, std::u32string text)
    {
        draw_text(x, y, text, Transform{});
    }

    void ContextImplVulkan::draw_text(float x, float y, std::u32string text,
                                      const Transform& transform)
    {
        draw_text_impl(x, y, text, transform, false, nullptr);
    }

    void ContextImplVulkan::draw_text_alpha_mask(float x, float y, std::u32string text,
                                                 float erosion_radius,
                                                 const Transform& transform)
    {
        draw_text_impl(x, y, text, transform, true, nullptr, erosion_radius);
    }

    void ContextImplVulkan::draw_text_inset_alpha_mask(float x, float y,
                                                        std::u32string text,
                                                        float sample_offset_x,
                                                        float sample_offset_y,
                                                        float dilation_radius,
                                                        const Transform& transform)
    {
        const vec2 offset(sample_offset_x, sample_offset_y);
        draw_text_impl(x, y, text, transform, true, &offset, dilation_radius);
    }

    void ContextImplVulkan::draw_text_impl(float x, float y, const std::u32string& text,
                                           const Transform& transform, bool alpha_mask,
                                           const vec2* inset_sample_offset,
                                           float morphology_radius)
    {
        ContextImplVulkan* impl = getImpl(this);
        if (!std::isfinite(x) || !std::isfinite(y))
            throw std::invalid_argument("gCanvas affine text position must be finite");
        validate_transform(transform);

        if (_font == nullptr)
        {
            std::cerr << "Error: No font loaded!" << std::endl;
            return;
        }

        float initialX = x;
        float scale = (float)_font_size / LOADED_HEIGHT;
        const std::map<char32_t, character>& characters = _font->get_characters();
        auto* img = static_cast<ImageImplVulkan*>(&_font->get_image());
        character fallback_character{};
        const auto fallback = characters.find(0);
        if (fallback != characters.end())
            fallback_character = fallback->second;

        for (const char32_t token : text)
        {
            character ch = fallback_character;
            if (token > 0)
            {
                const auto glyph = characters.find(token);
                ch = glyph == characters.end() ? character{} : glyph->second;
            }

            if (token == '\n')
            {
                y += _font->get_line_height() * scale;
                x = initialX;
                continue;
            }

            float xpos = x + ch.bearing.x() * scale;
            float ypos = y + (LOADED_HEIGHT - ch.bearing.y()) * scale;

            float width = ch.size.x() * scale;
            float height = ch.size.y() * scale;
            if (width <= 0.0f || height <= 0.0f)
            {
                x += ch.advance * scale;
                continue;
            }
            const ImageQuad quad = make_image_quad(xpos, ypos, width, height, transform);

            float originx = ch.origin.x() / img->get_width();
            float originy = ch.origin.y() / img->get_height();
            float cropx = originx + ch.size.x() / img->get_width();
            float cropy = originy + ch.size.y() / img->get_height();
            const vec2 uv_minimum(originx, originy);
            const vec2 uv_maximum(cropx, cropy);
            vec2 inset_delta{};
            if (inset_sample_offset != nullptr)
            {
                inset_delta = inset_uv_delta(
                    quad.top_left, quad.top_right, quad.bottom_left, _width, _height,
                    inset_sample_offset->get_x() * impl->_metrics.scale_x *
                        impl->_metrics.dpi_scale,
                    inset_sample_offset->get_y() * impl->_metrics.scale_y *
                        impl->_metrics.dpi_scale,
                    uv_minimum, uv_maximum);
            }
            vec2 morphology_x{};
            vec2 morphology_y{};
            if (morphology_radius > 0.0f)
            {
                const float metric_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
                const float metric_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
                morphology_x = inset_uv_delta(
                    quad.top_left, quad.top_right, quad.bottom_left, _width, _height,
                    morphology_radius * metric_x * 0.5f, 0.0f, uv_minimum, uv_maximum);
                morphology_y = inset_uv_delta(
                    quad.top_left, quad.top_right, quad.bottom_left, _width, _height,
                    0.0f, morphology_radius * metric_y * 0.5f, uv_minimum, uv_maximum);
            }

            impl->append_uniform({
                _fill_color,                                                            // color
                {quad.top_left, quad.top_right, quad.bottom_left, quad.bottom_right},    // vertices
                {morphology_x.get_x(), morphology_x.get_y(),
                 morphology_y.get_x(), morphology_y.get_y()}, // morphology basis
                (float)img->_sampler_index,                   // sampler_index
                alpha_mask ? 3.0f : (ch.has_color ? 2.0f : 1.0f), // source or alpha mask
                vec2(width, height),                          // resolution
                {uv_minimum, uv_maximum},                     // uvs
                {inset_delta.get_x(), inset_delta.get_y(), 0, 0}, // inset sample
                0,                                            // shadow_size
                {0, 0, inset_sample_offset != nullptr ? 3.0f
                                                      : (morphology_radius > 0.0f ? 4.0f : 0.0f)},
            });

            x += ch.advance * scale;
        }
    }

    void ContextImplVulkan::draw_image(float x, float y, float width, float height, Image& image,
                                      bool tint)
    {
        draw_rounded_image(x, y, width, height, image, 0, 0, 0, 0, tint);
    }

    void ContextImplVulkan::draw_image(float x, float y, float width, float height, Image& image,
                                       const Transform& transform, bool tint)
    {
        ContextImplVulkan* impl = getImpl(this);
        validate_resource(image);
        const ImageQuad quad = make_image_quad(x, y, width, height, transform);
        if (width == 0.0f || height == 0.0f)
            return;
        auto* img = static_cast<ImageImplVulkan*>(&image);

        impl->append_uniform({
            _fill_color,
            {quad.top_left, quad.top_right, quad.bottom_left, quad.bottom_right},
            {0, 0, 0, 0},
            static_cast<float>(img->_sampler_index),
            static_cast<float>(tint),
            quad.resolution,
            {vec2(0), vec2(1)},
            {0, 0, 0, 0},
            0,
            {0, 0, 0},
        });
    }

    void ContextImplVulkan::draw_image_alpha_mask(float x, float y, float width, float height,
                                                   Image& image, float erosion_radius,
                                                   const Transform& transform)
    {
        ContextImplVulkan* impl = getImpl(this);
        validate_resource(image);
        const ImageQuad quad = make_image_quad(x, y, width, height, transform);
        if (width == 0.0f || height == 0.0f)
            return;
        auto* img = static_cast<ImageImplVulkan*>(&image);
        const vec2 uv_minimum(0.0f);
        const vec2 uv_maximum(1.0f);
        vec2 morphology_x{};
        vec2 morphology_y{};
        if (erosion_radius > 0.0f)
        {
            const float metric_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
            const float metric_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
            morphology_x = inset_uv_delta(
                quad.top_left, quad.top_right, quad.bottom_left, _width, _height,
                erosion_radius * metric_x * 0.5f, 0.0f, uv_minimum, uv_maximum);
            morphology_y = inset_uv_delta(
                quad.top_left, quad.top_right, quad.bottom_left, _width, _height,
                0.0f, erosion_radius * metric_y * 0.5f, uv_minimum, uv_maximum);
        }
        impl->append_uniform({
            _fill_color,
            {quad.top_left, quad.top_right, quad.bottom_left, quad.bottom_right},
            {morphology_x.get_x(), morphology_x.get_y(),
             morphology_y.get_x(), morphology_y.get_y()},
            static_cast<float>(img->_sampler_index),
            3.0f,
            quad.resolution,
            {uv_minimum, uv_maximum},
            {0, 0, 0, 0},
            0,
            {0, 0, erosion_radius > 0.0f ? 4.0f : 0.0f},
        });
    }

    void ContextImplVulkan::draw_image_inset_alpha_mask(
        float x, float y, float width, float height, Image& image, float sample_offset_x,
        float sample_offset_y, float dilation_radius, const Transform& transform)
    {
        ContextImplVulkan* impl = getImpl(this);
        validate_resource(image);
        const ImageQuad quad = make_image_quad(x, y, width, height, transform);
        if (width == 0.0f || height == 0.0f)
            return;
        auto* img = static_cast<ImageImplVulkan*>(&image);
        const vec2 uv_minimum(0.0f);
        const vec2 uv_maximum(1.0f);
        const vec2 inset_delta = inset_uv_delta(
            quad.top_left, quad.top_right, quad.bottom_left, _width, _height,
            sample_offset_x * impl->_metrics.scale_x * impl->_metrics.dpi_scale,
            sample_offset_y * impl->_metrics.scale_y * impl->_metrics.dpi_scale,
            uv_minimum, uv_maximum);
        vec2 morphology_x{};
        vec2 morphology_y{};
        if (dilation_radius > 0.0f)
        {
            const float metric_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
            const float metric_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
            morphology_x = inset_uv_delta(
                quad.top_left, quad.top_right, quad.bottom_left, _width, _height,
                dilation_radius * metric_x * 0.5f, 0.0f, uv_minimum, uv_maximum);
            morphology_y = inset_uv_delta(
                quad.top_left, quad.top_right, quad.bottom_left, _width, _height,
                0.0f, dilation_radius * metric_y * 0.5f, uv_minimum, uv_maximum);
        }
        impl->append_uniform({
            _fill_color,
            {quad.top_left, quad.top_right, quad.bottom_left, quad.bottom_right},
            {morphology_x.get_x(), morphology_x.get_y(),
             morphology_y.get_x(), morphology_y.get_y()},
            static_cast<float>(img->_sampler_index),
            3.0f,
            quad.resolution,
            {uv_minimum, uv_maximum},
            {inset_delta.get_x(), inset_delta.get_y(), 0, 0},
            0,
            {0, 0, 3.0f},
        });
    }

    void ContextImplVulkan::draw_image(float x, float y, float width, float height, Image& image, float src_x,
                             float src_y, float src_width, float src_height, bool tint)
    {
        draw_rounded_image(x, y, width, height, image, 0, 0, 0, 0, src_x, src_y, src_width,
                           src_height, tint);
    }

    void ContextImplVulkan::draw_rounded_image(float x, float y, float width, float height, Image& image,
                                     float border_radius, bool tint)
    {
        draw_rounded_image(x, y, width, height, image, border_radius, border_radius, border_radius,
                           border_radius, tint);
    }

    void ContextImplVulkan::draw_rounded_image(float x, float y, float width, float height, Image& image,
                                     float radius_nw, float radius_ne, float radius_se,
                                     float radius_sw, bool tint)
    {
        draw_rounded_image(x, y, width, height, image, radius_nw, radius_ne, radius_se, radius_sw,
                           0, 0, (float)image.get_width(), (float)image.get_height(), tint);
    }

    void ContextImplVulkan::draw_rounded_image(float x, float y, float width, float height, Image& image,
                                     float radius_nw, float radius_ne, float radius_se,
                                     float radius_sw, float src_x, float src_y, float src_width,
                                     float src_height, bool tint)
    {
        ContextImplVulkan* impl = getImpl(this);
        validate_resource(image);
        auto* img = static_cast<ImageImplVulkan*>(&image);

        x += impl->_metrics.offset_x;
        y += impl->_metrics.offset_y;
        x *= impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        y *= impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        width *= impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        height *= impl->_metrics.scale_y * impl->_metrics.dpi_scale;

        float xf = (x / _width);
        float yf = (y / _height);
        float widthf = (width / _width);
        float heightf = (height / _height);

        float nwf = radius_nw * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float nef = radius_ne * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float sef = radius_se * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float swf = radius_sw * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);

        float originx = src_x / img->get_width();
        float originy = src_y / img->get_height();
        float cropx = originx + src_width / img->get_width();
        float cropy = originy + src_height / img->get_height();

        impl->append_uniform({
            _fill_color,                                                            // color
            {vec2(xf, yf) * 2.0f - vec2(1), vec2(xf + widthf, yf) * 2.0f - vec2(1), //
             vec2(xf, yf + heightf) * 2.0f - vec2(1),                               //
             vec2(xf + widthf, yf + heightf) * 2.0f - vec2(1)},                     // vertices
            {nef, sef, nwf, swf},                                                   // border_radius
            (float)img->_sampler_index,                                             // sampler_index
            (float)tint,                                                            // use_tint
            vec2(width, height),                                                    // resolution
            {vec2(originx, originy), vec2(cropx, cropy)},                           // uvs
            {0, 0, 0, 0},                                                           // line_width
            0,                                                                      // shadow_size
            {0, 0, 0},                                                              // is_msdf
        });
    }

    void ContextImplVulkan::draw_rect_shadow(float x, float y, float width, float height,
                                              float blur_radius)
    {
        ContextImplVulkan* impl = getImpl(this);
        const ShadowQuad shadow = make_shadow_quad(x, y, width, height, blur_radius);
        if (width == 0.0f || height == 0.0f)
            return;
        const ImageQuad& quad = shadow.geometry;

        impl->append_uniform({
            _fill_color,
            {quad.top_left, quad.top_right, quad.bottom_left, quad.bottom_right},
            {0, 0, 0, 0},
            -1,
            0,
            quad.resolution,
            {vec2(0), vec2(1)},
            {0, 0, 0, 0},
            shadow.blur_radius.get_x(),
            {0, shadow.blur_radius.get_y(), 0},
        });
    }

    void ContextImplVulkan::draw_rounded_rect_shadow(float x, float y, float width, float height,
                                                       float border_radius, float blur_radius)
    {
        draw_rounded_rect_shadow(x, y, width, height, border_radius, border_radius, border_radius,
                                 border_radius, blur_radius);
    }

    void ContextImplVulkan::draw_rounded_rect_shadow(float x, float y, float width, float height,
                                                       float radius_nw, float radius_ne,
                                                       float radius_se, float radius_sw,
                                                       float blur_radius)
    {
        ContextImplVulkan* impl = getImpl(this);
        validate_shadow_radii(radius_nw, radius_ne, radius_se, radius_sw);
        const ShadowQuad shadow = make_shadow_quad(x, y, width, height, blur_radius);
        if (width == 0.0f || height == 0.0f)
            return;
        const ImageQuad& quad = shadow.geometry;

        float nwf = radius_nw * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float nef = radius_ne * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float sef = radius_se * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float swf = radius_sw * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);

        impl->append_uniform({
            _fill_color,
            {quad.top_left, quad.top_right, quad.bottom_left, quad.bottom_right},
            {nef, sef, nwf, swf},
            -1,
            0,
            quad.resolution,
            {vec2(0), vec2(1)},
            {0, 0, 0, 0},
            shadow.blur_radius.get_x(),
            {0, shadow.blur_radius.get_y(), 0},
        });
    }

    void ContextImplVulkan::draw_circle_shadow(float x, float y, float radius,
                                                float blur_radius)
    {
        ContextImplVulkan* impl = getImpl(this);
        const ShadowQuad shadow =
            make_shadow_quad(x - radius, y - radius, radius * 2.0f, radius * 2.0f,
                             blur_radius);
        if (radius == 0.0f)
            return;
        const ImageQuad& quad = shadow.geometry;
        const float width = shadow.geometry.resolution.get_x();
        float radf = width / 2.0f;

        impl->append_uniform({
            _fill_color,
            {quad.top_left, quad.top_right, quad.bottom_left, quad.bottom_right},
            {radf, radf, radf, radf},
            -1,
            0,
            vec2(width, width),
            {vec2(0), vec2(1)},
            {0, 0, 0, 0},
            shadow.blur_radius.get_x(),
            {0, shadow.blur_radius.get_x(), 0},
        });
    }

    void ContextImplVulkan::draw_inset_shadow(const InsetShadow& shadow)
    {
        ContextImplVulkan* impl = getImpl(this);
        const ImageQuad quad = make_image_quad(shadow.x, shadow.y, shadow.width, shadow.height,
                                               Transform{});
        const float metric_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        const float metric_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        const float metric_max = (std::max)(std::fabs(metric_x), std::fabs(metric_y));
        impl->append_uniform({
            _fill_color,
            {quad.top_left, quad.top_right, quad.bottom_left, quad.bottom_right},
            {shadow.radius * metric_x, shadow.radius * metric_x,
             shadow.radius * metric_x, shadow.radius * metric_x},
            -1,
            0,
            quad.resolution,
            {vec2(0), vec2(1)},
            {shadow.offset_x * metric_x, shadow.offset_y * metric_y,
             shadow.spread_radius * metric_max, 0.0f},
            shadow.blur_radius * metric_max,
            {0, 0, shadow.ellipse ? 2.0f : 1.0f},
        });
    }

    void ContextImplVulkan::set_clear_color(color color)
    {
        ContextImplVulkan* impl = getImpl(this);
        _clear_color = color;
        impl->clearValue.color = {color.rf(), color.gf(), color.bf(), color.af()};
    }

    void ContextImplVulkan::set_rect_mask(float x, float y, float width, float height)
    {
        validate_rect_mask(x, y, width, height);
        ContextImplVulkan* impl = getImpl(this);
        const float scale_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        const float scale_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        const auto clamp_x = [&](float value) {
            return (std::min)(static_cast<float>(_width), (std::max)(0.0f, value));
        };
        const auto clamp_y = [&](float value) {
            return (std::min)(static_cast<float>(_height), (std::max)(0.0f, value));
        };
        const float left = clamp_x(x * scale_x);
        const float top = clamp_y(y * scale_y);
        const float right = clamp_x((x + width) * scale_x);
        const float bottom = clamp_y((y + height) * scale_y);
        const auto s_x = static_cast<std::int32_t>(std::floor(left));
        const auto s_y = static_cast<std::int32_t>(std::floor(top));
        const auto s_right = static_cast<std::uint32_t>(
            std::ceil((std::max)(left, right)));
        const auto s_bottom = static_cast<std::uint32_t>(
            std::ceil((std::max)(top, bottom)));
        const auto s_width = s_right - static_cast<std::uint32_t>(s_x);
        const auto s_height = s_bottom - static_cast<std::uint32_t>(s_y);

        impl->draw_sequence_chain.push_back(
            {(int)impl->storage.size(), {{s_x, s_y}, {s_width, s_height}}});
    }

    void ContextImplVulkan::set_convex_mask(const std::vector<vec2>& vertices)
    {
        validate_convex_mask(vertices);
        ContextImplVulkan* impl = getImpl(this);
        const std::size_t triangle_count = vertices.size() >= 3U ? vertices.size() - 2U : 0U;
        if (impl->storage.size() + triangle_count > uniform_rect_capacity ||
            impl->storage.size() + triangle_count >
                static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
            throw std::length_error("Vulkan convex mask storage limit reached");

        const auto first = static_cast<std::uint32_t>(impl->storage.size());
        const Transform device_transform =
            Transform::scaling(impl->_metrics.scale_x * impl->_metrics.dpi_scale,
                               impl->_metrics.scale_y * impl->_metrics.dpi_scale) *
            Transform::translation(impl->_metrics.offset_x, impl->_metrics.offset_y);
        for (std::size_t index = 1U; index + 1U < vertices.size(); ++index)
            impl->append_uniform(mask_triangle_uniform(
                vertices.front(), vertices[index], vertices[index + 1U], device_transform,
                _width, _height));

        impl->draw_sequence_chain.push_back(
            {static_cast<int>(impl->storage.size()),
             {{0, 0}, {static_cast<std::uint32_t>(_width),
                       static_cast<std::uint32_t>(_height)}},
             first, static_cast<std::uint32_t>(triangle_count), true});
    }

    void ContextImplVulkan::remove_rect_mask()
    {
        ContextImplVulkan* impl = getImpl(this);
        impl->draw_sequence_chain.push_back(
            {(int)impl->storage.size(), {{0, 0}, {(uint32_t)_width, (uint32_t)_height}}});
    }

    void ContextImplVulkan::draw_frame()
    {
        ContextImplVulkan* impl = getImpl(this);
        if (impl->external_target_ != nullptr)
        {
            impl->record_external_frame();
            return;
        }
        if (impl->rendering)
            return;
        if (impl->headless)
        {
            impl->clear_draw_queue();
            reset_transient_path_resources();
            return;
        }
        if (impl->storage.empty())
            return;

        frame_resources& frame = impl->frameResources[impl->currentFrame];
        vku::err_check(vkWaitForFences(VulkanSharedInfo::getInstance()->device, 1,
                                       &frame.render_fence, VK_TRUE,
                                       std::numeric_limits<std::uint64_t>::max()));

        impl->imageIndex = invalid_image_index;
        const VkResult acquire_result = vkAcquireNextImageKHR(
            VulkanSharedInfo::getInstance()->device, impl->swapchain,
            std::numeric_limits<uint64_t>::max(), frame.image_available, VK_NULL_HANDLE,
            &impl->imageIndex);
        if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            impl->clear_draw_queue();
            reset_transient_path_resources();
            impl->update_swapchain(static_cast<std::uint32_t>(_width),
                                   static_cast<std::uint32_t>(_height));
            return;
        }
        if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR)
            vku::err_check(acquire_result);
        impl->swapchain_suboptimal = acquire_result == VK_SUBOPTIMAL_KHR;

        VkFence& image_fence = impl->imagesInFlight[impl->imageIndex];
        if (image_fence != VK_NULL_HANDLE && image_fence != frame.render_fence)
        {
            vku::err_check(vkWaitForFences(VulkanSharedInfo::getInstance()->device, 1,
                                           &image_fence, VK_TRUE,
                                           std::numeric_limits<std::uint64_t>::max()));
        }
        image_fence = frame.render_fence;

        impl->update_storage(frame);
        impl->record_command_buffer(impl->imageIndex, frame.descriptor_set);
        impl->dirty = false;

        VkPipelineStageFlags waitStageMask[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &frame.image_available;
        submitInfo.pWaitDstStageMask = waitStageMask;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &(impl->commandBuffers[impl->imageIndex]);
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &frame.rendering_complete;

        vku::err_check(vkResetFences(VulkanSharedInfo::getInstance()->device, 1,
                                     &frame.render_fence));
        vku::err_check(vkQueueSubmit(impl->queue, 1, &submitInfo, frame.render_fence));
        impl->rendering = true;
        impl->clear_draw_queue();
    }

    void ContextImplVulkan::present_frame()
    {
        ContextImplVulkan* impl = getImpl(this);
        if (impl->external_target_ != nullptr)
            return;
        if (!impl->rendering || impl->imageIndex == invalid_image_index)
            return;

        frame_resources& frame = impl->frameResources[impl->currentFrame];
        VkPresentInfoKHR presentInfoKHR{};
        presentInfoKHR.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfoKHR.pNext = nullptr;
        presentInfoKHR.waitSemaphoreCount = 1;
        presentInfoKHR.pWaitSemaphores = &frame.rendering_complete;
        presentInfoKHR.swapchainCount = 1;
        presentInfoKHR.pSwapchains = &(impl->swapchain);
        presentInfoKHR.pImageIndices = &impl->imageIndex;
        presentInfoKHR.pResults = nullptr;

        const VkResult present_result = vkQueuePresentKHR(impl->queue, &presentInfoKHR);
        const bool recreate_swapchain = impl->swapchain_suboptimal ||
                                        present_result == VK_ERROR_OUT_OF_DATE_KHR ||
                                        present_result == VK_SUBOPTIMAL_KHR;

        reset_transient_path_resources();
        impl->reset_frame_state();
        impl->currentFrame = (impl->currentFrame + 1U) % frames_in_flight;
        impl->swapchain_suboptimal = false;

        if (present_result != VK_SUCCESS && present_result != VK_ERROR_OUT_OF_DATE_KHR &&
            present_result != VK_SUBOPTIMAL_KHR)
            vku::err_check(present_result);
        if (recreate_swapchain && !impl->headless)
            impl->update_swapchain(static_cast<std::uint32_t>(_width),
                                   static_cast<std::uint32_t>(_height));
    }

    std::vector<std::uint8_t> ContextImplVulkan::read_pixels()
    {
        if (external_target_ != nullptr)
            throw std::logic_error("External Vulkan targets are read back by the host");
        if (!rendering || imageIndex == invalid_image_index || headless)
        {
            throw std::logic_error("Vulkan readback requires a submitted frame");
        }
        frame_resources& frame = frameResources[currentFrame];
        vku::err_check(vkWaitForFences(VulkanSharedInfo::getInstance()->device, 1,
                                       &frame.render_fence, VK_TRUE,
                                       std::numeric_limits<std::uint64_t>::max()));

        const auto pixel_count = static_cast<std::size_t>(swapchainExtent.width) *
                                 static_cast<std::size_t>(swapchainExtent.height);
        if (pixel_count > std::numeric_limits<std::size_t>::max() / 4U)
        {
            throw std::overflow_error("Vulkan readback size overflow");
        }
        const VkDeviceSize byte_count = static_cast<VkDeviceSize>(pixel_count * 4U);
        VkBuffer staging_buffer = VK_NULL_HANDLE;
        VmaAllocation staging_allocation = nullptr;
        vku::create_buffer(byte_count, VK_BUFFER_USAGE_TRANSFER_DST_BIT, staging_buffer,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           staging_allocation);
        const auto release_staging = [&]() noexcept {
            if (staging_buffer != VK_NULL_HANDLE)
                vmaDestroyBuffer(VulkanSharedInfo::getInstance()->allocator, staging_buffer,
                                 staging_allocation);
        };

        std::vector<std::uint8_t> pixels;
        try
        {
        VkCommandBuffer command = vku::beginSingleTimeCommands(commandPool);
        VkImageMemoryBarrier to_transfer{};
        to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        to_transfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        to_transfer.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_transfer.image = swapchainImages[imageIndex];
        to_transfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        to_transfer.subresourceRange.baseMipLevel = 0;
        to_transfer.subresourceRange.levelCount = 1;
        to_transfer.subresourceRange.baseArrayLayer = 0;
        to_transfer.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &to_transfer);

        VkBufferImageCopy copy_region{};
        copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_region.imageSubresource.mipLevel = 0;
        copy_region.imageSubresource.baseArrayLayer = 0;
        copy_region.imageSubresource.layerCount = 1;
        copy_region.imageExtent = {swapchainExtent.width, swapchainExtent.height, 1};
        vkCmdCopyImageToBuffer(command, swapchainImages[imageIndex],
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buffer, 1,
                               &copy_region);

        VkImageMemoryBarrier to_present = to_transfer;
        to_present.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        to_present.dstAccessMask = 0;
        to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &to_present);
        vku::endSingleTimeCommands(command, commandPool, queue);

        pixels.resize(static_cast<std::size_t>(byte_count));
        void* mapped = nullptr;
        vku::err_check(vmaMapMemory(VulkanSharedInfo::getInstance()->allocator,
                                    staging_allocation, &mapped));
        std::memcpy(pixels.data(), mapped, pixels.size());
        vmaUnmapMemory(VulkanSharedInfo::getInstance()->allocator, staging_allocation);
        }
        catch (...)
        {
            release_staging();
            throw;
        }
        release_staging();

        if (selectedImageFormat == VK_FORMAT_B8G8R8A8_UNORM ||
            selectedImageFormat == VK_FORMAT_B8G8R8A8_SRGB)
        {
            for (std::size_t offset = 0; offset < pixels.size(); offset += 4U)
            {
                std::swap(pixels[offset], pixels[offset + 2U]);
            }
        }
        else if (selectedImageFormat != VK_FORMAT_R8G8B8A8_UNORM &&
                 selectedImageFormat != VK_FORMAT_R8G8B8A8_SRGB)
        {
            throw std::runtime_error("Vulkan swapchain format is not RGBA8/BGRA8");
        }
        return pixels;
    }

    void ContextImplVulkan::resize_context(int width, int height)
    {
        if (external_target_ != nullptr)
            throw std::logic_error("External Vulkan target extent is owned by the host");
        if (width < 0 || height < 0)
            throw std::invalid_argument("Vulkan framebuffer dimensions must be non-negative");
        ContextImplVulkan* impl = getImpl(this);
        impl->dirty = true;
        if (width == 0 || height == 0)
        {
            impl->headless = true;
        }
        else
        {
            impl->headless = false;
            impl->update_swapchain((uint32_t)width, (uint32_t)height);
        }
    }

    Image& ContextImplVulkan::create_image(const std::string& file_path, ImageConfig config)
    {
        if (_owned_images.size() >= _resource_limits.max_images)
        {
            throw std::length_error("gCanvas image limit reached");
        }
        auto owned = std::make_unique<ImageImplVulkan>(file_path, config);
        Image& image = *owned;
        register_image(&image);
        return own_image(std::move(owned));
    }

    void ContextImplVulkan::set_vsync(bool enabled)
    {
        if (external_target_ != nullptr)
            throw std::logic_error("External Vulkan presentation policy is owned by the host");
        if (_vsync == enabled)
        {
            return;
        }
        _vsync = enabled;
        if (!headless)
        {
            update_swapchain(static_cast<std::uint32_t>(_width),
                             static_cast<std::uint32_t>(_height));
        }
    }

    Image& ContextImplVulkan::create_image(int width, int height, int components,
                                           const unsigned char* data, std::size_t size,
                                           ImageConfig config)
    {
        if (_owned_images.size() >= _resource_limits.max_images)
        {
            throw std::length_error("gCanvas image limit reached");
        }
        auto owned =
            std::make_unique<ImageImplVulkan>(width, height, components, data, size, config);
        Image& image = *owned;
        register_image(&image);
        return own_image(std::move(owned));
    }

    Font& ContextImplVulkan::create_font(const std::string& file_path)
    {
        if (_owned_fonts.size() >= _resource_limits.max_fonts)
        {
            throw std::length_error("gCanvas font limit reached");
        }
        auto owned = std::make_unique<FontImplVulkan>(file_path);
        Font& font = *owned;
        register_font(&font);
        return own_font(std::move(owned));
    }

    Font& ContextImplVulkan::create_font(const unsigned char* buffer, std::size_t size)
    {
        if (_owned_fonts.size() >= _resource_limits.max_fonts)
        {
            throw std::length_error("gCanvas font limit reached");
        }
        auto owned = std::make_unique<FontImplVulkan>(buffer, size);
        Font& font = *owned;
        register_font(&font);
        return own_font(std::move(owned));
    }

    void ContextImplVulkan::register_image(Image* image)
    {
        ContextImplVulkan* impl = getImpl(this);
        ImageImplVulkan* img = (ImageImplVulkan*)image;
        if (!img->_uploaded)
        {
            if (impl->images.size() >= static_cast<std::size_t>(impl->texture_array_size))
            {
                throw std::length_error("Vulkan texture sampler limit reached");
            }
            else
            {
                img->upload(impl->commandPool, impl->queue);
                img->_sampler_index = (int)impl->images.size();
                impl->images.push_back(img);
                impl->update_image_descriptor(*img);
            }
        }
    }

    void ContextImplVulkan::register_font(Font* font)
    {
        ContextImplVulkan* impl = getImpl(this);
        FontImplVulkan* fiv = (FontImplVulkan*)font;
        if (!fiv->_uploaded)
        {
            auto* img = static_cast<ImageImplVulkan*>(&font->get_image());
            if (impl->images.size() >= static_cast<std::size_t>(impl->texture_array_size))
            {
                throw std::length_error("Vulkan font sampler limit reached");
            }
            else
            {
                fiv->upload(impl->commandPool, impl->queue);
                img->_sampler_index = (int)impl->images.size();
                impl->images.push_back(img);
                impl->update_image_descriptor(*img);
            }
        }
    }

    void ContextImplVulkan::update_image(Image* image)
    {
        ContextImplVulkan* impl = getImpl(this);
        ImageImplVulkan* img = (ImageImplVulkan*)image;

        img->upload_update(impl->commandPool, impl->queue);
    }

    void ContextImplVulkan::update_image_descriptor(ImageImplVulkan& image)
    {
        VkDescriptorImageInfo image_info{};
        image_info.sampler = image._sampler;
        image_info.imageView = image._imageView;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        const std::size_t frame_count = external_target_ == nullptr ? frames_in_flight : 1U;
        for (std::size_t frame_index = 0; frame_index < frame_count; ++frame_index)
        {
            if (frameResources[frame_index].descriptor_set == VK_NULL_HANDLE)
                continue;
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = frameResources[frame_index].descriptor_set;
            write.dstBinding = 1;
            write.dstArrayElement = static_cast<std::uint32_t>(image._sampler_index);
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo = &image_info;
            vkUpdateDescriptorSets(VulkanSharedInfo::getInstance()->device, 1, &write, 0, nullptr);
        }
    }

    void ContextImplVulkan::prepare()
    {
        _default_font = &create_font(default_font.data(), default_font.size());
        if (_font == nullptr)
        {
            set_font(*_default_font);
        }

        ContextImplVulkan* impl = getImpl(this);

        impl->create_index_buffers();
        impl->create_storage_buffer();
        impl->create_descriptor_pool();
        impl->create_descriptor_set();
    }

    void ContextImplVulkan::draw_tessellated_path(const Path& path, const Paint& paint,
                                                   float line_width,
                                                   const Transform& transform)
    {
        ContextImplVulkan* impl = getImpl(this);
        const Transform device_transform =
            Transform::scaling(impl->_metrics.scale_x * impl->_metrics.dpi_scale,
                               impl->_metrics.scale_y * impl->_metrics.dpi_scale) *
            Transform::translation(impl->_metrics.offset_x, impl->_metrics.offset_y) * transform;
        if (impl->storage.size() + 1U >= uniform_rect_capacity)
            throw std::length_error("Vulkan path storage limit reached");
        const std::size_t available_quads =
            uniform_rect_capacity - impl->storage.size() - 1U;
        detail::PathGeometry geometry = detail::tessellate_path(
            path, paint, device_transform, line_width,
            std::min(_resource_limits.max_path_mesh_quads, available_quads),
            _resource_limits.path_paint_texture_size);
        if (geometry.mask_quads.empty())
            return;

        const std::size_t required = geometry.mask_quads.size() + 1U;
        if (required > std::numeric_limits<std::uint32_t>::max() ||
            impl->storage.size() > std::numeric_limits<std::uint32_t>::max() - required ||
            impl->storage.size() + required > uniform_rect_capacity)
            throw std::length_error("Vulkan path storage limit reached");

        int sampler_index = -1;
        if (geometry.paint_image != nullptr)
            sampler_index = static_cast<ImageImplVulkan&>(*geometry.paint_image)._sampler_index;
        else if (geometry.raster_paint.type() != Paint::Type::None)
        {
            Image& texture = acquire_cached_path_paint_texture(
                geometry.raster_paint,
                vec2(geometry.raster_minimum.x, geometry.raster_minimum.y),
                vec2(geometry.raster_maximum.x, geometry.raster_maximum.y));
            sampler_index = static_cast<ImageImplVulkan&>(texture)._sampler_index;
        }
        else if (!geometry.paint_pixels.empty())
        {
            Image& texture = acquire_path_paint_texture(geometry.paint_pixels);
            sampler_index = static_cast<ImageImplVulkan&>(texture)._sampler_index;
        }
        const auto first = static_cast<std::uint32_t>(impl->storage.size());
        for (const detail::MeshQuad& quad : geometry.mask_quads)
            impl->append_uniform(
                path_uniform(quad, _width, _height, color(255, 255, 255, 255)));
        impl->append_uniform(path_uniform(geometry.paint_quad, _width, _height,
                                          geometry.solid_color, sampler_index,
                                          geometry.paint_uv_min, geometry.paint_uv_max));
        impl->path_draws.push_back(
            {first, static_cast<std::uint32_t>(geometry.mask_quads.size()),
             geometry.mask_mode == detail::PathMaskMode::EvenOdd});
    }

    void ContextImplVulkan::draw_tessellated_path_blur(
        const Path& path, const Paint& paint, float line_width,
        const ShadowKernel& kernel, float source_alpha, const Transform& transform)
    {
        ContextImplVulkan* impl = getImpl(this);
        const Transform device_transform =
            Transform::scaling(impl->_metrics.scale_x * impl->_metrics.dpi_scale,
                               impl->_metrics.scale_y * impl->_metrics.dpi_scale) *
            Transform::translation(impl->_metrics.offset_x, impl->_metrics.offset_y) * transform;
        detail::PathGeometry geometry = detail::tessellate_path(
            path, paint, device_transform, line_width,
            _resource_limits.max_path_mesh_quads,
            _resource_limits.path_paint_texture_size);
        if (geometry.mask_quads.empty() || kernel.count == 0)
            return;

        const std::size_t logical_per_sample = geometry.mask_quads.size() + 1U;
        if (kernel.count > (std::numeric_limits<std::size_t>::max)() /
                               logical_per_sample)
            throw std::overflow_error("Vulkan blur path command size overflow");
        const std::size_t logical_required = kernel.count * logical_per_sample;
        if (logical_required > _resource_limits.max_blur_commands)
            throw std::length_error("Vulkan blur path command limit reached");
        if (geometry.mask_quads.size() >
            (std::numeric_limits<std::size_t>::max)() - kernel.count)
            throw std::overflow_error("Vulkan blur path storage size overflow");
        const std::size_t required = geometry.mask_quads.size() + kernel.count;
        if (required > uniform_rect_capacity ||
            impl->storage.size() > uniform_rect_capacity - required ||
            required > (std::numeric_limits<std::uint32_t>::max)() ||
            impl->storage.size() >
                (std::numeric_limits<std::uint32_t>::max)() - required)
            throw std::length_error("Vulkan blur path storage limit reached");
        const std::size_t index_capacity =
            static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)());
        reserve_geometric(impl->storage, impl->storage.size() + required,
                          uniform_rect_capacity);
        reserve_geometric(impl->path_draws, impl->path_draws.size() + 1U,
                          index_capacity);
        reserve_sample_offsets(*impl, kernel.count);

        int sampler_index = -1;
        if (geometry.paint_image != nullptr)
            sampler_index = static_cast<ImageImplVulkan&>(*geometry.paint_image)._sampler_index;
        else if (geometry.raster_paint.type() != Paint::Type::None)
        {
            Image& texture = acquire_cached_path_paint_texture(
                geometry.raster_paint,
                vec2(geometry.raster_minimum.x, geometry.raster_minimum.y),
                vec2(geometry.raster_maximum.x, geometry.raster_maximum.y));
            sampler_index = static_cast<ImageImplVulkan&>(texture)._sampler_index;
        }
        else if (!geometry.paint_pixels.empty())
        {
            Image& texture = acquire_path_paint_texture(geometry.paint_pixels);
            sampler_index = static_cast<ImageImplVulkan&>(texture)._sampler_index;
        }

        const bool solid = sampler_index < 0;
        const float metric_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        const float metric_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        const auto first = static_cast<std::uint32_t>(impl->storage.size());
        for (const detail::MeshQuad& quad : geometry.mask_quads)
            impl->append_uniform(
                path_uniform(quad, _width, _height, color(255, 255, 255, 255)));

        const auto offset_first =
            static_cast<std::uint32_t>(impl->path_sample_offsets.size());
        for (std::size_t sample_index = 0; sample_index < kernel.count; ++sample_index)
        {
            const ShadowSample& sample = kernel.samples[sample_index];
            impl->path_sample_offsets.emplace_back(sample.x * metric_x,
                                                   sample.y * metric_y);

            const float multiplier = source_alpha > 0.0f
                ? (std::min)(1.0f, sample.alpha / source_alpha)
                : 0.0f;
            const color paint_color = solid
                ? color(geometry.solid_color.rf(), geometry.solid_color.gf(),
                        geometry.solid_color.bf(), sample.alpha)
                : color(1.0f, 1.0f, 1.0f, multiplier);
            impl->append_uniform(path_uniform(
                geometry.paint_quad, _width, _height, paint_color, sampler_index,
                geometry.paint_uv_min, geometry.paint_uv_max));
        }
        impl->path_draws.push_back(
            {first, static_cast<std::uint32_t>(geometry.mask_quads.size()),
             geometry.mask_mode == detail::PathMaskMode::EvenOdd, 0, 0, 0, false, 0, 0,
             offset_first, static_cast<std::uint32_t>(kernel.count)});
    }

    void ContextImplVulkan::draw_tessellated_path_inset_shadow(
        const Path& path, float line_width, float offset_x, float offset_y,
        const ShadowKernel& kernel, float dilation_radius, const Transform& transform)
    {
        ContextImplVulkan* impl = getImpl(this);
        const Transform device_transform =
            Transform::scaling(impl->_metrics.scale_x * impl->_metrics.dpi_scale,
                               impl->_metrics.scale_y * impl->_metrics.dpi_scale) *
            Transform::translation(impl->_metrics.offset_x, impl->_metrics.offset_y) * transform;
        detail::PathGeometry geometry = detail::tessellate_path(
            path, Paint::solid(_fill_color), device_transform, line_width,
            _resource_limits.max_path_mesh_quads, _resource_limits.path_paint_texture_size);
        if (geometry.mask_quads.empty() || kernel.count == 0)
            return;

        detail::PathGeometry dilation_geometry;
        if (dilation_radius > 0.0f)
        {
            dilation_geometry = detail::tessellate_path(
                path, Paint::solid(_fill_color), device_transform,
                line_width > 0.0f ? line_width + dilation_radius * 2.0f
                                  : dilation_radius * 2.0f,
                _resource_limits.max_path_mesh_quads,
                _resource_limits.path_paint_texture_size);
        }

        const std::size_t mask_count = geometry.mask_quads.size();
        const bool replace_shifted_base = line_width > 0.0f && dilation_radius > 0.0f;
        const std::size_t shifted_base_count = replace_shifted_base
            ? dilation_geometry.mask_quads.size()
            : mask_count;
        const std::size_t shifted_extra_count =
            line_width == 0.0f ? dilation_geometry.mask_quads.size() : 0U;
        if (shifted_base_count > std::numeric_limits<std::size_t>::max() -
                                     shifted_extra_count)
            throw std::overflow_error("Vulkan inset path command size overflow");
        const std::size_t shifted_mask_count = shifted_base_count + shifted_extra_count;
        if (shifted_mask_count == std::numeric_limits<std::size_t>::max() ||
            kernel.count > (std::numeric_limits<std::size_t>::max() - mask_count) /
                               (shifted_mask_count + 1U))
            throw std::overflow_error("Vulkan inset path command size overflow");
        const std::size_t logical_required =
            mask_count + kernel.count * (shifted_mask_count + 1U);
        // Keep the pre-optimization logical command budget stable even though masks are
        // now uploaded once and translated by a dynamic viewport for every sample.
        if (logical_required > uniform_rect_capacity ||
            impl->storage.size() > uniform_rect_capacity - logical_required)
            throw std::length_error("Vulkan inset path storage limit reached");
        if (mask_count > std::numeric_limits<std::size_t>::max() - shifted_mask_count ||
            mask_count + shifted_mask_count >
                std::numeric_limits<std::size_t>::max() - kernel.count)
            throw std::overflow_error("Vulkan inset path storage size overflow");
        const std::size_t required = mask_count + shifted_mask_count + kernel.count;
        if (required > uniform_rect_capacity ||
            impl->storage.size() > uniform_rect_capacity - required ||
            required > std::numeric_limits<std::uint32_t>::max() ||
            impl->storage.size() > std::numeric_limits<std::uint32_t>::max() - required)
            throw std::length_error("Vulkan inset path storage limit reached");

        const std::size_t index_capacity =
            static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)());
        reserve_geometric(impl->storage, impl->storage.size() + required,
                          uniform_rect_capacity);
        reserve_geometric(impl->path_draws, impl->path_draws.size() + 1U,
                          index_capacity);
        reserve_sample_offsets(*impl, kernel.count);

        const auto first = static_cast<std::uint32_t>(impl->storage.size());
        for (const detail::MeshQuad& quad : geometry.mask_quads)
            impl->append_uniform(
                path_uniform(quad, _width, _height, color(255, 255, 255, 255)));

        // Frame-local layout is O(masks + samples): translated masks are shared and the
        // per-sample viewport supplies their device-space offset.
        const std::vector<detail::MeshQuad>& shifted_base = replace_shifted_base
            ? dilation_geometry.mask_quads
            : geometry.mask_quads;
        for (const detail::MeshQuad& quad : shifted_base)
            impl->append_uniform(
                path_uniform(quad, _width, _height, color(255, 255, 255, 255)));
        for (const detail::MeshQuad& quad : dilation_geometry.mask_quads)
        {
            if (shifted_extra_count > 0U)
                impl->append_uniform(
                    path_uniform(quad, _width, _height, color(255, 255, 255, 255)));
        }

        const color base = _fill_color;
        const float metric_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        const float metric_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        const float ndc_scale_x = 2.0f / static_cast<float>(_width);
        const float ndc_scale_y = 2.0f / static_cast<float>(_height);
        const auto offset_first =
            static_cast<std::uint32_t>(impl->path_sample_offsets.size());
        for (std::size_t sample_index = 0; sample_index < kernel.count; ++sample_index)
        {
            const ShadowSample& sample = kernel.samples[sample_index];
            const float shift_x = (offset_x + sample.x) * metric_x;
            const float shift_y = (offset_y + sample.y) * metric_y;
            impl->path_sample_offsets.emplace_back(shift_x, shift_y);
            uniform_rect paint_uniform = path_uniform(
                geometry.paint_quad, _width, _height,
                color(base.rf(), base.gf(), base.bf(), sample.alpha));
            // The sample viewport moves masks; compensate the paint quad so the inset paint
            // remains at the original path bounds without another viewport transition.
            shift_uniform_vertices(paint_uniform, -shift_x * ndc_scale_x,
                                   -shift_y * ndc_scale_y);
            impl->append_uniform(paint_uniform);
        }
        impl->path_draws.push_back(
            {first, static_cast<std::uint32_t>(mask_count),
             geometry.mask_mode == detail::PathMaskMode::EvenOdd,
             static_cast<std::uint32_t>(kernel.count),
             static_cast<std::uint32_t>(shifted_base_count),
             static_cast<std::uint32_t>(shifted_extra_count),
             !replace_shifted_base && geometry.mask_mode == detail::PathMaskMode::EvenOdd,
             0, 0, offset_first});
    }

    void ContextImplVulkan::draw_tessellated_path_eroded_shadow(
        const Path& path, float erosion_radius, const ShadowKernel& kernel,
        const Transform& transform)
    {
        ContextImplVulkan* impl = getImpl(this);
        const Transform device_transform =
            Transform::scaling(impl->_metrics.scale_x * impl->_metrics.dpi_scale,
                               impl->_metrics.scale_y * impl->_metrics.dpi_scale) *
            Transform::translation(impl->_metrics.offset_x, impl->_metrics.offset_y) * transform;
        detail::PathGeometry base = detail::tessellate_path(
            path, Paint::solid(_fill_color), device_transform, 0.0f,
            _resource_limits.max_path_mesh_quads, _resource_limits.path_paint_texture_size);
        detail::PathGeometry boundary = detail::tessellate_path(
            path, Paint::solid(_fill_color), device_transform, erosion_radius * 2.0f,
            _resource_limits.max_path_mesh_quads, _resource_limits.path_paint_texture_size);
        if (base.mask_quads.empty() || boundary.mask_quads.empty() || kernel.count == 0)
            return;

        if (base.mask_quads.size() >
            std::numeric_limits<std::size_t>::max() - boundary.mask_quads.size())
            throw std::overflow_error("Vulkan eroded path command size overflow");
        const std::size_t masks_per_sample =
            base.mask_quads.size() + boundary.mask_quads.size();
        if (masks_per_sample == std::numeric_limits<std::size_t>::max() ||
            kernel.count > std::numeric_limits<std::size_t>::max() /
                               (masks_per_sample + 1U))
            throw std::overflow_error("Vulkan eroded path command size overflow");
        const std::size_t logical_required = kernel.count * (masks_per_sample + 1U);
        // Preserve the former per-frame command ceiling independently of the compact
        // physical upload layout used by the immediate-mode recorder.
        if (logical_required > uniform_rect_capacity ||
            impl->storage.size() > uniform_rect_capacity - logical_required)
            throw std::length_error("Vulkan eroded path storage limit reached");
        if (masks_per_sample >
            std::numeric_limits<std::size_t>::max() - kernel.count)
            throw std::overflow_error("Vulkan eroded path storage size overflow");
        const std::size_t required = masks_per_sample + kernel.count;
        if (required > uniform_rect_capacity ||
            impl->storage.size() > uniform_rect_capacity - required ||
            required > std::numeric_limits<std::uint32_t>::max() ||
            impl->storage.size() > std::numeric_limits<std::uint32_t>::max() - required)
            throw std::length_error("Vulkan eroded path storage limit reached");

        const std::size_t index_capacity =
            static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)());
        reserve_geometric(impl->storage, impl->storage.size() + required,
                          uniform_rect_capacity);
        reserve_geometric(impl->path_draws, impl->path_draws.size() + 1U,
                          index_capacity);
        reserve_sample_offsets(*impl, kernel.count);

        const auto first = static_cast<std::uint32_t>(impl->storage.size());
        // Both erosion masks share one translation per sample, so upload each mesh once.
        for (const detail::MeshQuad& quad : base.mask_quads)
            impl->append_uniform(
                path_uniform(quad, _width, _height, color(255, 255, 255, 255)));
        for (const detail::MeshQuad& quad : boundary.mask_quads)
            impl->append_uniform(
                path_uniform(quad, _width, _height, color(255, 255, 255, 255)));

        const color base_color = _fill_color;
        const float metric_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        const float metric_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        const auto offset_first =
            static_cast<std::uint32_t>(impl->path_sample_offsets.size());
        for (std::size_t sample_index = 0; sample_index < kernel.count; ++sample_index)
        {
            const ShadowSample& sample = kernel.samples[sample_index];
            const float shift_x = sample.x * metric_x;
            const float shift_y = sample.y * metric_y;
            impl->path_sample_offsets.emplace_back(shift_x, shift_y);
            impl->append_uniform(path_uniform(
                base.paint_quad, _width, _height,
                color(base_color.rf(), base_color.gf(), base_color.bf(), sample.alpha)));
        }
        impl->path_draws.push_back(
            {first, static_cast<std::uint32_t>(base.mask_quads.size()),
             base.mask_mode == detail::PathMaskMode::EvenOdd, 0, 0, 0, false,
             static_cast<std::uint32_t>(boundary.mask_quads.size()),
             static_cast<std::uint32_t>(kernel.count), offset_first});
    }

    /* ------------------------ PRIVATE IMPLEMENTATION ------------------------ */

    void ContextImplVulkan::initialize_limits()
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(VulkanSharedInfo::getInstance()->bestPhysicalDevice,
                                      &properties);

        if (uniform_rect_buffer_size > properties.limits.maxStorageBufferRange)
        {
            throw std::runtime_error(
                "Vulkan device storage-buffer range is too small for gCanvas draw records");
        }
        if (_resource_limits.max_fonts >
            std::numeric_limits<std::size_t>::max() - _resource_limits.max_images)
            throw std::overflow_error("gCanvas Vulkan texture capacity overflow");

        const std::size_t requested_textures =
            std::max<std::size_t>(1, _resource_limits.max_images + _resource_limits.max_fonts);
        const std::uint32_t device_limit =
            std::min({properties.limits.maxPerStageDescriptorSamplers,
                      properties.limits.maxDescriptorSetSamplers,
                      properties.limits.maxPerStageDescriptorSampledImages,
                      properties.limits.maxDescriptorSetSampledImages});
        if (device_limit == 0)
            throw std::runtime_error("Vulkan device exposes no sampled-image descriptors");
        texture_array_size = static_cast<std::uint32_t>(
            std::min<std::size_t>(requested_textures, device_limit));
    }

    void ContextImplVulkan::create_surface()
    {
        // --------------- Create WIndow Surface ---------------

        std::uint64_t surface_handle = 0;
        const int result = _host.create_surface(
            _host.user_data,
            reinterpret_cast<std::uintptr_t>(VulkanSharedInfo::getInstance()->instance),
            &surface_handle);
        vku::err_check(static_cast<VkResult>(result));
        static_assert(sizeof(surface) <= sizeof(surface_handle));
        std::memcpy(&surface, &surface_handle, sizeof(surface));
    }

    void ContextImplVulkan::create_queue()
    {
        // --------------- Get Device Queue ---------------

        vkGetDeviceQueue(VulkanSharedInfo::getInstance()->device,
                         VulkanSharedInfo::getInstance()->queueFamilyIndex, 0, &queue);
    }

    void ContextImplVulkan::check_surface_support()
    {
        // --------------- Check Surface Support ---------------

        VkBool32 surfaceSupport = false;
        vku::err_check(vkGetPhysicalDeviceSurfaceSupportKHR(
            VulkanSharedInfo::getInstance()->bestPhysicalDevice,
            VulkanSharedInfo::getInstance()->queueFamilyIndex, surface, &surfaceSupport));
        if (!surfaceSupport)
        {
            throw std::runtime_error("Vulkan device does not support presentation to this surface");
        }
    }

    void ContextImplVulkan::create_swapchain()
    {
        // --------------- Get Surface Capabilities ---------------

        VkSurfaceCapabilitiesKHR surfaceCapabilities{};
        vku::err_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            VulkanSharedInfo::getInstance()->bestPhysicalDevice, surface, &surfaceCapabilities));

        // --------------- Get Surface Formats ---------------

        uint32_t surfaceFormatCount = 0;
        vku::err_check(vkGetPhysicalDeviceSurfaceFormatsKHR(
            VulkanSharedInfo::getInstance()->bestPhysicalDevice, surface, &surfaceFormatCount,
            nullptr));
        if (surfaceFormatCount == 0)
        {
            throw std::runtime_error("Vulkan surface exposes no formats");
        }
        std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
        vku::err_check(vkGetPhysicalDeviceSurfaceFormatsKHR(
            VulkanSharedInfo::getInstance()->bestPhysicalDevice, surface, &surfaceFormatCount,
            surfaceFormats.data()));

        // --------------- Select Image Count ---------------

        uint32_t selectedImageCount = surfaceCapabilities.minImageCount + 1;
        if (surfaceCapabilities.maxImageCount > 0 &&
            selectedImageCount > surfaceCapabilities.maxImageCount)
            selectedImageCount = surfaceCapabilities.maxImageCount;

        // --------------- Select Image Format and Colorspace ---------------

        selectedImageFormat = surfaceFormats[0].format == VK_FORMAT_UNDEFINED
                                  ? VK_FORMAT_B8G8R8A8_UNORM
                                  : surfaceFormats[0].format;
        VkColorSpaceKHR selectedImageColorSpace = surfaceFormats[0].colorSpace;
        for (uint32_t i = 0; i < surfaceFormatCount; ++i)
        {
            if (surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
                surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                selectedImageFormat = surfaceFormats[i].format;
                selectedImageColorSpace = surfaceFormats[i].colorSpace;
            }
        }

        // --------------- Select Extents ---------------

        VkExtent2D selectedImageExtent = surfaceCapabilities.currentExtent;
        if (selectedImageExtent.width == std::numeric_limits<std::uint32_t>::max())
        {
            selectedImageExtent = {static_cast<std::uint32_t>(_width),
                                   static_cast<std::uint32_t>(_height)};
            selectedImageExtent.width =
                std::clamp(selectedImageExtent.width, surfaceCapabilities.minImageExtent.width,
                           surfaceCapabilities.maxImageExtent.width);
            selectedImageExtent.height =
                std::clamp(selectedImageExtent.height, surfaceCapabilities.minImageExtent.height,
                           surfaceCapabilities.maxImageExtent.height);
        }
        swapchainExtent = selectedImageExtent;
        _width = static_cast<int>(swapchainExtent.width);
        _height = static_cast<int>(swapchainExtent.height);

        // --------------- Get Present Modes ---------------

        uint32_t presentModeCount = 0;
        vku::err_check(vkGetPhysicalDeviceSurfacePresentModesKHR(
            VulkanSharedInfo::getInstance()->bestPhysicalDevice, surface, &presentModeCount,
            nullptr));
        if (presentModeCount == 0)
        {
            throw std::runtime_error("Vulkan surface exposes no present modes");
        }
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vku::err_check(vkGetPhysicalDeviceSurfacePresentModesKHR(
            VulkanSharedInfo::getInstance()->bestPhysicalDevice, surface, &presentModeCount,
            presentModes.data()));

        // --------------- Select Present Mode ---------------

        VkPresentModeKHR selectedPresentMode =
            vku::select_present_mode(presentModes.data(), presentModeCount, _vsync);

        constexpr VkImageUsageFlags required_image_usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if ((surfaceCapabilities.supportedUsageFlags & required_image_usage) !=
            required_image_usage)
        {
            throw std::runtime_error(
                "Vulkan surface does not support color rendering with pixel readback");
        }
        constexpr std::array<VkCompositeAlphaFlagBitsKHR, 4> composite_alpha_candidates = {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
        const auto composite_alpha = std::find_if(
            composite_alpha_candidates.begin(), composite_alpha_candidates.end(),
            [&](VkCompositeAlphaFlagBitsKHR candidate) {
                return (surfaceCapabilities.supportedCompositeAlpha & candidate) != 0;
            });
        if (composite_alpha == composite_alpha_candidates.end())
            throw std::runtime_error("Vulkan surface exposes no supported composite-alpha mode");

        // --------------- Create Swapchain Create Info ---------------

        VkSwapchainCreateInfoKHR swapchainCreateInfo{};
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.pNext = nullptr;
        swapchainCreateInfo.flags = 0;
        swapchainCreateInfo.surface = surface;
        swapchainCreateInfo.minImageCount = selectedImageCount;
        swapchainCreateInfo.imageFormat = selectedImageFormat;
        swapchainCreateInfo.imageColorSpace = selectedImageColorSpace;
        swapchainCreateInfo.imageExtent = selectedImageExtent;
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.imageUsage = required_image_usage;
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.queueFamilyIndexCount = 0;
        swapchainCreateInfo.pQueueFamilyIndices = nullptr;
        swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
        swapchainCreateInfo.compositeAlpha = *composite_alpha;
        swapchainCreateInfo.presentMode = selectedPresentMode;
        swapchainCreateInfo.clipped = VK_TRUE;
        swapchainCreateInfo.oldSwapchain = swapchain;

        // --------------- Create Swapchain ---------------

        VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
        vku::err_check(vkCreateSwapchainKHR(VulkanSharedInfo::getInstance()->device,
                                            &swapchainCreateInfo, nullptr, &new_swapchain));
        swapchain = new_swapchain;

    }

    void ContextImplVulkan::create_image_views()
    {
        // --------------- Get Swapchain Image ---------------
        if (external_target_ != nullptr)
        {
            swapchainImageCount = 1;
            swapchainImages.assign(1, external_target_->image);
        }
        else
        {
            vku::err_check(vkGetSwapchainImagesKHR(
                VulkanSharedInfo::getInstance()->device, swapchain,
                &swapchainImageCount, nullptr));
            swapchainImages.assign(swapchainImageCount, VK_NULL_HANDLE);
            vku::err_check(vkGetSwapchainImagesKHR(
                VulkanSharedInfo::getInstance()->device, swapchain,
                &swapchainImageCount, swapchainImages.data()));
            swapchainImages.resize(swapchainImageCount);
        }

        // --------------- Create Image Views ---------------

        imageViews.assign(swapchainImageCount, VK_NULL_HANDLE);
        for (uint32_t i = 0; i < swapchainImageCount; ++i)
        {
            VkImageViewCreateInfo imageViewCreateInfo{};
            imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewCreateInfo.pNext = nullptr;
            imageViewCreateInfo.flags = 0;
            imageViewCreateInfo.image = swapchainImages[i];
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            imageViewCreateInfo.format = selectedImageFormat;
            imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
            imageViewCreateInfo.subresourceRange.levelCount = 1;
            imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
            imageViewCreateInfo.subresourceRange.layerCount = 1;

            vku::err_check(vkCreateImageView(VulkanSharedInfo::getInstance()->device,
                                             &imageViewCreateInfo, nullptr, &imageViews[i]));
        }

    }

    void ContextImplVulkan::create_stencil_resources()
    {
        const std::array<VkFormat, 2> candidates = {VK_FORMAT_D24_UNORM_S8_UINT,
                                                     VK_FORMAT_D32_SFLOAT_S8_UINT};
        for (VkFormat candidate : candidates)
        {
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(
                VulkanSharedInfo::getInstance()->bestPhysicalDevice, candidate, &properties);
            if ((properties.optimalTilingFeatures &
                 VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
            {
                stencilFormat = candidate;
                break;
            }
        }
        if (stencilFormat == VK_FORMAT_UNDEFINED)
            throw std::runtime_error("Vulkan device has no supported stencil attachment format");

        const VkDevice device = VulkanSharedInfo::getInstance()->device;
        const VmaAllocator allocator = VulkanSharedInfo::getInstance()->allocator;
        stencilImages.assign(swapchainImageCount, VK_NULL_HANDLE);
        stencilAllocations.assign(swapchainImageCount, nullptr);
        stencilImageViews.assign(swapchainImageCount, VK_NULL_HANDLE);
        for (std::uint32_t index = 0; index < swapchainImageCount; ++index)
        {
            VkImageCreateInfo image_info{};
            image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            image_info.imageType = VK_IMAGE_TYPE_2D;
            image_info.format = stencilFormat;
            image_info.extent = {swapchainExtent.width, swapchainExtent.height, 1};
            image_info.mipLevels = 1;
            image_info.arrayLayers = 1;
            image_info.samples = VK_SAMPLE_COUNT_1_BIT;
            image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VmaAllocationCreateInfo allocation_info{};
            allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            allocation_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            vku::err_check(vmaCreateImage(allocator, &image_info, &allocation_info,
                                          &stencilImages[index], &stencilAllocations[index],
                                          nullptr));

            VkImageViewCreateInfo view_info{};
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = stencilImages[index];
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = stencilFormat;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.layerCount = 1;
            vku::err_check(vkCreateImageView(device, &view_info, nullptr,
                                             &stencilImageViews[index]));
        }
    }

    void ContextImplVulkan::destroy_stencil_resources() noexcept
    {
        const VkDevice device = VulkanSharedInfo::getInstance()->device;
        const VmaAllocator allocator = VulkanSharedInfo::getInstance()->allocator;
        for (VkImageView view : stencilImageViews)
            if (view != VK_NULL_HANDLE)
                vkDestroyImageView(device, view, nullptr);
        for (std::size_t index = 0; index < stencilImages.size(); ++index)
            if (stencilImages[index] != VK_NULL_HANDLE)
                vmaDestroyImage(allocator, stencilImages[index], stencilAllocations[index]);
        stencilImageViews.clear();
        stencilImages.clear();
        stencilAllocations.clear();
        stencilFormat = VK_FORMAT_UNDEFINED;
    }

    void ContextImplVulkan::create_render_pass()
    {
        // --------------- Create Attachment Description ---------------

        VkAttachmentDescription attachmentDescription{};
        attachmentDescription.flags = 0;
        attachmentDescription.format = selectedImageFormat;
        attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescription.finalLayout = external_target_ != nullptr
                                                ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription stencilAttachment{};
        stencilAttachment.format = stencilFormat;
        stencilAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        stencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        stencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        stencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        stencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        stencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        stencilAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // --------------- Create Attachment Reference ---------------

        VkAttachmentReference attachmentReference{};
        attachmentReference.attachment = 0;
        attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference stencilReference{};
        stencilReference.attachment = 1;
        stencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // --------------- Create Subpass Desciption ---------------

        VkSubpassDescription subpassDescription{};
        subpassDescription.flags = 0;
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.inputAttachmentCount = 0;
        subpassDescription.pInputAttachments = nullptr;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &attachmentReference;
        subpassDescription.pResolveAttachments = nullptr;
        subpassDescription.pDepthStencilAttachment = &stencilReference;
        subpassDescription.preserveAttachmentCount = 0;
        subpassDescription.pPreserveAttachments = nullptr;

        // --------------- Create Subpass Dependency ---------------

        VkSubpassDependency subpassDependency{};
        subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDependency.dstSubpass = 0;
        subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        subpassDependency.srcAccessMask = 0;
        subpassDependency.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        subpassDependency.dependencyFlags = 0;

        // --------------- Create Render Pass Create Info ---------------

        VkRenderPassCreateInfo renderPassCreateInfo{};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.pNext = nullptr;
        renderPassCreateInfo.flags = 0;
        const std::array<VkAttachmentDescription, 2> attachments = {
            attachmentDescription, stencilAttachment};
        renderPassCreateInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        renderPassCreateInfo.pAttachments = attachments.data();
        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpassDescription;
        renderPassCreateInfo.dependencyCount = 1;
        renderPassCreateInfo.pDependencies = &subpassDependency;

        // --------------- Create Render Pass ---------------

        vku::err_check(vkCreateRenderPass(VulkanSharedInfo::getInstance()->device,
                                          &renderPassCreateInfo, nullptr, &renderPass));
    }

    void ContextImplVulkan::create_descriptor_set_layout()
    {
        /// VkDescriptorSetLayoutBinding uniformDescriptorSetLayoutBinding;
        /// uniformDescriptorSetLayoutBinding.binding = 0;
        /// uniformDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        /// uniformDescriptorSetLayoutBinding.descriptorCount = 1;
        /// uniformDescriptorSetLayoutBinding.stageFlags =
        /// VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        /// uniformDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding storageDescriptorSetLayoutBinding{};
        storageDescriptorSetLayoutBinding.binding = 0;
        storageDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        storageDescriptorSetLayoutBinding.descriptorCount = 1;
        storageDescriptorSetLayoutBinding.stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        storageDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding samplerDescriptorSetLayoutBinding{};
        samplerDescriptorSetLayoutBinding.binding = 1;
        samplerDescriptorSetLayoutBinding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerDescriptorSetLayoutBinding.descriptorCount = texture_array_size;
        samplerDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings = {
            /// uniformDescriptorSetLayoutBinding,
            storageDescriptorSetLayoutBinding, samplerDescriptorSetLayoutBinding};

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.pNext = nullptr;
        descriptorSetLayoutCreateInfo.flags = 0;
        descriptorSetLayoutCreateInfo.bindingCount = (uint32_t)descriptorSetLayoutBindings.size();
        descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();

        vku::err_check(vkCreateDescriptorSetLayout(VulkanSharedInfo::getInstance()->device,
                                                   &descriptorSetLayoutCreateInfo, nullptr,
                                                   &descriptorSetLayout));
    }

    void ContextImplVulkan::create_pipeline()
    {
        // --------------- Load Shader ---------------

        //        vku::create_shader_module("./gcanvas_res/shader/rounded_rect.frag.spv",
        //        fragShaderModule);
        //        vku::create_shader_module("./gcanvas_res/shader/rounded_rect.vert.spv",
        //        vertShaderModule);
        vku::create_shader_module(vk_fragment_code, &fragShaderModule);
        vku::create_shader_module(vk_vertex_code, &vertShaderModule);

        // --------------- Create Pipeline Shader Stage Create Info ---------------

        VkPipelineShaderStageCreateInfo shaderStageCreateInfoVert{};
        shaderStageCreateInfoVert.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfoVert.pNext = nullptr;
        shaderStageCreateInfoVert.flags = 0;
        shaderStageCreateInfoVert.stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStageCreateInfoVert.module = vertShaderModule;
        shaderStageCreateInfoVert.pName = "main";
        shaderStageCreateInfoVert.pSpecializationInfo = nullptr;

        // --------------- Create Pipeline Shader Stage Create Info ---------------

        VkPipelineShaderStageCreateInfo shaderStageCreateInfoFrag{};
        shaderStageCreateInfoFrag.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfoFrag.pNext = nullptr;
        shaderStageCreateInfoFrag.flags = 0;
        shaderStageCreateInfoFrag.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStageCreateInfoFrag.module = fragShaderModule;
        shaderStageCreateInfoFrag.pName = "main";
        shaderStageCreateInfoFrag.pSpecializationInfo = nullptr;

        VkPipelineShaderStageCreateInfo shaderStages[] = {shaderStageCreateInfoVert,
                                                          shaderStageCreateInfoFrag};

        /*
        VkVertexInputBindingDescription vertexInputBindingDescription =
        vertex::getBindingDescription(); std::array<VkVertexInputAttributeDescription, 1>
        vertexInputAttributeDescription = vertex::gerAttributeDescriptions();
            */

        // --------------- Create Pipeline Vertex Input State Create Info ---------------

        VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
        pipelineVertexInputStateCreateInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        pipelineVertexInputStateCreateInfo.pNext = nullptr;
        pipelineVertexInputStateCreateInfo.flags = 0;
        /*
        pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
        pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions =
            &vertexInputBindingDescription;
        pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount =
            (uint32_t)vertexInputAttributeDescription.size();
        pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions =
            vertexInputAttributeDescription.data();
        */
        pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
        pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
        pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
        pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

        // --------------- Create Pipeline Input Assembly State Create Info ---------------

        VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
        pipelineInputAssemblyStateCreateInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        pipelineInputAssemblyStateCreateInfo.pNext = nullptr;
        pipelineInputAssemblyStateCreateInfo.flags = 0;
        pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

        // --------------- Create Pipeline Viewport State Create Info ---------------

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)_width;
        viewport.height = (float)_height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {(uint32_t)_width, (uint32_t)_height};

        VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{};
        pipelineViewportStateCreateInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        pipelineViewportStateCreateInfo.pNext = nullptr;
        pipelineViewportStateCreateInfo.flags = 0;
        pipelineViewportStateCreateInfo.viewportCount = 1;
        pipelineViewportStateCreateInfo.pViewports = &viewport;
        pipelineViewportStateCreateInfo.scissorCount = 1;
        pipelineViewportStateCreateInfo.pScissors = &scissor;

        // --------------- Create Pipeline Rasterization State Create Info ---------------

        VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{};
        pipelineRasterizationStateCreateInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        pipelineRasterizationStateCreateInfo.pNext = nullptr;
        pipelineRasterizationStateCreateInfo.flags = 0;
        pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
        pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
        pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
        pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
        pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
        pipelineRasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
        pipelineRasterizationStateCreateInfo.depthBiasClamp = 0.0f;
        pipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
        pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

        // --------------- Create Pipeline Multisample State Create Info ---------------

        VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
        pipelineMultisampleStateCreateInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        pipelineMultisampleStateCreateInfo.pNext = nullptr;
        pipelineMultisampleStateCreateInfo.flags = 0;
        pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        pipelineMultisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
        pipelineMultisampleStateCreateInfo.minSampleShading = 0.0f;
        pipelineMultisampleStateCreateInfo.pSampleMask = nullptr;
        pipelineMultisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
        pipelineMultisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

        // --------------- Create Pipeline Color Blend Attachment State ---------------

        VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
        pipelineColorBlendAttachmentState.blendEnable = VK_TRUE;
        pipelineColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        pipelineColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        pipelineColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
        pipelineColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pipelineColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pipelineColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
        pipelineColorBlendAttachmentState.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;

        // --------------- Create Pipeline Color Blend State Create Info ---------------

        VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{};
        pipelineColorBlendStateCreateInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        pipelineColorBlendStateCreateInfo.pNext = nullptr;
        pipelineColorBlendStateCreateInfo.flags = 0;
        pipelineColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
        pipelineColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_NO_OP;
        pipelineColorBlendStateCreateInfo.attachmentCount = 1;
        pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;
        pipelineColorBlendStateCreateInfo.blendConstants[0] = 0.0f;
        pipelineColorBlendStateCreateInfo.blendConstants[1] = 0.0f;
        pipelineColorBlendStateCreateInfo.blendConstants[2] = 0.0f;
        pipelineColorBlendStateCreateInfo.blendConstants[3] = 0.0f;

        VkPipelineDepthStencilStateCreateInfo depth_stencil{};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = VK_FALSE;
        depth_stencil.depthWriteEnable = VK_FALSE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        depth_stencil.stencilTestEnable = VK_FALSE;

        VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
        pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        pipelineDynamicStateCreateInfo.pNext = nullptr;
        pipelineDynamicStateCreateInfo.flags = 0;
        pipelineDynamicStateCreateInfo.dynamicStateCount = 2;
        pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStates;

        // --------------- Create Pipeline Layout Create Info ---------------

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.pNext = nullptr;
        pipelineLayoutCreateInfo.flags = 0;
        pipelineLayoutCreateInfo.setLayoutCount = 1;
        pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
        pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

        // --------------- Create Pipeline Layout ---------------

        vku::err_check(vkCreatePipelineLayout(VulkanSharedInfo::getInstance()->device,
                                              &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

        // --------------- Create Graphics Pipeline Create Info ---------------

        VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
        graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphicsPipelineCreateInfo.pNext = nullptr;
        graphicsPipelineCreateInfo.flags = 0;
        graphicsPipelineCreateInfo.stageCount = 2;
        graphicsPipelineCreateInfo.pStages = shaderStages;
        graphicsPipelineCreateInfo.pVertexInputState = &pipelineVertexInputStateCreateInfo;
        graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
        graphicsPipelineCreateInfo.pTessellationState = nullptr;
        graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
        graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
        graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
        graphicsPipelineCreateInfo.pDepthStencilState = &depth_stencil;
        graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
        graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
        graphicsPipelineCreateInfo.layout = pipelineLayout;
        graphicsPipelineCreateInfo.renderPass = renderPass;
        graphicsPipelineCreateInfo.subpass = 0;
        graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
        graphicsPipelineCreateInfo.basePipelineIndex = -1;

        // --------------- Create Graphics Pipeline ---------------

        vku::err_check(vkCreateGraphicsPipelines(VulkanSharedInfo::getInstance()->device,
                                                 VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo,
                                                 nullptr, &pipeline));

        const auto create_variant = [&](VkPipeline& result) {
            vku::err_check(vkCreateGraphicsPipelines(
                VulkanSharedInfo::getInstance()->device, VK_NULL_HANDLE, 1,
                &graphicsPipelineCreateInfo, nullptr, &result));
        };

        depth_stencil.stencilTestEnable = VK_TRUE;
        depth_stencil.front.compareOp = VK_COMPARE_OP_EQUAL;
        depth_stencil.front.failOp = VK_STENCIL_OP_KEEP;
        depth_stencil.front.depthFailOp = VK_STENCIL_OP_KEEP;
        depth_stencil.front.passOp = VK_STENCIL_OP_KEEP;
        depth_stencil.front.compareMask = 0x02;
        depth_stencil.front.writeMask = 0;
        depth_stencil.front.reference = 0x02;
        depth_stencil.back = depth_stencil.front;
        create_variant(clippedPipeline);

        pipelineColorBlendAttachmentState.colorWriteMask = 0;
        depth_stencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
        depth_stencil.front.passOp = VK_STENCIL_OP_REPLACE;
        depth_stencil.front.compareMask = 0x02;
        depth_stencil.front.writeMask = 0x02;
        depth_stencil.front.reference = 0x02;
        depth_stencil.back = depth_stencil.front;
        create_variant(convexMaskPipeline);

        depth_stencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
        depth_stencil.front.passOp = VK_STENCIL_OP_INVERT;
        depth_stencil.front.compareMask = 0x01;
        depth_stencil.front.writeMask = 0x01;
        depth_stencil.front.reference = 1;
        depth_stencil.back = depth_stencil.front;
        create_variant(evenOddMaskPipeline);

        depth_stencil.front.passOp = VK_STENCIL_OP_REPLACE;
        depth_stencil.back = depth_stencil.front;
        create_variant(unionMaskPipeline);

        depth_stencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
        depth_stencil.front.passOp = VK_STENCIL_OP_INVERT;
        depth_stencil.front.compareMask = 0x04;
        depth_stencil.front.writeMask = 0x04;
        depth_stencil.front.reference = 0x04;
        depth_stencil.back = depth_stencil.front;
        create_variant(shiftedEvenOddMaskPipeline);

        depth_stencil.front.passOp = VK_STENCIL_OP_REPLACE;
        depth_stencil.back = depth_stencil.front;
        create_variant(shiftedUnionMaskPipeline);

        depth_stencil.front.compareOp = VK_COMPARE_OP_EQUAL;
        depth_stencil.front.passOp = VK_STENCIL_OP_INVERT;
        depth_stencil.front.compareMask = 0x02;
        depth_stencil.front.writeMask = 0x01;
        depth_stencil.front.reference = 0x03;
        depth_stencil.back = depth_stencil.front;
        create_variant(evenOddClippedMaskPipeline);

        depth_stencil.front.passOp = VK_STENCIL_OP_REPLACE;
        depth_stencil.back = depth_stencil.front;
        create_variant(unionClippedMaskPipeline);

        depth_stencil.front.compareOp = VK_COMPARE_OP_EQUAL;
        depth_stencil.front.passOp = VK_STENCIL_OP_INVERT;
        depth_stencil.front.compareMask = 0x02;
        depth_stencil.front.writeMask = 0x04;
        depth_stencil.front.reference = 0x06;
        depth_stencil.back = depth_stencil.front;
        create_variant(shiftedEvenOddClippedMaskPipeline);

        depth_stencil.front.passOp = VK_STENCIL_OP_REPLACE;
        depth_stencil.back = depth_stencil.front;
        create_variant(shiftedUnionClippedMaskPipeline);

        pipelineColorBlendAttachmentState.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        depth_stencil.front.compareOp = VK_COMPARE_OP_EQUAL;
        // Unclipped path paint covers the tessellated path bounds and the fragment shader never
        // discards, so every fragment that passes the path-bit test can clear that bit in-place.
        // Clipped and multi-sample inset variants restore KEEP below because they still need the
        // original path mask after this draw.
        depth_stencil.front.passOp = VK_STENCIL_OP_ZERO;
        depth_stencil.front.compareMask = 0x01;
        depth_stencil.front.writeMask = 0x01;
        depth_stencil.front.reference = 0x01;
        depth_stencil.back = depth_stencil.front;
        create_variant(pathPaintPipeline);

        depth_stencil.front.passOp = VK_STENCIL_OP_KEEP;
        depth_stencil.front.writeMask = 0;
        depth_stencil.front.compareMask = 0x03;
        depth_stencil.front.reference = 0x03;
        depth_stencil.back = depth_stencil.front;
        create_variant(clippedPathPaintPipeline);

        depth_stencil.front.compareMask = 0x05;
        depth_stencil.front.reference = 0x01;
        depth_stencil.back = depth_stencil.front;
        create_variant(insetPathPaintPipeline);

        depth_stencil.front.compareMask = 0x07;
        depth_stencil.front.reference = 0x03;
        depth_stencil.back = depth_stencil.front;
        create_variant(clippedInsetPathPaintPipeline);

        pipelineColorBlendAttachmentState.colorWriteMask = 0;
        depth_stencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
        depth_stencil.front.passOp = VK_STENCIL_OP_REPLACE;
        depth_stencil.front.compareMask = 0x01;
        depth_stencil.front.writeMask = 0x01;
        depth_stencil.front.reference = 0;
        depth_stencil.back = depth_stencil.front;
        create_variant(pathClearPipeline);

        depth_stencil.front.compareMask = 0x04;
        depth_stencil.front.writeMask = 0x04;
        depth_stencil.back = depth_stencil.front;
        create_variant(shiftedPathClearPipeline);
    }

    void ContextImplVulkan::destroy_pipeline_resources() noexcept
    {
        const VkDevice device = VulkanSharedInfo::getInstance()->device;
        for (VkPipeline* pipeline_handle :
             {&pipeline, &clippedPipeline, &convexMaskPipeline, &evenOddMaskPipeline,
              &unionMaskPipeline, &pathPaintPipeline, &evenOddClippedMaskPipeline,
              &unionClippedMaskPipeline, &clippedPathPaintPipeline,
              &pathClearPipeline, &shiftedEvenOddMaskPipeline,
              &shiftedUnionMaskPipeline, &shiftedEvenOddClippedMaskPipeline,
              &shiftedUnionClippedMaskPipeline, &insetPathPaintPipeline,
              &clippedInsetPathPaintPipeline, &shiftedPathClearPipeline})
        {
            if (*pipeline_handle != VK_NULL_HANDLE)
            {
                vkDestroyPipeline(device, *pipeline_handle, nullptr);
                *pipeline_handle = VK_NULL_HANDLE;
            }
        }
        if (pipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
        if (vertShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device, vertShaderModule, nullptr);
            vertShaderModule = VK_NULL_HANDLE;
        }
        if (fragShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device, fragShaderModule, nullptr);
            fragShaderModule = VK_NULL_HANDLE;
        }
    }

    void ContextImplVulkan::create_framebuffer()
    {
        // --------------- Create Framebuffers ---------------

        frameBuffers.assign(swapchainImageCount, VK_NULL_HANDLE);

        for (uint32_t i = 0; i < swapchainImageCount; ++i)
        {
            const std::array<VkImageView, 2> attachments = {imageViews[i],
                                                            stencilImageViews[i]};
            VkFramebufferCreateInfo frameBufferCreateInfo{};
            frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            frameBufferCreateInfo.pNext = nullptr;
            frameBufferCreateInfo.flags = 0;
            frameBufferCreateInfo.renderPass = renderPass;
            frameBufferCreateInfo.attachmentCount =
                static_cast<std::uint32_t>(attachments.size());
            frameBufferCreateInfo.pAttachments = attachments.data();
            frameBufferCreateInfo.width = (uint32_t)_width;
            frameBufferCreateInfo.height = (uint32_t)_height;
            frameBufferCreateInfo.layers = 1;

            vku::err_check(vkCreateFramebuffer(VulkanSharedInfo::getInstance()->device,
                                               &frameBufferCreateInfo, nullptr,
                                               &(frameBuffers[i])));
        }
    }

    void ContextImplVulkan::create_command_pool()
    {
        // --------------- Create Command Pool Create Info ---------------

        VkCommandPoolCreateInfo commandPoolCreateInfo{};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.pNext = nullptr;
        commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolCreateInfo.queueFamilyIndex = VulkanSharedInfo::getInstance()->queueFamilyIndex;

        // --------------- Create Command Pool ---------------

        vku::err_check(vkCreateCommandPool(VulkanSharedInfo::getInstance()->device,
                                           &commandPoolCreateInfo, nullptr, &commandPool));
    }

    void ContextImplVulkan::create_command_buffers()
    {
        // --------------- Create Command Buffer Allocate Info ---------------

        VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.pNext = nullptr;
        commandBufferAllocateInfo.commandPool = commandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount =
            swapchainImageCount;

        // --------------- Allocate Command Buffers ---------------

        commandBuffers.assign(swapchainImageCount, VK_NULL_HANDLE);
        vku::err_check(vkAllocateCommandBuffers(VulkanSharedInfo::getInstance()->device,
                                                &commandBufferAllocateInfo,
                                                commandBuffers.data()));
    }

    void ContextImplVulkan::record_command_buffer(std::uint32_t image_index,
                                                   VkDescriptorSet descriptor_set)
    {
        if (image_index >= commandBuffers.size() || image_index >= frameBuffers.size() ||
            descriptor_set == VK_NULL_HANDLE)
            throw std::logic_error("Vulkan frame recording resources are incomplete");

        VkCommandBuffer command_buffer = commandBuffers[image_index];
        vku::err_check(vkResetCommandBuffer(command_buffer, 0));
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vku::err_check(vkBeginCommandBuffer(command_buffer, &begin_info));
        record_draw_commands(command_buffer, frameBuffers[image_index], descriptor_set);
        vku::err_check(vkEndCommandBuffer(command_buffer));
    }

    void ContextImplVulkan::record_draw_sequences(VkCommandBuffer command_buffer,
                                                   VkDescriptorSet descriptor_set)
    {
        VkViewport viewport{};
        viewport.width = static_cast<float>(_width);
        viewport.height = static_cast<float>(_height);
        viewport.maxDepth = 1.0f;

        VkClearAttachment clear_stencil{};
        clear_stencil.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        clear_stencil.clearValue.depthStencil = {1.0f, 0};
        VkClearRect clear_rect{};
        clear_rect.rect.extent = {static_cast<std::uint32_t>(_width),
                                  static_cast<std::uint32_t>(_height)};
        clear_rect.layerCount = 1;

        for (std::size_t sequence_index = 0; sequence_index < draw_sequence_chain.size();
             ++sequence_index)
        {
            const draw_sequence& sequence = draw_sequence_chain[sequence_index];
            const std::uint32_t sequence_start =
                static_cast<std::uint32_t>(sequence.start_index);
            std::uint32_t sequence_end = static_cast<std::uint32_t>(storage.size());
            if (sequence_index + 1U < draw_sequence_chain.size())
            {
                const draw_sequence& next = draw_sequence_chain[sequence_index + 1U];
                sequence_end = next.convex_mask_active
                                   ? next.mask_instance_index
                                   : static_cast<std::uint32_t>(next.start_index);
            }

            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &sequence.scissor);
            vkCmdBindIndexBuffer(command_buffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout, 0, 1, &descriptor_set, 0, nullptr);

            if (sequence.convex_mask_active)
            {
                vkCmdClearAttachments(command_buffer, 1, &clear_stencil, 1, &clear_rect);
                if (sequence.mask_triangle_count > 0U)
                {
                    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      convexMaskPipeline);
                    vkCmdDrawIndexed(command_buffer,
                                     static_cast<std::uint32_t>(rect_indices.size()),
                                     sequence.mask_triangle_count, 0, 0,
                                     sequence.mask_instance_index);
                }
            }

            std::uint32_t cursor = sequence_start;
            for (const path_draw& path : path_draws)
            {
                if (path.instance_index < sequence_start || path.instance_index >= sequence_end)
                    continue;
                if (path.instance_index > cursor)
                {
                    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      sequence.convex_mask_active ? clippedPipeline : pipeline);
                    vkCmdDrawIndexed(command_buffer,
                                     static_cast<std::uint32_t>(rect_indices.size()),
                                     path.instance_index - cursor, 0, 0, cursor);
                }

                if (path.blur_sample_count > 0U)
                {
                    const VkPipeline path_mask_pipeline = sequence.convex_mask_active
                        ? (path.even_odd ? evenOddClippedMaskPipeline
                                         : unionClippedMaskPipeline)
                        : (path.even_odd ? evenOddMaskPipeline : unionMaskPipeline);
                    for (std::uint32_t sample = 0; sample < path.blur_sample_count; ++sample)
                    {
                        const vec2 offset =
                            path_sample_offsets[path.sample_offset_index + sample];
                        VkViewport shifted_viewport = viewport;
                        shifted_viewport.x += offset.get_x();
                        shifted_viewport.y += offset.get_y();
                        vkCmdSetViewport(command_buffer, 0, 1, &shifted_viewport);
                        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          path_mask_pipeline);
                        vkCmdDrawIndexed(command_buffer,
                                         static_cast<std::uint32_t>(rect_indices.size()),
                                         path.mask_count, 0, 0, path.instance_index);
                        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          sequence.convex_mask_active
                                              ? clippedPathPaintPipeline
                                              : pathPaintPipeline);
                        vkCmdDrawIndexed(command_buffer,
                                         static_cast<std::uint32_t>(rect_indices.size()), 1, 0,
                                         0, path.instance_index + path.mask_count + sample);
                        if (sequence.convex_mask_active)
                        {
                            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                              pathClearPipeline);
                            vkCmdDrawIndexed(command_buffer,
                                             static_cast<std::uint32_t>(rect_indices.size()),
                                             path.mask_count, 0, 0, path.instance_index);
                        }
                    }
                    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
                    cursor = path.instance_index + path.mask_count + path.blur_sample_count;
                    continue;
                }

                if (path.erosion_sample_count > 0U)
                {
                    for (std::uint32_t sample = 0; sample < path.erosion_sample_count; ++sample)
                    {
                        const vec2 offset =
                            path_sample_offsets[path.sample_offset_index + sample];
                        VkViewport shifted_viewport = viewport;
                        shifted_viewport.x += offset.get_x();
                        shifted_viewport.y += offset.get_y();
                        vkCmdSetViewport(command_buffer, 0, 1, &shifted_viewport);
                        const VkPipeline base_mask_pipeline = sequence.convex_mask_active
                            ? (path.even_odd ? evenOddClippedMaskPipeline
                                             : unionClippedMaskPipeline)
                            : (path.even_odd ? evenOddMaskPipeline : unionMaskPipeline);
                        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          base_mask_pipeline);
                        vkCmdDrawIndexed(command_buffer,
                                         static_cast<std::uint32_t>(rect_indices.size()),
                                         path.mask_count, 0, 0, path.instance_index);

                        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          pathClearPipeline);
                        vkCmdDrawIndexed(command_buffer,
                                         static_cast<std::uint32_t>(rect_indices.size()),
                                         path.erosion_mask_count, 0, 0,
                                         path.instance_index + path.mask_count);

                        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          sequence.convex_mask_active ? clippedPathPaintPipeline
                                                                      : pathPaintPipeline);
                        vkCmdDrawIndexed(command_buffer,
                                         static_cast<std::uint32_t>(rect_indices.size()), 1, 0,
                                         0, path.instance_index + path.mask_count +
                                                path.erosion_mask_count + sample);

                        if (sequence.convex_mask_active)
                        {
                            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                              pathClearPipeline);
                            vkCmdDrawIndexed(command_buffer,
                                             static_cast<std::uint32_t>(rect_indices.size()),
                                             path.mask_count, 0, 0, path.instance_index);
                        }
                    }
                    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
                    cursor = path.instance_index + path.mask_count +
                             path.erosion_mask_count + path.erosion_sample_count;
                    continue;
                }

                if (path.inset_sample_count > 0U)
                {
                    const VkPipeline original_mask_pipeline = sequence.convex_mask_active
                        ? (path.even_odd ? evenOddClippedMaskPipeline
                                         : unionClippedMaskPipeline)
                        : (path.even_odd ? evenOddMaskPipeline : unionMaskPipeline);
                    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      original_mask_pipeline);
                    vkCmdDrawIndexed(command_buffer,
                                     static_cast<std::uint32_t>(rect_indices.size()),
                                     path.mask_count, 0, 0, path.instance_index);

                    std::uint32_t sample_instance = path.instance_index + path.mask_count;
                    for (std::uint32_t sample = 0; sample < path.inset_sample_count; ++sample)
                    {
                        const vec2 offset =
                            path_sample_offsets[path.sample_offset_index + sample];
                        VkViewport shifted_viewport = viewport;
                        shifted_viewport.x += offset.get_x();
                        shifted_viewport.y += offset.get_y();
                        vkCmdSetViewport(command_buffer, 0, 1, &shifted_viewport);
                        const VkPipeline shifted_mask_pipeline = sequence.convex_mask_active
                            ? (path.shifted_even_odd ? shiftedEvenOddClippedMaskPipeline
                                             : shiftedUnionClippedMaskPipeline)
                            : (path.shifted_even_odd ? shiftedEvenOddMaskPipeline
                                             : shiftedUnionMaskPipeline);
                        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          shifted_mask_pipeline);
                        vkCmdDrawIndexed(command_buffer,
                                         static_cast<std::uint32_t>(rect_indices.size()),
                                         path.shifted_base_count, 0, 0, sample_instance);
                        if (path.shifted_extra_count > 0U)
                        {
                            vkCmdBindPipeline(
                                command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                sequence.convex_mask_active ? shiftedUnionClippedMaskPipeline
                                                            : shiftedUnionMaskPipeline);
                            vkCmdDrawIndexed(
                                command_buffer,
                                static_cast<std::uint32_t>(rect_indices.size()),
                                path.shifted_extra_count, 0, 0,
                                sample_instance + path.shifted_base_count);
                        }

                        vkCmdBindPipeline(
                            command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            sequence.convex_mask_active ? clippedInsetPathPaintPipeline
                                                        : insetPathPaintPipeline);
                        vkCmdDrawIndexed(command_buffer,
                                         static_cast<std::uint32_t>(rect_indices.size()),
                                         1, 0, 0, sample_instance + path.shifted_base_count +
                                                      path.shifted_extra_count + sample);

                        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          shiftedPathClearPipeline);
                        vkCmdDrawIndexed(command_buffer,
                                         static_cast<std::uint32_t>(rect_indices.size()),
                                         path.shifted_base_count + path.shifted_extra_count,
                                         0, 0, sample_instance);
                    }

                    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

                    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      pathClearPipeline);
                    vkCmdDrawIndexed(command_buffer,
                                     static_cast<std::uint32_t>(rect_indices.size()),
                                     path.mask_count, 0, 0, path.instance_index);
                    cursor = sample_instance + path.shifted_base_count +
                             path.shifted_extra_count + path.inset_sample_count;
                    continue;
                }

                const VkPipeline path_mask_pipeline = sequence.convex_mask_active
                    ? (path.even_odd ? evenOddClippedMaskPipeline
                                     : unionClippedMaskPipeline)
                    : (path.even_odd ? evenOddMaskPipeline : unionMaskPipeline);
                vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  path_mask_pipeline);
                vkCmdDrawIndexed(command_buffer,
                                 static_cast<std::uint32_t>(rect_indices.size()),
                                 path.mask_count, 0, 0, path.instance_index);
                vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  sequence.convex_mask_active ? clippedPathPaintPipeline
                                                              : pathPaintPipeline);
                vkCmdDrawIndexed(command_buffer,
                                 static_cast<std::uint32_t>(rect_indices.size()), 1, 0, 0,
                                 path.instance_index + path.mask_count);

                if (sequence.convex_mask_active)
                {
                    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      pathClearPipeline);
                    vkCmdDrawIndexed(command_buffer,
                                     static_cast<std::uint32_t>(rect_indices.size()),
                                     path.mask_count, 0, 0, path.instance_index);
                }
                cursor = path.instance_index + path.mask_count + 1U;
            }
            if (cursor < sequence_end)
            {
                vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  sequence.convex_mask_active ? clippedPipeline : pipeline);
                vkCmdDrawIndexed(command_buffer,
                                 static_cast<std::uint32_t>(rect_indices.size()),
                                 sequence_end - cursor, 0, 0, cursor);
            }
        }
    }

    void ContextImplVulkan::record_draw_commands(VkCommandBuffer command_buffer,
                                                  VkFramebuffer framebuffer,
                                                  VkDescriptorSet descriptor_set)
    {
        VkRenderPassBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        begin_info.renderPass = renderPass;
        begin_info.framebuffer = framebuffer;
        begin_info.renderArea.extent = {static_cast<std::uint32_t>(_width),
                                        static_cast<std::uint32_t>(_height)};
        std::array<VkClearValue, 2> clear_values{};
        clear_values[0] = clearValue;
        clear_values[1].depthStencil = {1.0f, 0};
        begin_info.clearValueCount = static_cast<std::uint32_t>(clear_values.size());
        begin_info.pClearValues = clear_values.data();
        vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
        record_draw_sequences(command_buffer, descriptor_set);

        vkCmdEndRenderPass(command_buffer);
    }

    void ContextImplVulkan::sync_external_target()
    {
        VulkanSharedInfo* shared = VulkanSharedInfo::getInstance();
        if (external_target_ == nullptr || external_target_->instance != shared->instance ||
            external_target_->physical_device != shared->bestPhysicalDevice ||
            external_target_->device != shared->device || external_target_->queue != queue ||
            external_target_->queue_family_index != shared->queueFamilyIndex ||
            external_target_->command_buffer == VK_NULL_HANDLE ||
            external_target_->image == VK_NULL_HANDLE ||
            external_target_->extent.width != swapchainExtent.width ||
            external_target_->extent.height != swapchainExtent.height ||
            external_target_->format != selectedImageFormat ||
            external_target_->final_layout == VK_IMAGE_LAYOUT_UNDEFINED ||
            external_target_->final_stage_mask == 0)
        {
            throw std::invalid_argument(
                "External Vulkan target identity, extent, format or frame handles changed");
        }

        if (swapchainImages[0] == external_target_->image)
            return;

        if (frameBuffers[0] != VK_NULL_HANDLE)
            vkDestroyFramebuffer(shared->device, frameBuffers[0], nullptr);
        if (imageViews[0] != VK_NULL_HANDLE)
            vkDestroyImageView(shared->device, imageViews[0], nullptr);
        frameBuffers.clear();
        imageViews.clear();
        swapchainImages.clear();
        create_image_views();
        create_framebuffer();
    }

    void ContextImplVulkan::record_external_frame()
    {
        sync_external_target();
        if (!storage.empty())
            update_storage(frameResources[0]);

        record_draw_commands(external_target_->command_buffer, frameBuffers[0],
                             frameResources[0].descriptor_set);
        if (external_target_->final_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = external_target_->final_access_mask;
            barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.newLayout = external_target_->final_layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = external_target_->image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(external_target_->command_buffer,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 external_target_->final_stage_mask, 0, 0, nullptr, 0, nullptr,
                                 1, &barrier);
        }
        external_target_->image_layout = external_target_->final_layout;
        dirty = false;
        clear_draw_queue();
        reset_transient_path_resources();
    }

    void ContextImplVulkan::append_uniform(uniform_rect value)
    {
        if (storage.size() >= uniform_rect_capacity)
            throw std::length_error("Vulkan draw storage limit reached");
        storage.push_back(value);
    }

    void ContextImplVulkan::clear_draw_queue()
    {
        storage.clear();
        path_draws.clear();
        path_sample_offsets.clear();
        if (draw_sequence_chain.empty())
        {
            draw_sequence_chain.push_back(
                {0, {{0, 0}, {static_cast<std::uint32_t>(_width),
                               static_cast<std::uint32_t>(_height)}}});
        }
        else
        {
            draw_sequence_chain.resize(1);
            draw_sequence_chain.front() =
                {0, {{0, 0}, {static_cast<std::uint32_t>(_width),
                               static_cast<std::uint32_t>(_height)}}};
        }
    }

    void ContextImplVulkan::reset_frame_state() noexcept
    {
        imageIndex = invalid_image_index;
        rendering = false;
    }

    void ContextImplVulkan::create_semaphores()
    {
        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.pNext = nullptr;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphoreCreateInfo{};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.pNext = nullptr;
        semaphoreCreateInfo.flags = 0;

        for (frame_resources& frame : frameResources)
        {
            vku::err_check(vkCreateFence(VulkanSharedInfo::getInstance()->device,
                                         &fenceCreateInfo, nullptr, &frame.render_fence));
            vku::err_check(vkCreateSemaphore(VulkanSharedInfo::getInstance()->device,
                                             &semaphoreCreateInfo, nullptr,
                                             &frame.image_available));
            vku::err_check(vkCreateSemaphore(VulkanSharedInfo::getInstance()->device,
                                             &semaphoreCreateInfo, nullptr,
                                             &frame.rendering_complete));
        }
        imagesInFlight.assign(swapchainImageCount, VK_NULL_HANDLE);
        currentFrame = 0U;
    }

    void ContextImplVulkan::initialize_resources()
    {
        const std::uint8_t buffer[4] = {0, 0, 0, 0};
        dummy =
            std::make_unique<ImageImplVulkan>(1, 1, 4, buffer, sizeof(buffer), ImageConfig{});
        dummy->upload(commandPool, queue);
    }

    void ContextImplVulkan::update_swapchain(uint32_t width, uint32_t height)
    {
        if (resizing)
            return;
        resizing = true;
        try
        {
            this->_width = width;
            this->_height = height;

            const VkDevice device = VulkanSharedInfo::getInstance()->device;
            vku::err_check(vkDeviceWaitIdle(device));
            reset_frame_state();
            swapchain_suboptimal = false;
            reset_transient_path_resources();

            for (frame_resources& frame : frameResources)
            {
                if (frame.render_fence != VK_NULL_HANDLE)
                    vkDestroyFence(device, frame.render_fence, nullptr);
                if (frame.image_available != VK_NULL_HANDLE)
                    vkDestroySemaphore(device, frame.image_available, nullptr);
                if (frame.rendering_complete != VK_NULL_HANDLE)
                    vkDestroySemaphore(device, frame.rendering_complete, nullptr);
                frame.render_fence = VK_NULL_HANDLE;
                frame.image_available = VK_NULL_HANDLE;
                frame.rendering_complete = VK_NULL_HANDLE;
            }
            imagesInFlight.clear();
            if (commandPool != VK_NULL_HANDLE)
            {
                if (!commandBuffers.empty())
                    vkFreeCommandBuffers(device, commandPool,
                                         static_cast<std::uint32_t>(commandBuffers.size()),
                                         commandBuffers.data());
                vkDestroyCommandPool(device, commandPool, nullptr);
                commandPool = VK_NULL_HANDLE;
            }
            for (VkFramebuffer framebuffer : frameBuffers)
                if (framebuffer != VK_NULL_HANDLE)
                    vkDestroyFramebuffer(device, framebuffer, nullptr);
            destroy_pipeline_resources();
            if (renderPass != VK_NULL_HANDLE)
            {
                vkDestroyRenderPass(device, renderPass, nullptr);
                renderPass = VK_NULL_HANDLE;
            }
            destroy_stencil_resources();
            for (VkImageView image_view : imageViews)
                if (image_view != VK_NULL_HANDLE)
                    vkDestroyImageView(device, image_view, nullptr);

            commandBuffers.clear();
            frameBuffers.clear();
            imageViews.clear();
            swapchainImages.clear();

            const VkSwapchainKHR old_swapchain = swapchain;

            create_swapchain();
            vkDestroySwapchainKHR(device, old_swapchain, nullptr);
            create_image_views();
            create_stencil_resources();
            create_render_pass();
            create_pipeline();
            create_framebuffer();
            create_command_pool();
            create_command_buffers();
            create_semaphores();
            resizing = false;
        }
        catch (...)
        {
            resizing = false;
            throw;
        }
    }

    ContextImplVulkan::ContextImplVulkan(const vulkan::CreateInfo& create_info)
        : Context(create_info.metrics, create_info.resource_limits), _host(create_info.host),
          _vsync(create_info.vsync)
    {
        if (_host.required_instance_extensions == nullptr || _host.create_surface == nullptr)
        {
            throw std::invalid_argument("Vulkan host callbacks must all be provided");
        }
        std::uint32_t extension_count = 0;
        const char* const* extension_names =
            _host.required_instance_extensions(_host.user_data, &extension_count);
        if (extension_count == 0 || extension_names == nullptr)
        {
            throw std::invalid_argument("Vulkan host returned no instance extensions");
        }
        std::vector<std::string> required_extensions;
        required_extensions.reserve(extension_count);
        for (std::uint32_t index = 0; index < extension_count; ++index)
        {
            if (extension_names[index] == nullptr)
            {
                throw std::invalid_argument("Vulkan host returned a null extension name");
            }
            required_extensions.emplace_back(extension_names[index]);
        }
        VulkanSharedInfo::retain(required_extensions);
        shared_retained_ = true;
        try
        {
            initialize_limits();
            create_surface();
            check_surface_support();
            create_queue();

            create_swapchain();
            create_image_views();
            create_stencil_resources();
            create_render_pass();
            create_descriptor_set_layout();
            create_pipeline();
            create_framebuffer();
            create_command_pool();
            create_command_buffers();
            initialize_resources();
            create_semaphores();
            prepare();

#ifndef NDEBUG
            std::cout << "{\n";
            vku::print_layers();
            vku::print_physical_devices();
            vku::print_selected_device();
            std::cout << "}\n";
#endif

            remove_rect_mask();
        }
        catch (...)
        {
            cleanup();
            throw;
        }
    }

    ContextImplVulkan::ContextImplVulkan(const vulkan::ExternalCreateInfo& create_info)
        : Context(create_info.metrics, create_info.resource_limits),
          external_target_(create_info.target)
    {
        if (external_target_ == nullptr || external_target_->instance == VK_NULL_HANDLE ||
            external_target_->physical_device == VK_NULL_HANDLE ||
            external_target_->device == VK_NULL_HANDLE ||
            external_target_->queue == VK_NULL_HANDLE ||
            external_target_->queue_family_index == VK_QUEUE_FAMILY_IGNORED ||
            external_target_->command_buffer == VK_NULL_HANDLE ||
            external_target_->image == VK_NULL_HANDLE || external_target_->extent.width == 0 ||
            external_target_->extent.height == 0 ||
            external_target_->final_layout == VK_IMAGE_LAYOUT_UNDEFINED ||
            external_target_->final_stage_mask == 0)
        {
            throw std::invalid_argument("External Vulkan target is incomplete");
        }
        const bool supported_format =
            external_target_->format == VK_FORMAT_B8G8R8A8_UNORM ||
            external_target_->format == VK_FORMAT_B8G8R8A8_SRGB ||
            external_target_->format == VK_FORMAT_R8G8B8A8_UNORM ||
            external_target_->format == VK_FORMAT_R8G8B8A8_SRGB;
        if (!supported_format || external_target_->extent.width >
                                     static_cast<std::uint32_t>((std::numeric_limits<int>::max)()) ||
            external_target_->extent.height >
                static_cast<std::uint32_t>((std::numeric_limits<int>::max)()))
        {
            throw std::invalid_argument("External Vulkan format or extent is unsupported");
        }
        std::uint32_t family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(external_target_->physical_device,
                                                 &family_count, nullptr);
        if (external_target_->queue_family_index >= family_count)
            throw std::invalid_argument("External Vulkan queue family index is invalid");
        std::vector<VkQueueFamilyProperties> families(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(external_target_->physical_device,
                                                 &family_count, families.data());
        if ((families[external_target_->queue_family_index].queueFlags &
             VK_QUEUE_GRAPHICS_BIT) == 0)
            throw std::invalid_argument("External Vulkan queue family must support graphics");
        if (_width != static_cast<int>(external_target_->extent.width) ||
            _height != static_cast<int>(external_target_->extent.height))
        {
            throw std::invalid_argument(
                "External Vulkan target extent must match gCanvas metrics");
        }

        VulkanSharedInfo::retain_external(
            external_target_->instance, external_target_->physical_device,
            external_target_->device, external_target_->queue_family_index,
            external_target_->api_version);
        shared_retained_ = true;
        try
        {
            queue = external_target_->queue;
            selectedImageFormat = external_target_->format;
            swapchainExtent = external_target_->extent;
            initialize_limits();
            create_image_views();
            create_stencil_resources();
            create_render_pass();
            create_descriptor_set_layout();
            create_pipeline();
            create_framebuffer();
            create_command_pool();
            initialize_resources();
            prepare();
            remove_rect_mask();
        }
        catch (...)
        {
            cleanup();
            throw;
        }
    }

    ContextImplVulkan::~ContextImplVulkan()
    {
        cleanup();
    }

    void ContextImplVulkan::cleanup() noexcept
    {
        if (!shared_retained_)
        {
            return;
        }
        VulkanSharedInfo* shared = VulkanSharedInfo::getInstance();
        const VkDevice device = shared->device;
        if (external_target_ == nullptr)
            vkDeviceWaitIdle(device);

        release_owned_resources();
        dummy.reset();

        if (descriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        destroy_pipeline_resources();
        if (descriptorSetLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        const std::size_t frame_count = external_target_ == nullptr ? frames_in_flight : 1U;
        for (std::size_t frame_index = 0; frame_index < frame_count; ++frame_index)
        {
            frame_resources& frame = frameResources[frame_index];
            if (frame.storage_buffer != VK_NULL_HANDLE)
                vmaDestroyBuffer(shared->allocator, frame.storage_buffer,
                                 frame.storage_allocation);
            if (frame.render_fence != VK_NULL_HANDLE)
                vkDestroyFence(device, frame.render_fence, nullptr);
            if (frame.image_available != VK_NULL_HANDLE)
                vkDestroySemaphore(device, frame.image_available, nullptr);
            if (frame.rendering_complete != VK_NULL_HANDLE)
                vkDestroySemaphore(device, frame.rendering_complete, nullptr);
            frame = {};
        }
        if (indexBuffer != VK_NULL_HANDLE)
            vmaDestroyBuffer(shared->allocator, indexBuffer, indexBufferAllocation);
        if (commandPool != VK_NULL_HANDLE)
        {
            if (!commandBuffers.empty())
                vkFreeCommandBuffers(device, commandPool,
                                     static_cast<std::uint32_t>(commandBuffers.size()),
                                     commandBuffers.data());
            vkDestroyCommandPool(device, commandPool, nullptr);
        }
        for (VkFramebuffer framebuffer : frameBuffers)
            if (framebuffer != VK_NULL_HANDLE)
                vkDestroyFramebuffer(device, framebuffer, nullptr);
        if (renderPass != VK_NULL_HANDLE)
            vkDestroyRenderPass(device, renderPass, nullptr);
        destroy_stencil_resources();
        for (VkImageView image_view : imageViews)
            if (image_view != VK_NULL_HANDLE)
                vkDestroyImageView(device, image_view, nullptr);
        if (swapchain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(device, swapchain, nullptr);
        if (surface != VK_NULL_HANDLE)
            vkDestroySurfaceKHR(shared->instance, surface, nullptr);

        commandBuffers.clear();
        frameBuffers.clear();
        imageViews.clear();
        swapchainImages.clear();
        imagesInFlight.clear();
        shared_retained_ = false;
        VulkanSharedInfo::release();
    }

    void ContextImplVulkan::create_index_buffers()
    {
        vku::create_and_upload_buffer(rect_indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer,
                                      indexBufferAllocation, commandPool, queue);
    }

    void ContextImplVulkan::create_storage_buffer()
    {
        const std::size_t frame_count = external_target_ == nullptr ? frames_in_flight : 1U;
        for (std::size_t frame_index = 0; frame_index < frame_count; ++frame_index)
        {
            frame_resources& frame = frameResources[frame_index];
            VmaAllocationInfo allocation_info{};
            vku::create_buffer(uniform_rect_buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               frame.storage_buffer,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               frame.storage_allocation, VMA_ALLOCATION_CREATE_MAPPED_BIT,
                               &allocation_info);
            frame.storage_mapped_data = allocation_info.pMappedData;
            if (frame.storage_mapped_data == nullptr)
                throw std::runtime_error(
                    "Vulkan storage buffer persistent mapping is unavailable");
        }
    }

    void ContextImplVulkan::create_descriptor_pool()
    {
        const std::uint32_t frame_count = static_cast<std::uint32_t>(
            external_target_ == nullptr ? frames_in_flight : 1U);
        /*
        VkDescriptorPoolSize uniformDescriptorPoolSize;
        uniformDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniformDescriptorPoolSize.descriptorCount = 1;
        */

        VkDescriptorPoolSize storageDescriptorPoolSize{};
        storageDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        storageDescriptorPoolSize.descriptorCount = frame_count;

        VkDescriptorPoolSize samplerDescriptorPoolSize{};
        samplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        if (texture_array_size > std::numeric_limits<std::uint32_t>::max() / frame_count)
            throw std::overflow_error("Vulkan descriptor pool size overflow");
        samplerDescriptorPoolSize.descriptorCount = texture_array_size * frame_count;

        std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {// uniformDescriptorPoolSize,
                                                                 storageDescriptorPoolSize,
                                                                 samplerDescriptorPoolSize};

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.pNext = nullptr;
        descriptorPoolCreateInfo.flags = 0;
        descriptorPoolCreateInfo.maxSets = frame_count;
        descriptorPoolCreateInfo.poolSizeCount = (uint32_t)descriptorPoolSizes.size();
        descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();

        vku::err_check(vkCreateDescriptorPool(VulkanSharedInfo::getInstance()->device,
                                              &descriptorPoolCreateInfo, nullptr, &descriptorPool));
    }

    void ContextImplVulkan::create_descriptor_set()
    {
        const std::uint32_t frame_count = static_cast<std::uint32_t>(
            external_target_ == nullptr ? frames_in_flight : 1U);
        std::array<VkDescriptorSetLayout, frames_in_flight> layouts{};
        std::array<VkDescriptorSet, frames_in_flight> descriptor_sets{};
        layouts.fill(descriptorSetLayout);
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.pNext = nullptr;
        descriptorSetAllocateInfo.descriptorPool = descriptorPool;
        descriptorSetAllocateInfo.descriptorSetCount = frame_count;
        descriptorSetAllocateInfo.pSetLayouts = layouts.data();

        vku::err_check(vkAllocateDescriptorSets(VulkanSharedInfo::getInstance()->device,
                                                &descriptorSetAllocateInfo,
                                                descriptor_sets.data()));

        /*
        // Uniform Buffer
        VkDescriptorBufferInfo descriptorBufferInfo;
        descriptorBufferInfo.buffer = uniformBuffer;
        descriptorBufferInfo.offset = 0;
        descriptorBufferInfo.range = properties.limits.maxUniformBufferRange;

        VkWriteDescriptorSet uniformWriteDescriptorSet;
        uniformWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uniformWriteDescriptorSet.pNext = nullptr;
        uniformWriteDescriptorSet.dstSet = descriptorSet;
        uniformWriteDescriptorSet.dstBinding = 0;
        uniformWriteDescriptorSet.dstArrayElement = 0;
        uniformWriteDescriptorSet.descriptorCount = 1;
        uniformWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniformWriteDescriptorSet.pImageInfo = nullptr;
        uniformWriteDescriptorSet.pBufferInfo = &descriptorBufferInfo;
        uniformWriteDescriptorSet.pTexelBufferView = nullptr;
        */

        VkDescriptorImageInfo dummy_image_info{};
        dummy_image_info.sampler = dummy->_sampler;
        dummy_image_info.imageView = dummy->_imageView;
        dummy_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        std::vector<VkDescriptorImageInfo> descriptorImageInfos(texture_array_size,
                                                                 dummy_image_info);
        for (std::size_t index = 0; index < images.size(); ++index)
        {
            descriptorImageInfos[index].sampler = images[index]->_sampler;
            descriptorImageInfos[index].imageView = images[index]->_imageView;
        }

        for (std::uint32_t frame_index = 0; frame_index < frame_count; ++frame_index)
        {
            frame_resources& frame = frameResources[frame_index];
            frame.descriptor_set = descriptor_sets[frame_index];

            VkDescriptorBufferInfo storage_info{};
            storage_info.buffer = frame.storage_buffer;
            storage_info.range = uniform_rect_buffer_size;

            VkWriteDescriptorSet storage_write{};
            storage_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            storage_write.dstSet = frame.descriptor_set;
            storage_write.dstBinding = 0;
            storage_write.descriptorCount = 1;
            storage_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            storage_write.pBufferInfo = &storage_info;

            VkWriteDescriptorSet sampler_write{};
            sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            sampler_write.dstSet = frame.descriptor_set;
            sampler_write.dstBinding = 1;
            sampler_write.descriptorCount = texture_array_size;
            sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            sampler_write.pImageInfo = descriptorImageInfos.data();

            const std::array<VkWriteDescriptorSet, 2> writes = {storage_write, sampler_write};
            vkUpdateDescriptorSets(VulkanSharedInfo::getInstance()->device,
                                   static_cast<std::uint32_t>(writes.size()), writes.data(), 0,
                                   nullptr);
        }
    }

    void ContextImplVulkan::update_storage(frame_resources& frame)
    {
        if (storage.size() > uniform_rect_capacity)
            throw std::logic_error("Vulkan draw storage invariant violated");
        const VkDeviceSize bufferSize = storage.size() * sizeof(uniform_rect);

        if (frame.storage_mapped_data == nullptr)
            throw std::logic_error("Vulkan storage buffer is not mapped");
        std::memcpy(frame.storage_mapped_data, storage.data(), bufferSize);
    }

} // namespace gcanvas
