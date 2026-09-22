#include "context_impl_gl.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "font_impl_gl.hpp"
#include "../../path_tessellator.hpp"
#include "../../resources.hpp"
#include "../../utf8.hpp"


namespace gcanvas
{
namespace GCANVAS_GL_PROFILE_NAMESPACE
{
    namespace
    {
        constexpr GLuint path_stencil_bit = 0x01U;
        constexpr GLuint clip_stencil_bit = 0x02U;
        constexpr GLuint shifted_path_stencil_bit = 0x04U;
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
                throw std::invalid_argument("OpenGL inset alpha transform is singular");
            const float local_x =
                (delta_x * edge_y.get_y() - delta_y * edge_y.get_x()) / determinant;
            const float local_y =
                (edge_x.get_x() * delta_y - edge_x.get_y() * delta_x) / determinant;
            return vec2(local_x * (uv_maximum.get_x() - uv_minimum.get_x()),
                        local_y * (uv_maximum.get_y() - uv_minimum.get_y()));
        }

        ContextImplGl::uniform_rect path_uniform(
            const detail::MeshQuad& quad, int width, int height, color value,
            int sampler_index = -1, detail::MeshPoint uv_min = {0.0f, 0.0f},
            detail::MeshPoint uv_max = {1.0f, 1.0f})
        {
            if (width <= 0 || height <= 0)
                throw std::logic_error("OpenGL path draw requires a non-empty canvas");
            ContextImplGl::uniform_rect result{};
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

        void shift_uniform_vertices(ContextImplGl::uniform_rect& value, float x, float y)
        {
            for (vec2& vertex : value.vertices)
            {
                vertex.x() += x;
                vertex.y() += y;
            }
        }

        ContextImplGl::uniform_rect mask_triangle_uniform(
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
                    throw std::invalid_argument("OpenGL convex mask transform overflow");
            }
            return path_uniform(quad, width, height, color(255, 255, 255, 255));
        }

        void upload_uniform_range(ContextImplGl& context, int first, int count)
        {
            glBindBuffer(GL_UNIFORM_BUFFER, context.storageBuffer);
            glBufferSubData(
                GL_UNIFORM_BUFFER, 0,
                static_cast<GLsizei>(count * sizeof(ContextImplGl::uniform_rect)),
                &context.storage[static_cast<std::size_t>(first)]);
        }

        void draw_uploaded_uniform_range(ContextImplGl& context, int first, int count)
        {
            glUniform1i(context.instance_offset_location, first);
            glBindVertexArray(context.vertex_array_object);
            glDrawElementsInstanced(GL_TRIANGLES,
                                    static_cast<GLsizei>(context.rect_indices.size()),
                                    GL_UNSIGNED_INT, nullptr, count);
        }

        void draw_uniform_range(ContextImplGl& context, int first, int count)
        {
            while (count > 0)
            {
                const int batch = std::min(count, context.MAX_UNIFORM_RECT_PER_BLOCK_COUNT);
                upload_uniform_range(context, first, batch);
                draw_uploaded_uniform_range(context, 0, batch);
                first += batch;
                count -= batch;
            }
        }
    }

    /* ------------------------ DOWNCAST ------------------------ */

    static inline ContextImplGl* getImpl(Context* ptr)
    {
        return (ContextImplGl*)ptr;
    }
    static inline const ContextImplGl* getImpl(const Context* ptr)
    {
        return (const ContextImplGl*)ptr;
    }

    /* ------------------------ PUBLIC IMPLEMENTATION ------------------------ */

    void ContextImplGl::stroke_rect(float x, float y, float width, float height)
    {
        ContextImplGl* impl = getImpl(this);

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

        impl->storage.push_back({
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
        impl->queue_color_call();
    }

    void ContextImplGl::stroke_rounded_rect(float x, float y, float width, float height,
                                      float border_radius)
    {
        stroke_rounded_rect(x, y, width, height, border_radius, border_radius, border_radius,
                            border_radius);
    }

    void ContextImplGl::stroke_rounded_rect(float x, float y, float width, float height, float radius_nw,
                                      float radius_ne, float radius_se, float radius_sw)
    {
        ContextImplGl* impl = getImpl(this);
        
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
     
        float nwf = radius_nw  * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float nef = radius_ne  * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float sef = radius_se  * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float swf = radius_sw  * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);

        impl->storage.push_back({
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
        impl->queue_color_call();
    }

    void ContextImplGl::stroke_circle(float x, float y, float radius)
    {
        ContextImplGl* impl = getImpl(this);

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
        
        float radf = width/ 2.0f;

        impl->storage.push_back({
            _stroke_color,                                                          // color
            {vec2(xf, yf) * 2.0f - vec2(1), vec2(xf + widthf, yf) * 2.0f - vec2(1), //
             vec2(xf, yf + heightf) * 2.0f - vec2(1),//
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
        impl->queue_color_call();
    }


    void ContextImplGl::stroke_ellipse(float x, float y, float radius_x, float radius_y)
    {
        ContextImplGl* impl = getImpl(this);
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
        impl->storage.push_back({
            _stroke_color,
            {vec2(left, top) * 2.0f - vec2(1),
             vec2(left + normalized_width, top) * 2.0f - vec2(1),
             vec2(left, top + normalized_height) * 2.0f - vec2(1),
             vec2(left + normalized_width, top + normalized_height) * 2.0f - vec2(1)},
            {0, 0, 0, 0}, -1, 0, vec2(width, height), {vec2(0), vec2(1)},
            {line_width, line_width, line_width, line_width}, 0,
            {0, 0, analytic_ellipse_stroke_effect},
        });
        impl->queue_color_call();
    }

    void ContextImplGl::fill_rect(float x, float y, float width, float height)
    {
        ContextImplGl* impl = getImpl(this);
        
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
  
        impl->storage.push_back({
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
        impl->queue_color_call();
    }

    void ContextImplGl::fill_rounded_rect(float x, float y, float width, float height,
                                    float border_radius)
    {
        fill_rounded_rect(x, y, width, height, border_radius, border_radius, border_radius,
                          border_radius);
    }

    void ContextImplGl::fill_rounded_rect(float x, float y, float width, float height, float radius_nw,
                                    float radius_ne, float radius_se, float radius_sw)
    {
        ContextImplGl* impl = getImpl(this);

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

        impl->storage.push_back({
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
        impl->queue_color_call();
    }

    void ContextImplGl::fill_circle(float x, float y, float radius)
    {
        ContextImplGl* impl = getImpl(this);

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

        impl->storage.push_back({
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
        impl->queue_color_call();
    }

    
    void ContextImplGl::fill_ellipse(float x, float y, float radius_x, float radius_y)
    {
        ContextImplGl* impl = getImpl(this);
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
        impl->storage.push_back({
            _fill_color,
            {vec2(left, top) * 2.0f - vec2(1),
             vec2(left + normalized_width, top) * 2.0f - vec2(1),
             vec2(left, top + normalized_height) * 2.0f - vec2(1),
             vec2(left + normalized_width, top + normalized_height) * 2.0f - vec2(1)},
            {0, 0, 0, 0}, -1, 0, vec2(width, height), {vec2(0), vec2(1)},
            {0, 0, 0, 0}, 0, {0, 0, analytic_ellipse_fill_effect},
        });
        impl->queue_color_call();
    }

    void ContextImplGl::draw_text(float x, float y, std::string text)
    {
        std::u32string unicode_codepoints = detail::decode_utf8(text);
        draw_text(x, y, unicode_codepoints, Transform{});
    }

    void ContextImplGl::draw_text(float x, float y, std::string text,
                                      const Transform& transform)
    {
        std::u32string unicode_codepoints = detail::decode_utf8(text);
        draw_text(x, y, unicode_codepoints, transform);
    }

    void ContextImplGl::draw_text(float x, float y, std::u32string text)
    {
        draw_text(x, y, text, Transform{});
    }

    void ContextImplGl::draw_text(float x, float y, std::u32string text,
                                      const Transform& transform)
    {
        draw_text_impl(x, y, text, transform, false, nullptr);
    }

    void ContextImplGl::draw_text_alpha_mask(float x, float y, std::u32string text,
                                                 float erosion_radius,
                                                 const Transform& transform)
    {
        draw_text_impl(x, y, text, transform, true, nullptr, erosion_radius);
    }

    void ContextImplGl::draw_text_inset_alpha_mask(float x, float y,
                                                        std::u32string text,
                                                        float sample_offset_x,
                                                        float sample_offset_y,
                                                        float dilation_radius,
                                                        const Transform& transform)
    {
        const vec2 offset(sample_offset_x, sample_offset_y);
        draw_text_impl(x, y, text, transform, true, &offset, dilation_radius);
    }

    void ContextImplGl::draw_text_impl(float x, float y, const std::u32string& text,
                                           const Transform& transform, bool alpha_mask,
                                           const vec2* inset_sample_offset,
                                           float morphology_radius)
    {
        ContextImplGl* impl = getImpl(this);
        if (!std::isfinite(x) || !std::isfinite(y))
            throw std::invalid_argument("gCanvas affine text position must be finite");
        validate_transform(transform);

        if (_font == nullptr) {
            std::cerr << "Error: No font loaded!" << std::endl;
            return;
        }

        float initialX = x;
        float scale = (float)_font_size / LOADED_HEIGHT;
        const std::map<char32_t, character>& characters = _font->get_characters();
        auto* img = static_cast<ImageImplGl*>(&_font->get_image());
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
  
            impl->storage.push_back({
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
                //{(float)!(_font->is_color_font()), 0, 0},          // is_msdf
            });
            impl->queue_color_call();

            x += ch.advance * scale;
        }
    }


    void ContextImplGl::draw_image(float x, float y, float width, float height, Image& image,
                                       bool tint)
    {
        draw_rounded_image(x, y, width, height, image, 0, 0, 0, 0, tint);
    }

    void ContextImplGl::draw_image(float x, float y, float width, float height, Image& image,
                                       const Transform& transform, bool tint)
    {
        ContextImplGl* impl = getImpl(this);
        validate_resource(image);
        const ImageQuad quad = make_image_quad(x, y, width, height, transform);
        if (width == 0.0f || height == 0.0f)
            return;
        auto* img = static_cast<ImageImplGl*>(&image);

        impl->storage.push_back({
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
        impl->queue_color_call();
    }

    void ContextImplGl::draw_image_alpha_mask(float x, float y, float width, float height,
                                                   Image& image, float erosion_radius,
                                                   const Transform& transform)
    {
        ContextImplGl* impl = getImpl(this);
        validate_resource(image);
        const ImageQuad quad = make_image_quad(x, y, width, height, transform);
        if (width == 0.0f || height == 0.0f)
            return;
        auto* img = static_cast<ImageImplGl*>(&image);
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
        impl->storage.push_back({
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
        impl->queue_color_call();
    }

    void ContextImplGl::draw_image_inset_alpha_mask(
        float x, float y, float width, float height, Image& image, float sample_offset_x,
        float sample_offset_y, float dilation_radius, const Transform& transform)
    {
        ContextImplGl* impl = getImpl(this);
        validate_resource(image);
        const ImageQuad quad = make_image_quad(x, y, width, height, transform);
        if (width == 0.0f || height == 0.0f)
            return;
        auto* img = static_cast<ImageImplGl*>(&image);
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
        impl->storage.push_back({
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
        impl->queue_color_call();
    }

    void ContextImplGl::draw_image(float x, float y, float width, float height, Image& image, float src_x,
                             float src_y, float src_width, float src_height, bool tint)
    {
        draw_rounded_image(x, y, width, height, image, 0, 0, 0, 0, src_x, src_y, src_width,
                           src_height, tint);
    }

    void ContextImplGl::draw_rounded_image(float x, float y, float width, float height, Image& image,
                                     float border_radius, bool tint)
    {
        draw_rounded_image(x, y, width, height, image, border_radius, border_radius, border_radius,
                           border_radius, tint);
    }

    void ContextImplGl::draw_rounded_image(float x, float y, float width, float height, Image& image, float radius_nw, float radius_ne, float radius_se,
                                     float radius_sw, bool tint)
    {
        draw_rounded_image(x, y, width, height, image, radius_nw, radius_ne, radius_se, radius_sw,
                           0, 0, (float)image.get_width(), (float)image.get_height(), tint);
    }

    void ContextImplGl::draw_rounded_image(float x, float y, float width, float height, Image& image,
                                     float radius_nw, float radius_ne, float radius_se,
                                     float radius_sw, float src_x, float src_y, float src_width,
                                     float src_height, bool tint)
    {
        ContextImplGl* impl = getImpl(this);
        validate_resource(image);
        auto* img = static_cast<ImageImplGl*>(&image);

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

        impl->storage.push_back({
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
        impl->queue_color_call();
    }

    void ContextImplGl::draw_rect_shadow(float x, float y, float width, float height,
                                              float blur_radius)
    {
        ContextImplGl* impl = getImpl(this);
        const ShadowQuad shadow = make_shadow_quad(x, y, width, height, blur_radius);
        if (width == 0.0f || height == 0.0f)
            return;
        const ImageQuad& quad = shadow.geometry;

        impl->storage.push_back({
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
        impl->queue_color_call();
    }

    void ContextImplGl::draw_rounded_rect_shadow(float x, float y, float width, float height,
                                                       float border_radius, float blur_radius)
    {
        draw_rounded_rect_shadow(x, y, width, height, border_radius, border_radius, border_radius,
                                 border_radius, blur_radius);
    }

    void ContextImplGl::draw_rounded_rect_shadow(float x, float y, float width, float height,
                                                       float radius_nw, float radius_ne,
                                                       float radius_se, float radius_sw,
                                                       float blur_radius)
    {
        ContextImplGl* impl = getImpl(this);
        validate_shadow_radii(radius_nw, radius_ne, radius_se, radius_sw);
        const ShadowQuad shadow = make_shadow_quad(x, y, width, height, blur_radius);
        if (width == 0.0f || height == 0.0f)
            return;
        const ImageQuad& quad = shadow.geometry;

        float nwf = radius_nw * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float nef = radius_ne * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float sef = radius_se * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);
        float swf = radius_sw * (impl->_metrics.scale_x * impl->_metrics.dpi_scale);

        impl->storage.push_back({
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
        impl->queue_color_call();
    }

    void ContextImplGl::draw_circle_shadow(float x, float y, float radius,
                                                float blur_radius)
    {
        ContextImplGl* impl = getImpl(this);
        const ShadowQuad shadow =
            make_shadow_quad(x - radius, y - radius, radius * 2.0f, radius * 2.0f,
                             blur_radius);
        if (radius == 0.0f)
            return;
        const ImageQuad& quad = shadow.geometry;
        const float width = shadow.geometry.resolution.get_x();
        float radf = width / 2.0f;

        impl->storage.push_back({
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
        impl->queue_color_call();
    }

   
    void ContextImplGl::draw_inset_shadow(const InsetShadow& shadow)
    {
        ContextImplGl* impl = getImpl(this);
        const ImageQuad quad = make_image_quad(shadow.x, shadow.y, shadow.width, shadow.height,
                                               Transform{});
        const float metric_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        const float metric_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        const float metric_max = (std::max)(std::fabs(metric_x), std::fabs(metric_y));
        impl->storage.push_back({
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
        impl->queue_color_call();
    }

    void ContextImplGl::set_clear_color(color color)
    {
        _clear_color = color;
        glClearColor(_clear_color.rf(), _clear_color.gf(), _clear_color.bf(), _clear_color.af());

    }

    void ContextImplGl::set_rect_mask(float x, float y, float width, float height)
    {
        validate_rect_mask(x, y, width, height);
        ContextImplGl* impl = getImpl(this);
        const float scale_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        const float scale_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        const auto clamp_x = [&](float value) {
            return (std::min)(static_cast<float>(_width), (std::max)(0.0f, value));
        };
        const auto clamp_y = [&](float value) {
            return (std::min)(static_cast<float>(_height), (std::max)(0.0f, value));
        };
        const float left = clamp_x(x * scale_x);
        const float right = clamp_x((x + width) * scale_x);
        const float bottom = clamp_y(impl->_metrics.height - (y + height) * scale_y);
        const float top = clamp_y(impl->_metrics.height - y * scale_y);
        const GLint device_x = static_cast<GLint>(std::floor(left));
        const GLint device_y = static_cast<GLint>(std::floor(bottom));
        const GLint device_right = static_cast<GLint>(std::ceil((std::max)(left, right)));
        const GLint device_top = static_cast<GLint>(std::ceil((std::max)(bottom, top)));
        impl->current_type = ContextImplGl::SCISSOR;
        impl->scissor_primitives.push_back(
            {device_x, device_y, device_right - device_x, device_top - device_y});
        impl->draw_call_indices.push_back({(int)impl->storage.size(),
                                           (int)impl->scissor_primitives.size() - 1,
                                             ContextImplGl::SCISSOR});

    }

    void ContextImplGl::set_convex_mask(const std::vector<vec2>& vertices)
    {
        validate_convex_mask(vertices);
        ContextImplGl* impl = getImpl(this);
        const std::size_t triangle_count = vertices.size() >= 3U ? vertices.size() - 2U : 0U;
        if (triangle_count > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
            impl->storage.size() >
                static_cast<std::size_t>(std::numeric_limits<int>::max()) - triangle_count)
            throw std::length_error("OpenGL convex mask command index limit reached");

        const int first = static_cast<int>(impl->storage.size());
        const Transform device_transform =
            Transform::scaling(impl->_metrics.scale_x * impl->_metrics.dpi_scale,
                               impl->_metrics.scale_y * impl->_metrics.dpi_scale) *
            Transform::translation(impl->_metrics.offset_x, impl->_metrics.offset_y);
        for (std::size_t index = 1U; index + 1U < vertices.size(); ++index)
            impl->storage.push_back(mask_triangle_uniform(
                vertices.front(), vertices[index], vertices[index + 1U], device_transform,
                _width, _height));

        const int mask_index = static_cast<int>(impl->convex_mask_draws.size());
        impl->convex_mask_draws.push_back({first, static_cast<int>(triangle_count)});
        impl->draw_call_indices.push_back(
            {first, -1, ContextImplGl::CONVEX_MASK, mask_index});
        impl->current_type = ContextImplGl::CONVEX_MASK;
        impl->current_color_call_cnt = 0;
    }

    void ContextImplGl::remove_rect_mask()
    {
        ContextImplGl* impl = getImpl(this);

        impl->current_type = ContextImplGl::SCISSOR_CLEAR;
        impl->draw_call_indices.push_back({(int)impl->storage.size(),
                                           (int)impl->scissor_primitives.size() - 1,
                                           ContextImplGl::SCISSOR_CLEAR});
    }

    void ContextImplGl::draw_frame()
    {
        ContextImplGl* impl = getImpl(this);
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
        if (impl->storage.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::length_error("OpenGL draw storage index limit reached");
        impl->rendering = true;

        bool rerecord = false;
        if (impl->last_uniform_cnt != impl->storage.size())
        {
            impl->last_uniform_cnt = (int)impl->storage.size();
            rerecord = true;
        }

        glStencilMask(0xff);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glUseProgram(impl->shaderProgram);
        // impl->update_storage_buffer();
        //
        if (rerecord || impl->dirty)
        {

            impl->dirty = false;
        }

        for (int i = 0; i < impl->images.size(); ++i)
        {
            impl->images[i]->bind(i);
        }

        // std::cout << "------------- DRAW CALL START ------------- \n";
        bool convex_mask_active = false;
        int uploaded_path_first = -1;
        int uploaded_path_last_call = -1;
        for (int i = 0; i < impl->draw_call_indices.size(); ++i)
        {
            if (impl->draw_call_indices[i].type == ContextImplGl::draw_call_type::COLOR)
            {
                // std::cout << i + 1 << " CALL TYPE: COLOR\n";
                int batch_length = static_cast<int>(impl->storage.size()) -
                                   impl->draw_call_indices[i].instance_index;
                if (impl->draw_call_indices.size() > i + 1)
                {
                    batch_length = impl->draw_call_indices[i + 1].instance_index -
                                   impl->draw_call_indices[i].instance_index;
                }
                // std::cout << "\tSTART: " << impl->draw_call_indices[i].instance_index << "
                // batch_length: " << batch_length << "\n";

                // impl->update_uniform_buffer();

                draw_uniform_range(*impl, impl->draw_call_indices[i].instance_index,
                                   batch_length);
            }
            else if (impl->draw_call_indices[i].type == ContextImplGl::draw_call_type::PATH)
            {
                const ContextImplGl::path_draw& path =
                    impl->path_draws[impl->draw_call_indices[i].path_index];
                const int path_uniform_count = path.mask_count + 1;
                const bool path_fits_upload =
                    path_uniform_count <= impl->MAX_UNIFORM_RECT_PER_BLOCK_COUNT;
                if (path_fits_upload && i > uploaded_path_last_call)
                {
                    int upload_count = path_uniform_count;
                    uploaded_path_first = path.instance_index;
                    uploaded_path_last_call = i;
                    for (int next_call = i + 1;
                         next_call < static_cast<int>(impl->draw_call_indices.size());
                         ++next_call)
                    {
                        const ContextImplGl::draw_call_chain& next_chain =
                            impl->draw_call_indices[static_cast<std::size_t>(next_call)];
                        if (next_chain.type != ContextImplGl::draw_call_type::PATH)
                            break;
                        const ContextImplGl::path_draw& next_path =
                            impl->path_draws[static_cast<std::size_t>(next_chain.path_index)];
                        const int next_count = next_path.mask_count + 1;
                        if (next_path.instance_index != uploaded_path_first + upload_count ||
                            next_count > impl->MAX_UNIFORM_RECT_PER_BLOCK_COUNT - upload_count)
                            break;
                        upload_count += next_count;
                        uploaded_path_last_call = next_call;
                    }
                    upload_uniform_range(*impl, uploaded_path_first, upload_count);
                }
                glEnable(GL_STENCIL_TEST);
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                glStencilMask(path_stencil_bit);
                glStencilFunc(GL_ALWAYS, 1, path_stencil_bit);
                glStencilOp(GL_KEEP, GL_KEEP, path.even_odd ? GL_INVERT : GL_REPLACE);
                if (path_fits_upload)
                    draw_uploaded_uniform_range(*impl, path.instance_index - uploaded_path_first,
                                                path.mask_count);
                else
                    draw_uniform_range(*impl, path.instance_index, path.mask_count);

                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                glStencilMask(0x00);
                const GLuint stencil_mask =
                    path_stencil_bit | (convex_mask_active ? clip_stencil_bit : 0U);
                glStencilFunc(GL_EQUAL, static_cast<GLint>(stencil_mask), stencil_mask);
                glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                if (path_fits_upload)
                    draw_uploaded_uniform_range(
                        *impl, path.instance_index + path.mask_count - uploaded_path_first, 1);
                else
                    draw_uniform_range(*impl, path.instance_index + path.mask_count, 1);

                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                glStencilMask(path_stencil_bit);
                glClear(GL_STENCIL_BUFFER_BIT);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                if (convex_mask_active)
                {
                    glStencilMask(0x00);
                    glStencilFunc(GL_EQUAL, static_cast<GLint>(clip_stencil_bit),
                                  clip_stencil_bit);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                }
                else
                {
                    glStencilMask(0xff);
                    glDisable(GL_STENCIL_TEST);
                }
            }
            else if (impl->draw_call_indices[i].type ==
                     ContextImplGl::draw_call_type::PATH_ERODE)
            {
                const ContextImplGl::path_erode_draw& path =
                    impl->path_erode_draws[impl->draw_call_indices[i].path_index];
                glEnable(GL_STENCIL_TEST);
                int sample_instance = path.instance_index;
                for (int sample = 0; sample < path.sample_count; ++sample)
                {
                    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    glStencilMask(path_stencil_bit);
                    glStencilFunc(GL_ALWAYS, static_cast<GLint>(path_stencil_bit),
                                  path_stencil_bit);
                    glStencilOp(GL_KEEP, GL_KEEP, path.even_odd ? GL_INVERT : GL_REPLACE);
                    draw_uniform_range(*impl, sample_instance, path.base_mask_count);

                    glStencilFunc(GL_ALWAYS, 0, path_stencil_bit);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                    draw_uniform_range(*impl, sample_instance + path.base_mask_count,
                                       path.erosion_mask_count);

                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    glStencilMask(0x00);
                    const GLuint stencil_mask =
                        path_stencil_bit | (convex_mask_active ? clip_stencil_bit : 0U);
                    glStencilFunc(GL_EQUAL, static_cast<GLint>(stencil_mask), stencil_mask);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                    draw_uniform_range(*impl,
                                       sample_instance + path.base_mask_count +
                                           path.erosion_mask_count,
                                       1);

                    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    glStencilMask(path_stencil_bit);
                    glClear(GL_STENCIL_BUFFER_BIT);
                    sample_instance +=
                        path.base_mask_count + path.erosion_mask_count + 1;
                }
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                if (convex_mask_active)
                {
                    glStencilMask(0x00);
                    glStencilFunc(GL_EQUAL, static_cast<GLint>(clip_stencil_bit),
                                  clip_stencil_bit);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                }
                else
                {
                    glStencilMask(0xff);
                    glDisable(GL_STENCIL_TEST);
                }
            }
            else if (impl->draw_call_indices[i].type ==
                     ContextImplGl::draw_call_type::PATH_INSET)
            {
                const ContextImplGl::path_inset_draw& path =
                    impl->path_inset_draws[impl->draw_call_indices[i].path_index];
                glEnable(GL_STENCIL_TEST);
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                glStencilMask(path_stencil_bit);
                glStencilFunc(GL_ALWAYS, static_cast<GLint>(path_stencil_bit),
                              path_stencil_bit);
                glStencilOp(GL_KEEP, GL_KEEP,
                            path.original_even_odd ? GL_INVERT : GL_REPLACE);
                draw_uniform_range(*impl, path.instance_index, path.original_mask_count);

                int sample_instance = path.instance_index + path.original_mask_count;
                for (int sample = 0; sample < path.sample_count; ++sample)
                {
                    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    glStencilMask(shifted_path_stencil_bit);
                    glStencilFunc(GL_ALWAYS, static_cast<GLint>(shifted_path_stencil_bit),
                                  shifted_path_stencil_bit);
                    glStencilOp(GL_KEEP, GL_KEEP,
                                path.shifted_even_odd ? GL_INVERT : GL_REPLACE);
                    draw_uniform_range(*impl, sample_instance, path.shifted_base_count);
                    if (path.shifted_extra_count > 0)
                    {
                        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                        draw_uniform_range(*impl, sample_instance + path.shifted_base_count,
                                           path.shifted_extra_count);
                    }

                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    glStencilMask(0x00);
                    const GLuint compare_mask = path_stencil_bit |
                        shifted_path_stencil_bit |
                        (convex_mask_active ? clip_stencil_bit : 0U);
                    const GLuint reference = path_stencil_bit |
                        (convex_mask_active ? clip_stencil_bit : 0U);
                    glStencilFunc(GL_EQUAL, static_cast<GLint>(reference), compare_mask);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                    const int shifted_mask_count =
                        path.shifted_base_count + path.shifted_extra_count;
                    draw_uniform_range(*impl, sample_instance + shifted_mask_count, 1);

                    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    glStencilMask(shifted_path_stencil_bit);
                    glClear(GL_STENCIL_BUFFER_BIT);
                    sample_instance += shifted_mask_count + 1;
                }

                glStencilMask(path_stencil_bit | shifted_path_stencil_bit);
                glClear(GL_STENCIL_BUFFER_BIT);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                if (convex_mask_active)
                {
                    glStencilMask(0x00);
                    glStencilFunc(GL_EQUAL, static_cast<GLint>(clip_stencil_bit),
                                  clip_stencil_bit);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                }
                else
                {
                    glStencilMask(0xff);
                    glDisable(GL_STENCIL_TEST);
                }
            }
            else if (impl->draw_call_indices[i].type == ContextImplGl::draw_call_type::SCISSOR)
            {
                // std::cout << i + 1 << " CALL TYPE: SCISSOR\n";
                ContextImplGl::scissor_primitive sp =
                    impl->scissor_primitives[impl->draw_call_indices[i].scissor_index];
                glDisable(GL_STENCIL_TEST);
                glEnable(GL_SCISSOR_TEST);
                glScissor(sp.x, sp.y, sp.width, sp.height);
                convex_mask_active = false;
            }
            else if (impl->draw_call_indices[i].type ==
                     ContextImplGl::draw_call_type::CONVEX_MASK)
            {
                const ContextImplGl::convex_mask_draw& mask =
                    impl->convex_mask_draws[impl->draw_call_indices[i].path_index];
                glDisable(GL_SCISSOR_TEST);
                glEnable(GL_STENCIL_TEST);
                glStencilMask(clip_stencil_bit);
                glClear(GL_STENCIL_BUFFER_BIT);
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                glStencilFunc(GL_ALWAYS, static_cast<GLint>(clip_stencil_bit),
                              clip_stencil_bit);
                glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                draw_uniform_range(*impl, mask.instance_index, mask.triangle_count);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                glStencilMask(0x00);
                glStencilFunc(GL_EQUAL, static_cast<GLint>(clip_stencil_bit),
                              clip_stencil_bit);
                glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                convex_mask_active = true;
            }
            else if (impl->draw_call_indices[i].type ==
                     ContextImplGl::draw_call_type::SCISSOR_CLEAR)
            {
                // std::cout << i + 1 << " CALL TYPE: SCISSOR_CLEAR\n";
                glDisable(GL_SCISSOR_TEST);
                glDisable(GL_STENCIL_TEST);
                convex_mask_active = false;
            }
        }

        impl->clear_draw_queue();

        glFlush();
    }

    void ContextImplGl::present_frame()
    {
        ContextImplGl* impl = getImpl(this);
        if (!impl->rendering || impl->headless)
            return;

        //std::cout << " TOTAL COLOR UNITS: " << impl->storage.size() << "\n";

        if (impl->_presentation == detail::GlPresentationMode::HostManaged)
            impl->_host.swap_buffers(impl->_host.user_data);

        reset_transient_path_resources();
        impl->rendering = false;
    }

    std::vector<std::uint8_t> ContextImplGl::read_pixels()
    {
        int width = 0;
        int height = 0;
        _host.framebuffer_size(_host.user_data, &width, &height);
        if (width <= 0 || height <= 0)
        {
            throw std::runtime_error("cannot read pixels from an empty OpenGL framebuffer");
        }
        const auto pixel_count = static_cast<std::size_t>(width) *
                                 static_cast<std::size_t>(height);
        if (pixel_count > std::numeric_limits<std::size_t>::max() / 4U)
        {
            throw std::overflow_error("OpenGL readback size overflow");
        }
        std::vector<std::uint8_t> pixels(pixel_count * 4U);
        if (_host.make_current != nullptr)
            _host.make_current(_host.user_data);
        glFinish();
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    }

    void ContextImplGl::resize_context(int width, int height)
    {
        ContextImplGl* impl = getImpl(this);
        if (width < 0 || height < 0)
            throw std::invalid_argument("OpenGL framebuffer dimensions must be non-negative");
        impl->dirty = true;
        if (width == 0 || height == 0)
        {
            impl->headless = true;
        }
        else
        {
            int framebuffer_width = 0;
            int framebuffer_height = 0;
            _host.framebuffer_size(_host.user_data, &framebuffer_width, &framebuffer_height);
            impl->headless = framebuffer_width == 0 || framebuffer_height == 0;
            if (!impl->headless)
            {
                impl->_width = framebuffer_width;
                impl->_height = framebuffer_height;
                glViewport(0, 0, framebuffer_width, framebuffer_height);
            }
        }
    }

    void ContextImplGl::set_vsync(bool enabled)
    {
        if (_presentation == detail::GlPresentationMode::External)
            throw std::logic_error("external OpenGL presentation owns swap interval state");
        _host.make_current(_host.user_data);
        _host.set_swap_interval(_host.user_data, enabled ? 1 : 0);
    }

    Image& ContextImplGl::create_image(const std::string& file_path, ImageConfig config)
    {
        if (_owned_images.size() >= _resource_limits.max_images)
        {
            throw std::length_error("gCanvas image limit reached");
        }
        auto owned = std::make_unique<ImageImplGl>(file_path, config);
        Image& image = *owned;
        register_image(&image);
        return own_image(std::move(owned));
    }

    Image& ContextImplGl::create_image(int width, int height, int components,
                                           const unsigned char* data, std::size_t size,
                                           ImageConfig config)
    {
        if (_owned_images.size() >= _resource_limits.max_images)
        {
            throw std::length_error("gCanvas image limit reached");
        }
        auto owned =
            std::make_unique<ImageImplGl>(width, height, components, data, size, config);
        Image& image = *owned;
        register_image(&image);
        return own_image(std::move(owned));
    }

    Font& ContextImplGl::create_font(const std::string& file_path)
    {
        if (_owned_fonts.size() >= _resource_limits.max_fonts)
        {
            throw std::length_error("gCanvas font limit reached");
        }
        auto owned = std::make_unique<FontImplGl>(file_path);
        Font& font = *owned;
        register_font(&font);
        return own_font(std::move(owned));
    }

    Font& ContextImplGl::create_font(const unsigned char* buffer, std::size_t size)
    {
        if (_owned_fonts.size() >= _resource_limits.max_fonts)
        {
            throw std::length_error("gCanvas font limit reached");
        }
        auto owned = std::make_unique<FontImplGl>(buffer, size);
        Font& font = *owned;
        register_font(&font);
        return own_font(std::move(owned));
    }

    void ContextImplGl::register_image(Image* image)
    {
        ContextImplGl* impl = getImpl(this);
        ImageImplGl* img = (ImageImplGl*)image;
        if (!img->_uploaded)
        {
            if (impl->images.size() >= static_cast<std::size_t>(impl->texture_array_size))
            {
                throw std::length_error("OpenGL texture sampler limit reached");
            }
            else
            {
                img->upload();
                img->_sampler_index = (int)impl->images.size();
                impl->images.push_back(img);
                glUseProgram(impl->shaderProgram);
                const std::string location =
                    "textures[" + std::to_string(img->_sampler_index) + "]";
                glUniform1i(glGetUniformLocation(impl->shaderProgram, location.c_str()),
                            img->_sampler_index);
            }
        }
    }

    void ContextImplGl::register_font(Font* font)
    {
        ContextImplGl* impl = getImpl(this);
        FontImplGl* fiv = (FontImplGl*)font;
        if (!fiv->_uploaded)
        {
            auto* img = static_cast<ImageImplGl*>(&font->get_image());
            if (impl->images.size() >= static_cast<std::size_t>(impl->texture_array_size))
            {
                throw std::length_error("OpenGL font sampler limit reached");
            }
            else
            {
                fiv->upload();
                img->_sampler_index = (int)impl->images.size();
                impl->images.push_back(img);
                glUseProgram(impl->shaderProgram);
                const std::string location =
                    "textures[" + std::to_string(img->_sampler_index) + "]";
                glUniform1i(glGetUniformLocation(impl->shaderProgram, location.c_str()),
                            img->_sampler_index);
            }
        }
    }

    void ContextImplGl::update_image(Image* image)
    {
        ImageImplGl* img = (ImageImplGl*)image;   
        img->upload_update();
    }

    void ContextImplGl::prepare()
    {
        
        _default_font = &create_font(default_font.data(), default_font.size());
        if (_font == nullptr)
        {
            set_font(*_default_font);
        }
        
        ContextImplGl* impl = getImpl(this);
        
        impl->configure_surface();
        impl->create_vertex_array();
        //impl->create_storage_buffer();
        impl->create_uniform_buffer();


        glUseProgram(impl->shaderProgram);
        for (int i = 0; i < impl->images.size(); ++i)
        {
            std::string sloc = "textures[" + std::to_string(i) + "]";
            GLint loc = glGetUniformLocation(impl->shaderProgram, sloc.c_str());
            glUniform1i(loc, i);
        }
    }

    void ContextImplGl::draw_tessellated_path(const Path& path, const Paint& paint,
                                                   float line_width,
                                                   const Transform& transform)
    {
        ContextImplGl* impl = getImpl(this);
        const Transform device_transform =
            Transform::scaling(impl->_metrics.scale_x * impl->_metrics.dpi_scale,
                               impl->_metrics.scale_y * impl->_metrics.dpi_scale) *
            Transform::translation(impl->_metrics.offset_x, impl->_metrics.offset_y) * transform;
        detail::PathGeometry geometry =
            detail::tessellate_path(path, paint, device_transform, line_width,
                                    _resource_limits.max_path_mesh_quads,
                                    _resource_limits.path_paint_texture_size);
        if (geometry.mask_quads.empty())
            return;

        const std::size_t required = geometry.mask_quads.size() + 1U;
        if (required > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
            impl->storage.size() >
                static_cast<std::size_t>(std::numeric_limits<int>::max()) - required)
            throw std::length_error("OpenGL path command index limit reached");

        int sampler_index = -1;
        if (geometry.paint_image != nullptr)
            sampler_index = static_cast<ImageImplGl&>(*geometry.paint_image)._sampler_index;
        else if (geometry.raster_paint.type() != Paint::Type::None)
        {
            Image& texture = acquire_cached_path_paint_texture(
                geometry.raster_paint,
                vec2(geometry.raster_minimum.x, geometry.raster_minimum.y),
                vec2(geometry.raster_maximum.x, geometry.raster_maximum.y));
            sampler_index = static_cast<ImageImplGl&>(texture)._sampler_index;
        }
        else if (!geometry.paint_pixels.empty())
        {
            Image& texture = acquire_path_paint_texture(geometry.paint_pixels);
            sampler_index = static_cast<ImageImplGl&>(texture)._sampler_index;
        }
        const int first = static_cast<int>(impl->storage.size());
        for (const detail::MeshQuad& quad : geometry.mask_quads)
            impl->storage.push_back(path_uniform(quad, _width, _height,
                                                 color(255, 255, 255, 255)));
        impl->storage.push_back(path_uniform(geometry.paint_quad, _width, _height,
                                             geometry.solid_color, sampler_index,
                                             geometry.paint_uv_min, geometry.paint_uv_max));
        const int path_index = static_cast<int>(impl->path_draws.size());
        impl->path_draws.push_back(
            {first, static_cast<int>(geometry.mask_quads.size()),
             geometry.mask_mode == detail::PathMaskMode::EvenOdd});
        impl->draw_call_indices.push_back({first, -1, ContextImplGl::PATH, path_index});
        impl->current_type = ContextImplGl::PATH;
        impl->current_color_call_cnt = 0;
    }

    void ContextImplGl::draw_tessellated_path_blur(
        const Path& path, const Paint& paint, float line_width,
        const ShadowKernel& kernel, float source_alpha, const Transform& transform)
    {
        ContextImplGl* impl = getImpl(this);
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

        const std::size_t per_sample = geometry.mask_quads.size() + 1U;
        if (kernel.count > (std::numeric_limits<std::size_t>::max)() / per_sample)
            throw std::overflow_error("OpenGL blur path command size overflow");
        const std::size_t required = kernel.count * per_sample;
        if (required > _resource_limits.max_blur_commands)
            throw std::length_error("OpenGL blur path command limit reached");
        if (required > static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
            impl->storage.size() >
                static_cast<std::size_t>((std::numeric_limits<int>::max)()) - required)
            throw std::length_error("OpenGL blur path command index limit reached");
        const std::size_t index_capacity =
            static_cast<std::size_t>((std::numeric_limits<int>::max)());
        reserve_geometric(impl->storage, impl->storage.size() + required, index_capacity);
        reserve_geometric(impl->path_draws, impl->path_draws.size() + kernel.count,
                          index_capacity);
        reserve_geometric(impl->draw_call_indices,
                          impl->draw_call_indices.size() + kernel.count,
                          index_capacity);

        int sampler_index = -1;
        if (geometry.paint_image != nullptr)
            sampler_index = static_cast<ImageImplGl&>(*geometry.paint_image)._sampler_index;
        else if (geometry.raster_paint.type() != Paint::Type::None)
        {
            Image& texture = acquire_cached_path_paint_texture(
                geometry.raster_paint,
                vec2(geometry.raster_minimum.x, geometry.raster_minimum.y),
                vec2(geometry.raster_maximum.x, geometry.raster_maximum.y));
            sampler_index = static_cast<ImageImplGl&>(texture)._sampler_index;
        }
        else if (!geometry.paint_pixels.empty())
        {
            Image& texture = acquire_path_paint_texture(geometry.paint_pixels);
            sampler_index = static_cast<ImageImplGl&>(texture)._sampler_index;
        }

        const bool solid = sampler_index < 0;
        const float metric_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        const float metric_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        const float ndc_scale_x = 2.0f / static_cast<float>(_width);
        const float ndc_scale_y = 2.0f / static_cast<float>(_height);
        // The first sample is the frame-owned template. Later samples remain O(quads) but
        // reuse its normalized uniforms instead of repeating mesh-to-NDC conversion.
        const int template_first = static_cast<int>(impl->storage.size());
        float template_offset_x = 0.0f;
        float template_offset_y = 0.0f;
        for (std::size_t sample_index = 0; sample_index < kernel.count; ++sample_index)
        {
            const ShadowSample& sample = kernel.samples[sample_index];
            const float ndc_offset_x = sample.x * metric_x * ndc_scale_x;
            const float ndc_offset_y = sample.y * metric_y * ndc_scale_y;
            const int first = static_cast<int>(impl->storage.size());
            if (sample_index == 0U)
            {
                template_offset_x = ndc_offset_x;
                template_offset_y = ndc_offset_y;
                for (const detail::MeshQuad& quad : geometry.mask_quads)
                {
                    uniform_rect uniform =
                        path_uniform(quad, _width, _height, color(255, 255, 255, 255));
                    shift_uniform_vertices(uniform, ndc_offset_x, ndc_offset_y);
                    impl->storage.push_back(uniform);
                }
            }
            else
            {
                const float shift_x = ndc_offset_x - template_offset_x;
                const float shift_y = ndc_offset_y - template_offset_y;
                for (std::size_t mask_index = 0; mask_index < geometry.mask_quads.size();
                     ++mask_index)
                {
                    uniform_rect uniform =
                        impl->storage[static_cast<std::size_t>(template_first) + mask_index];
                    shift_uniform_vertices(uniform, shift_x, shift_y);
                    impl->storage.push_back(uniform);
                }
            }

            const float multiplier = source_alpha > 0.0f
                ? (std::min)(1.0f, sample.alpha / source_alpha)
                : 0.0f;
            const color paint_color = solid
                ? color(geometry.solid_color.rf(), geometry.solid_color.gf(),
                        geometry.solid_color.bf(), sample.alpha)
                : color(1.0f, 1.0f, 1.0f, multiplier);
            uniform_rect paint_uniform = sample_index == 0U
                ? path_uniform(geometry.paint_quad, _width, _height, paint_color,
                               sampler_index, geometry.paint_uv_min, geometry.paint_uv_max)
                : impl->storage[static_cast<std::size_t>(template_first) +
                                geometry.mask_quads.size()];
            shift_uniform_vertices(paint_uniform,
                                   sample_index == 0U
                                       ? ndc_offset_x
                                       : ndc_offset_x - template_offset_x,
                                   sample_index == 0U
                                       ? ndc_offset_y
                                       : ndc_offset_y - template_offset_y);
            paint_uniform.color = paint_color;
            impl->storage.push_back(paint_uniform);
            const int path_index = static_cast<int>(impl->path_draws.size());
            impl->path_draws.push_back(
                {first, static_cast<int>(geometry.mask_quads.size()),
                 geometry.mask_mode == detail::PathMaskMode::EvenOdd});
            impl->draw_call_indices.push_back(
                {first, -1, ContextImplGl::PATH, path_index});
        }
        impl->current_type = ContextImplGl::PATH;
        impl->current_color_call_cnt = 0;
    }

    void ContextImplGl::draw_tessellated_path_inset_shadow(
        const Path& path, float line_width, float offset_x, float offset_y,
        const ShadowKernel& kernel, float dilation_radius, const Transform& transform)
    {
        ContextImplGl* impl = getImpl(this);
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
        const std::size_t shifted_mask_count = shifted_base_count + shifted_extra_count;
        const std::size_t per_sample = shifted_mask_count + 1U;
        if (kernel.count > (std::numeric_limits<std::size_t>::max() - mask_count) /
                               per_sample)
            throw std::overflow_error("OpenGL inset path command size overflow");
        const std::size_t required = mask_count + kernel.count * per_sample;
        if (required > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
            impl->storage.size() >
                static_cast<std::size_t>(std::numeric_limits<int>::max()) - required)
            throw std::length_error("OpenGL inset path command index limit reached");

        const std::size_t index_capacity =
            static_cast<std::size_t>((std::numeric_limits<int>::max)());
        reserve_geometric(impl->storage, impl->storage.size() + required, index_capacity);

        const int first = static_cast<int>(impl->storage.size());
        for (const detail::MeshQuad& quad : geometry.mask_quads)
            impl->storage.push_back(path_uniform(quad, _width, _height,
                                                 color(255, 255, 255, 255)));

        const color base = _fill_color;
        const float metric_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        const float metric_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        for (std::size_t sample_index = 0; sample_index < kernel.count; ++sample_index)
        {
            const ShadowSample& sample = kernel.samples[sample_index];
            const float shift_x = (offset_x + sample.x) * metric_x;
            const float shift_y = (offset_y + sample.y) * metric_y;
            const std::vector<detail::MeshQuad>& shifted_base = replace_shifted_base
                ? dilation_geometry.mask_quads
                : geometry.mask_quads;
            for (const detail::MeshQuad& source : shifted_base)
            {
                detail::MeshQuad shifted = source;
                for (detail::MeshPoint& point : shifted.vertices)
                {
                    point.x += shift_x;
                    point.y += shift_y;
                }
                impl->storage.push_back(path_uniform(shifted, _width, _height,
                                                     color(255, 255, 255, 255)));
            }
            if (shifted_extra_count > 0U)
            {
                for (const detail::MeshQuad& source : dilation_geometry.mask_quads)
                {
                    detail::MeshQuad shifted = source;
                    for (detail::MeshPoint& point : shifted.vertices)
                    {
                        point.x += shift_x;
                        point.y += shift_y;
                    }
                    impl->storage.push_back(path_uniform(
                        shifted, _width, _height, color(255, 255, 255, 255)));
                }
            }
            impl->storage.push_back(path_uniform(
                geometry.paint_quad, _width, _height,
                color(base.rf(), base.gf(), base.bf(), sample.alpha)));
        }

        const int inset_index = static_cast<int>(impl->path_inset_draws.size());
        impl->path_inset_draws.push_back(
            {first, static_cast<int>(mask_count), static_cast<int>(shifted_base_count),
             static_cast<int>(shifted_extra_count), static_cast<int>(kernel.count),
             geometry.mask_mode == detail::PathMaskMode::EvenOdd,
             !replace_shifted_base && geometry.mask_mode == detail::PathMaskMode::EvenOdd});
        impl->draw_call_indices.push_back(
            {first, -1, ContextImplGl::PATH_INSET, inset_index});
        impl->current_type = ContextImplGl::PATH_INSET;
        impl->current_color_call_cnt = 0;
    }

    void ContextImplGl::draw_tessellated_path_eroded_shadow(
        const Path& path, float erosion_radius, const ShadowKernel& kernel,
        const Transform& transform)
    {
        ContextImplGl* impl = getImpl(this);
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

        const std::size_t per_sample =
            base.mask_quads.size() + boundary.mask_quads.size() + 1U;
        if (kernel.count > std::numeric_limits<std::size_t>::max() / per_sample)
            throw std::overflow_error("OpenGL eroded path command size overflow");
        const std::size_t required = kernel.count * per_sample;
        if (required > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
            impl->storage.size() >
                static_cast<std::size_t>(std::numeric_limits<int>::max()) - required)
            throw std::length_error("OpenGL eroded path command index limit reached");

        const std::size_t index_capacity =
            static_cast<std::size_t>((std::numeric_limits<int>::max)());
        reserve_geometric(impl->storage, impl->storage.size() + required, index_capacity);

        const int first = static_cast<int>(impl->storage.size());
        const color base_color = _fill_color;
        const float metric_x = impl->_metrics.scale_x * impl->_metrics.dpi_scale;
        const float metric_y = impl->_metrics.scale_y * impl->_metrics.dpi_scale;
        const float ndc_scale_x = 2.0f / static_cast<float>(_width);
        const float ndc_scale_y = 2.0f / static_cast<float>(_height);
        float template_offset_x = 0.0f;
        float template_offset_y = 0.0f;
        for (std::size_t sample_index = 0; sample_index < kernel.count; ++sample_index)
        {
            const ShadowSample& sample = kernel.samples[sample_index];
            const float ndc_offset_x = sample.x * metric_x * ndc_scale_x;
            const float ndc_offset_y = sample.y * metric_y * ndc_scale_y;
            if (sample_index == 0U)
            {
                template_offset_x = ndc_offset_x;
                template_offset_y = ndc_offset_y;
                const auto append_uniform = [&](const detail::MeshQuad& source) {
                    uniform_rect uniform = path_uniform(
                        source, _width, _height, color(255, 255, 255, 255));
                    shift_uniform_vertices(uniform, ndc_offset_x, ndc_offset_y);
                    impl->storage.push_back(uniform);
                };
                for (const detail::MeshQuad& quad : base.mask_quads)
                    append_uniform(quad);
                for (const detail::MeshQuad& quad : boundary.mask_quads)
                    append_uniform(quad);
            }
            else
            {
                const float shift_x = ndc_offset_x - template_offset_x;
                const float shift_y = ndc_offset_y - template_offset_y;
                const std::size_t mask_count =
                    base.mask_quads.size() + boundary.mask_quads.size();
                for (std::size_t mask_index = 0; mask_index < mask_count; ++mask_index)
                {
                    uniform_rect uniform =
                        impl->storage[static_cast<std::size_t>(first) + mask_index];
                    shift_uniform_vertices(uniform, shift_x, shift_y);
                    impl->storage.push_back(uniform);
                }
            }
            const std::size_t paint_template_index =
                static_cast<std::size_t>(first) + base.mask_quads.size() +
                boundary.mask_quads.size();
            uniform_rect paint_uniform = sample_index == 0U
                ? path_uniform(base.paint_quad, _width, _height,
                               color(base_color.rf(), base_color.gf(), base_color.bf(),
                                     sample.alpha))
                : impl->storage[paint_template_index];
            shift_uniform_vertices(
                paint_uniform,
                sample_index == 0U ? ndc_offset_x : ndc_offset_x - template_offset_x,
                sample_index == 0U ? ndc_offset_y : ndc_offset_y - template_offset_y);
            paint_uniform.color = color(base_color.rf(), base_color.gf(), base_color.bf(),
                                        sample.alpha);
            impl->storage.push_back(paint_uniform);
        }
        const int path_index = static_cast<int>(impl->path_erode_draws.size());
        impl->path_erode_draws.push_back(
            {first, static_cast<int>(base.mask_quads.size()),
             static_cast<int>(boundary.mask_quads.size()), static_cast<int>(kernel.count),
             base.mask_mode == detail::PathMaskMode::EvenOdd});
        impl->draw_call_indices.push_back(
            {first, -1, ContextImplGl::PATH_ERODE, path_index});
        impl->current_type = ContextImplGl::PATH_ERODE;
        impl->current_color_call_cnt = 0;
    }

    void ContextImplGl::queue_color_call()
    {
        // Start new color sequence
        if (current_type != COLOR || current_color_call_cnt >= MAX_UNIFORM_RECT_PER_BLOCK_COUNT)
        {
            draw_call_indices.push_back({(int)storage.size() - 1, -1, ContextImplGl::COLOR});
            current_color_call_cnt = 1;
            current_type = COLOR;
        }
        else
        {
            current_color_call_cnt++;
        } 
    }

    void ContextImplGl::clear_draw_queue() noexcept
    {
        storage.clear();
        draw_call_indices.clear();
        path_draws.clear();
        path_erode_draws.clear();
        path_inset_draws.clear();
        convex_mask_draws.clear();
        scissor_primitives.clear();
        current_color_call_cnt = 0;
        current_type = ContextImplGl::UNSET;
    }

    /* ------------------------ PRIVATE IMPLEMENTATION ------------------------ */

    

    void ContextImplGl::configure_surface()
    {
        int width, height;
        _host.framebuffer_size(_host.user_data, &width, &height);
        glViewport(0, 0, width, height);

        //glViewport(0, 0, _width, _height);
        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        GLint stencil_bits = 0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL,
                                              GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,
                                              &stencil_bits);
        if (stencil_bits < 3)
            throw std::runtime_error(
                "OpenGL gCanvas path effects require at least three stencil bits");
        glClearStencil(0);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    }

    void ContextImplGl::create_uniform_buffer()
    {
        glGenBuffers(1, &storageBuffer);
        glBindBuffer(GL_UNIFORM_BUFFER, storageBuffer);
        glBufferData(GL_UNIFORM_BUFFER,
                     MAX_UNIFORM_RECT_PER_BLOCK_COUNT * sizeof(uniform_rect), storage.data(),
                     GL_DYNAMIC_COPY);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, storageBuffer);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    /*
    void ContextImplGl::create_storage_buffer()
    {
        glGenBuffers(1, &storageBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, storageBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, UNIFORM_RECT_BUFFER_ARRAY_MAX_SIZE, storage.data(),
                     GL_DYNAMIC_COPY);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, storageBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    */

    void ContextImplGl::initialize_resources()
    {
        const std::uint8_t buffer[4] = {0, 0, 0, 0};
        dummy =
            std::make_unique<ImageImplGl>(1, 1, 4, buffer, sizeof(buffer), ImageConfig{});
        dummy->upload();
    }

    void ContextImplGl::update_viewport(uint32_t width, uint32_t height)
    {
        if (resizing)
            return;
        resizing = true;

        this->_width = width;
        this->_height = height;

        glViewport(0, 0, width, height);

        resizing = false;
    }


    ContextImplGl::ContextImplGl(const detail::GlCreateInfo& create_info)
        : Context(create_info.metrics, create_info.resource_limits), _host(create_info.host),
          _presentation(create_info.presentation), _backend(create_info.backend),
          _runtime(create_info.runtime), _vertex_shader_source(create_info.vertex_shader_source),
          _fragment_shader_source(create_info.fragment_shader_source)
    {
        if (_runtime == nullptr || _runtime->retain == nullptr || _runtime->release == nullptr ||
            _runtime->max_uniform_block_size == nullptr)
            throw std::invalid_argument("GL runtime profile is incomplete");
        if (_vertex_shader_source == nullptr || _fragment_shader_source == nullptr)
            throw std::invalid_argument("GL shader profile is incomplete");
        const bool common_callbacks_missing =
            _host.framebuffer_size == nullptr ||
            (_runtime->requires_proc_loader && _host.get_proc_address == nullptr);
        const bool managed_callbacks_missing =
            _presentation == detail::GlPresentationMode::HostManaged &&
            (_host.make_current == nullptr || _host.swap_buffers == nullptr ||
             _host.set_swap_interval == nullptr);
        if (common_callbacks_missing || managed_callbacks_missing)
        {
            throw std::invalid_argument(
                "OpenGL host callbacks do not satisfy the selected presentation mode");
        }
        if (_host.make_current != nullptr)
            _host.make_current(_host.user_data);
        _runtime->retain(_host.get_proc_address, _host.user_data);
        shared_retained_ = true;
        try
        {
            const int uniform_block_size = _runtime->max_uniform_block_size();
            if (uniform_block_size < static_cast<int>(sizeof(uniform_rect)))
                throw std::runtime_error("GL uniform block capacity is too small");
            MAX_UNIFORM_RECT_PER_BLOCK_COUNT =
                std::min(shader_batch_capacity,
                         uniform_block_size / static_cast<int>(sizeof(uniform_rect)));
            create_shader_programm();
            initialize_resources();
            prepare();
        }
        catch (...)
        {
            cleanup();
            throw;
        }
    }

    ContextImplGl::~ContextImplGl()
    {
        cleanup();
    }

    void ContextImplGl::cleanup() noexcept
    {
        if (!shared_retained_)
        {
            return;
        }
        if (_host.make_current != nullptr)
            _host.make_current(_host.user_data);
        release_owned_resources();
        dummy.reset();
        if (vertex_array_object != 0)
            glDeleteVertexArrays(1, &vertex_array_object);
        if (vertex_buffer != 0)
            glDeleteBuffers(1, &vertex_buffer);
        if (index_buffer != 0)
            glDeleteBuffers(1, &index_buffer);
        if (storageBuffer != 0)
            glDeleteBuffers(1, &storageBuffer);
        if (shaderProgram != 0)
            glDeleteProgram(shaderProgram);
        shared_retained_ = false;
        _runtime->release();
    }


    void ContextImplGl::create_vertex_array()
    {
        glGenVertexArrays(1, &vertex_array_object);
        glGenBuffers(1, &vertex_buffer);
        glGenBuffers(1, &index_buffer);
 
        glBindVertexArray(vertex_array_object);

        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(rect_vertices[0]) * rect_vertices.size(),
                     rect_vertices.data(),
                     GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rect_indices[0]) * rect_indices.size(),
                     rect_indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), (void*)0);
        glEnableVertexAttribArray(0);


        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0); 
    }

    void ContextImplGl::create_shader_programm()
    {
        int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &_vertex_shader_source, NULL);
        glCompileShader(vertexShader);
        // check for shader compile errors
        int success;
        char infoLog[512]{};
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            glDeleteShader(vertexShader);
            throw std::runtime_error(std::string("OpenGL vertex shader compilation failed: ") +
                                     infoLog);
        }
        // fragment shader
        int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &_fragment_shader_source, NULL);
        glCompileShader(fragmentShader);
        // check for shader compile errors
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            throw std::runtime_error(std::string("OpenGL fragment shader compilation failed: ") +
                                     infoLog);
        }
        // link shaders
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        // check for linking errors
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
            throw std::runtime_error(std::string("OpenGL shader link failed: ") + infoLog);
        }

        glValidateProgram(shaderProgram);

        // check for validation errors
        glGetProgramiv(shaderProgram, GL_VALIDATE_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
            throw std::runtime_error(std::string("OpenGL shader validation failed: ") + infoLog);
        }

        instance_offset_location = glGetUniformLocation(shaderProgram, "instance_offset");
        if (instance_offset_location < 0)
        {
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
            throw std::runtime_error("OpenGL instance offset uniform is unavailable");
        }
        
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    void ContextImplGl::update_uniform_buffer()
    {
        glBindBuffer(GL_UNIFORM_BUFFER, storageBuffer);
        GLsizei size = (GLsizei)std::min(storage.size() * sizeof(uniform_rect),
                                         static_cast<size_t>(MAX_UNIFORM_RECT_PER_BLOCK_COUNT) *
                                             sizeof(uniform_rect));
                                         
        //GLsizei size = (GLsizei)(storage.size() * sizeof(uniform_rect));

        glBufferSubData(GL_UNIFORM_BUFFER, 0, size, storage.data());
    }

    /*
    void ContextImplGl::update_storage_buffer()
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, storageBuffer);
        GLsizei size = (GLsizei)std::min(storage.size() * sizeof(uniform_rect),
                                         UNIFORM_RECT_BUFFER_ARRAY_MAX_SIZE);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, size, storage.data());
    }
    */

} // namespace GCANVAS_GL_PROFILE_NAMESPACE
} // namespace gcanvas 
