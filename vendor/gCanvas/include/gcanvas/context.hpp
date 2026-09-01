#ifndef GCANVAS_CONTEXT_HPP
#define GCANVAS_CONTEXT_HPP

#include <array>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "gcanvas/color.hpp"
#include "gcanvas/gcanvas.hpp"
#include "gcanvas/font.hpp"
#include "gcanvas/image.hpp"
#include "gcanvas/paint.hpp"
#include "gcanvas/path.hpp"
#include "gcanvas/transform.hpp"
#include "gcanvas/vec2.hpp"

namespace gcanvas
{
    enum class Backend
    {
        OpenGL,
        Vulkan
    };

    struct CanvasMetrics
    {
        int width = 1;
        int height = 1;
        float scale_x = 1.0f;
        float scale_y = 1.0f;
        float offset_x = 0.0f;
        float offset_y = 0.0f;
        float dpi_scale = 1.0f;
    };

    struct ResourceLimits
    {
        std::size_t max_images = 256;
        std::size_t max_fonts = 32;
        // Retained for source compatibility; now bounds reusable non-solid paint textures.
        std::size_t max_path_surfaces = 16;
        // Legacy full-surface limit retained for aggregate-initializer compatibility.
        std::size_t max_path_surface_pixels = 4096U * 4096U;
        std::size_t max_path_mesh_quads = 262144;
        std::size_t path_paint_texture_size = 256;
        std::size_t max_clip_vertices = 64;
        std::size_t max_shadow_samples = 25;
        // Bounds source-color Gaussian blur commands independently from shadow sampling.
        std::size_t max_blur_samples = 25;
        // Per blur call, including path mask and paint instances across every sample.
        std::size_t max_blur_commands = 65536;
    };

    class GCANVAS_API Context
    {
    public:
        virtual ~Context();

        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;

        virtual Backend backend() const noexcept = 0;

        virtual void stroke_rect(float x, float y, float width, float height) = 0;
        virtual void stroke_rounded_rect(float x, float y, float width, float height,
                                         float border_radius) = 0;
        virtual void stroke_rounded_rect(float x, float y, float width, float height,
                                         float radius_nw, float radius_ne, float radius_se,
                                         float radius_sw) = 0;
        virtual void stroke_circle(float x, float y, float radius) = 0;
        virtual void stroke_ellipse(float x, float y, float radius_x, float radius_y) = 0;

        virtual void fill_rect(float x, float y, float width, float height) = 0;
        virtual void fill_rounded_rect(float x, float y, float width, float height,
                                       float border_radius) = 0;
        virtual void fill_rounded_rect(float x, float y, float width, float height,
                                       float radius_nw, float radius_ne, float radius_se,
                                       float radius_sw) = 0;
        virtual void fill_circle(float x, float y, float radius) = 0;
        virtual void fill_ellipse(float x, float y, float radius_x, float radius_y) = 0;

        // Path and paint objects are caller-owned immutable values for the duration of this call.
        // Image patterns borrow an image owned by this context. Non-solid paints use bounded,
        // reusable lookup textures; path geometry is rendered through the backend stencil buffer.
        void fill_path(const Path& path, const Paint& paint,
                       const Transform& transform = {});
        void stroke_path(const Path& path, const Paint& paint, float line_width,
                         const Transform& transform = {});

        /**
         * Draws a bounded sampled Gaussian blur of a filled or stroked path.
         * The source paint remains the color source; every tap is a GPU path command and
         * no framebuffer or CPU readback is allocated. A zero line width selects fill coverage.
         * @param path Caller-owned path retained only for this call.
         * @param paint Source paint, including its alpha.
         * @param line_width Zero selects fill coverage; a positive value selects stroke coverage.
         * @param blur_radius Non-negative blur radius in logical canvas units.
         * @param transform Affine transform applied before sampling offsets.
         * @throws std::invalid_argument for invalid paint, width, radius, or transform.
         * @throws std::length_error when configured geometry or blur-sample limits are exceeded.
         */
        void draw_path_blur(const Path& path, const Paint& paint, float line_width,
                            float blur_radius, const Transform& transform = {});
        /**
         * Draws source-colored text through the bounded Gaussian blur kernel.
         * @param x Text origin x coordinate.
         * @param y Text origin y coordinate.
         * @param text UTF-8 text copied during the call.
         * @param blur_radius Non-negative blur radius in logical canvas units.
         * @param transform Affine transform applied before sampling offsets.
         * @throws std::invalid_argument for invalid positions, radius, or transform.
         * @throws std::range_error for malformed UTF-8 input.
         * @throws std::length_error when the configured blur-sample limit is exceeded.
         */
        void draw_text_blur(float x, float y, std::string text, float blur_radius,
                            const Transform& transform = {});
        /**
         * Draws an image through the bounded Gaussian blur kernel. The current fill alpha is
         * the source opacity; fill RGB does not tint the image.
         * @param x Destination x coordinate.
         * @param y Destination y coordinate.
         * @param width Non-negative destination width.
         * @param height Non-negative destination height.
         * @param image Image owned by this context.
         * @param blur_radius Non-negative blur radius in logical canvas units.
         * @param transform Affine transform applied before sampling offsets.
         * @throws std::invalid_argument for invalid geometry, ownership, radius, or transform.
         * @throws std::length_error when the configured blur-sample limit is exceeded.
         */
        void draw_image_blur(float x, float y, float width, float height, Image& image,
                             float blur_radius, const Transform& transform = {});

        /**
         * Draws a sampled outer shadow for an arbitrary path using the current fill color.
         * A zero line width shadows the filled path; a positive line width shadows its stroke.
         * Path tessellation and all samples remain backend GPU draw commands. Signed spread
         * expands or contracts the sampled mask; an over-contracted stroke produces no draw.
         */
        void draw_path_shadow(const Path& path, float line_width, float blur_radius,
                              float spread_radius, const Transform& transform = {});
        /** Draws a sampled glyph-alpha outer shadow with signed spread using the current color. */
        void draw_text_shadow(float x, float y, std::string text, float blur_radius,
                              float spread_radius, const Transform& transform = {});
        /** Draws a sampled image-alpha outer shadow with signed spread using the current color. */
        void draw_image_shadow(float x, float y, float width, float height, Image& image,
                               float blur_radius, float spread_radius,
                               const Transform& transform = {});
        /**
         * Draws a sampled inset shadow clipped to an arbitrary filled or stroked path.
         * @param path Caller-owned path retained only for this call.
         * @param line_width Zero selects fill coverage; a positive value selects stroke coverage.
         * @param offset_x Horizontal shadow offset in logical canvas units.
         * @param offset_y Vertical shadow offset in logical canvas units.
         * @param blur_radius Non-negative sampled blur radius.
         * @param spread_radius Signed sampled spread radius. Positive values expand the
         * shadow mask; negative values contract it.
         * @param transform Affine transform applied to the source path before inset sampling.
         * @throws std::invalid_argument for invalid values or transforms.
         * @throws std::length_error when configured draw or shadow limits are exceeded.
         * @note Uses the current fill color and must be submitted after the source content.
         */
        void draw_path_inset_shadow(const Path& path, float line_width, float offset_x,
                                    float offset_y, float blur_radius, float spread_radius,
                                    const Transform& transform = {});
        /**
         * Draws a sampled inset shadow clipped by glyph alpha using the current font and color.
         * @param x Text origin x coordinate.
         * @param y Text origin y coordinate.
         * @param text UTF-8 text copied during the call.
         * @param offset_x Horizontal shadow offset in logical canvas units.
         * @param offset_y Vertical shadow offset in logical canvas units.
         * @param blur_radius Non-negative sampled blur radius.
         * @param spread_radius Signed sampled spread radius. Positive values expand the
         * shadow mask; negative values contract it.
         * @param transform Affine transform applied to the glyph quads.
         * @throws std::invalid_argument for invalid values or transforms.
         * @throws std::range_error for malformed UTF-8 input.
         * @throws std::length_error when the configured shadow limit is exceeded.
         * @note Submit after draw_text() so the inset color overlays the glyph interior.
         */
        void draw_text_inset_shadow(float x, float y, std::string text, float offset_x,
                                    float offset_y, float blur_radius, float spread_radius,
                                    const Transform& transform = {});
        /**
         * Draws a sampled inset shadow clipped by image alpha using the current fill color.
         * @param x Destination x coordinate.
         * @param y Destination y coordinate.
         * @param width Non-negative destination width.
         * @param height Non-negative destination height.
         * @param image Image owned by this context.
         * @param offset_x Horizontal shadow offset in logical canvas units.
         * @param offset_y Vertical shadow offset in logical canvas units.
         * @param blur_radius Non-negative sampled blur radius.
         * @param spread_radius Signed sampled spread radius. Positive values expand the
         * shadow mask; negative values contract it.
         * @param transform Affine transform applied to the image quad.
         * @throws std::invalid_argument for invalid geometry, values, transforms, or ownership.
         * @throws std::length_error when the configured shadow limit is exceeded.
         * @note Submit after draw_image() so the inset color overlays the image interior.
         */
        void draw_image_inset_shadow(float x, float y, float width, float height, Image& image,
                                     float offset_x, float offset_y, float blur_radius,
                                     float spread_radius, const Transform& transform = {});

        virtual void draw_text(float x, float y, std::string text) = 0;
        virtual void draw_text(float x, float y, std::u32string text) = 0;
        virtual void draw_text(float x, float y, std::string text,
                               const Transform& transform);
        virtual void draw_text(float x, float y, std::u32string text,
                               const Transform& transform);
        virtual void draw_image(float x, float y, float width, float height, Image& image,
                                bool tint = false) = 0;
        // Draws the complete image through an affine transform. The transform is applied before
        // canvas metrics; image ownership and tint semantics match the axis-aligned overload.
        virtual void draw_image(float x, float y, float width, float height, Image& image,
                                const Transform& transform, bool tint = false);
        virtual void draw_image(float x, float y, float width, float height, Image& image,
                                float src_x, float src_y, float src_width, float src_height,
                                bool tint = false) = 0;
        virtual void draw_rounded_image(float x, float y, float width, float height, Image& image,
                                        float border_radius, bool tint = false) = 0;
        virtual void draw_rounded_image(float x, float y, float width, float height, Image& image,
                                        float radius_nw, float radius_ne, float radius_se,
                                        float radius_sw, bool tint = false) = 0;
        virtual void draw_rounded_image(float x, float y, float width, float height, Image& image,
                                        float radius_nw, float radius_ne, float radius_se,
                                        float radius_sw, float src_x, float src_y, float src_width,
                                        float src_height, bool tint = false) = 0;

        /**
         * Draws an outer shadow for the supplied primitive using the current fill color.
         * The primitive bounds describe the fully opaque interior; blur_radius extends
         * outwards from those bounds and is expressed in logical canvas units.
         * @param x Circle center on the logical x axis.
         * @param y Circle center on the logical y axis.
         * @param radius Non-negative logical circle radius.
         * @param blur_radius Non-negative outward falloff distance.
         * @throws std::invalid_argument if any value is invalid or the bounds overflow.
         */
        virtual void draw_circle_shadow(float x, float y, float radius,
                                        float blur_radius) = 0;
        /**
         * Draws an outer rectangular shadow using the current fill color.
         * @param x Left edge of the fully opaque interior.
         * @param y Top edge of the fully opaque interior.
         * @param width Non-negative logical interior width.
         * @param height Non-negative logical interior height.
         * @param blur_radius Non-negative outward falloff distance.
         * @throws std::invalid_argument if any value is invalid or the bounds overflow.
         */
        virtual void draw_rect_shadow(float x, float y, float width, float height,
                                      float blur_radius) = 0;
        /**
         * Draws an outer rounded-rectangle shadow with one logical corner radius.
         * @param x Left edge of the fully opaque interior.
         * @param y Top edge of the fully opaque interior.
         * @param width Non-negative logical interior width.
         * @param height Non-negative logical interior height.
         * @param border_radius Non-negative logical radius applied to every corner.
         * @param blur_radius Non-negative outward falloff distance.
         * @throws std::invalid_argument if any value is invalid or the bounds overflow.
         */
        virtual void draw_rounded_rect_shadow(float x, float y, float width, float height,
                                              float border_radius, float blur_radius) = 0;
        /**
         * Draws an outer rounded-rectangle shadow with independent logical corner radii.
         * @param x Left edge of the fully opaque interior.
         * @param y Top edge of the fully opaque interior.
         * @param width Non-negative logical interior width.
         * @param height Non-negative logical interior height.
         * @param radius_nw North-west corner radius.
         * @param radius_ne North-east corner radius.
         * @param radius_se South-east corner radius.
         * @param radius_sw South-west corner radius.
         * @param blur_radius Non-negative outward falloff distance.
         * @throws std::invalid_argument if any value is invalid or the bounds overflow.
         */
        virtual void draw_rounded_rect_shadow(float x, float y, float width, float height,
                                              float radius_nw, float radius_ne, float radius_se,
                                              float radius_sw, float blur_radius) = 0;
        /** Draws an inset rectangular shadow clipped analytically to the rectangle. */
        void draw_rect_inset_shadow(float x, float y, float width, float height,
                                    float offset_x, float offset_y, float blur_radius,
                                    float spread_radius);
        /** Draws an inset rounded-rectangle shadow clipped analytically to the shape. */
        void draw_rounded_rect_inset_shadow(
            float x, float y, float width, float height, float border_radius,
            float offset_x, float offset_y, float blur_radius, float spread_radius);
        /** Draws an inset circular shadow clipped analytically to the circle. */
        void draw_circle_inset_shadow(float x, float y, float radius, float offset_x,
                                      float offset_y, float blur_radius, float spread_radius);
        /** Draws an inset elliptical shadow clipped analytically to the ellipse. */
        void draw_ellipse_inset_shadow(float x, float y, float radius_x, float radius_y,
                                       float offset_x, float offset_y, float blur_radius,
                                       float spread_radius);

        void set_fill_color(color color);
        void set_stroke_color(color color);
        void set_line_width(float width);
        void set_font(Font& font);
        void use_default_font();
        void set_font_size(int size);

        virtual void set_clear_color(color color) = 0;
        virtual void set_rect_mask(float x, float y, float width, float height) = 0;
        /**
         * Replaces the current mask with a convex polygon in canvas coordinates.
         * @param vertices Ordered clockwise or counter-clockwise vertices. An empty polygon
         * clips all drawing; one or two vertices are invalid. Implementations copy the values.
         * @throws std::invalid_argument for non-finite, degenerate, or non-convex geometry.
         * @throws std::length_error when ResourceLimits::max_clip_vertices is exceeded.
         */
        virtual void set_convex_mask(const std::vector<vec2>& vertices);
        virtual void remove_rect_mask() = 0;

        virtual void draw_frame() = 0;
        virtual void present_frame() = 0;
        virtual std::vector<std::uint8_t> read_pixels() = 0;

        /// Returns the current physical render-target width in pixels.
        int get_width();
        /// Returns the current physical render-target height in pixels.
        int get_height();
        /// Updates the physical render-target extent without changing logical canvas metrics.
        virtual void resize_context(int width, int height) = 0;
        virtual void set_vsync(bool enabled) = 0;
        Font& get_default_font();
        const CanvasMetrics& metrics() const noexcept;
        /// Updates logical canvas coordinates without changing the physical target extent.
        void set_metrics(const CanvasMetrics& metrics);

        // Returned references are owned by this context and remain valid until context teardown.
        virtual Image& create_image(const std::string& file_path,
                                    ImageConfig config = {}) = 0;
        // size must equal width * height * components; input pixels are copied before return.
        virtual Image& create_image(int width, int height, int components,
                                    const unsigned char* data, std::size_t size,
                                    ImageConfig config = {}) = 0;
        virtual Font& create_font(const std::string& file_path) = 0;
        virtual Font& create_font(const unsigned char* buffer, std::size_t size) = 0;

    private:
        friend class Image;
        void image_pixels_updated(Image& image);
        void validate_path_paint(const Paint& paint) const;

    protected:
        struct ImageQuad
        {
            vec2 top_left;
            vec2 top_right;
            vec2 bottom_left;
            vec2 bottom_right;
            vec2 resolution;
        };

        struct ShadowQuad
        {
            ImageQuad geometry;
            vec2 blur_radius;
        };

        struct InsetShadow
        {
            float x = 0.0f;
            float y = 0.0f;
            float width = 0.0f;
            float height = 0.0f;
            float radius = 0.0f;
            float offset_x = 0.0f;
            float offset_y = 0.0f;
            float blur_radius = 0.0f;
            float spread_radius = 0.0f;
            bool ellipse = false;
        };

        struct ShadowSample
        {
            float x = 0.0f;
            float y = 0.0f;
            float alpha = 0.0f;
        };

        struct ShadowKernel
        {
            static constexpr std::size_t capacity = 25U;
            std::array<ShadowSample, capacity> samples{};
            std::size_t count = 0;
        };

        explicit Context(CanvasMetrics metrics = {}, ResourceLimits limits = {});

        Image& own_image(std::unique_ptr<Image> image);
        Font& own_font(std::unique_ptr<Font> font);
        void release_owned_resources() noexcept;
        void validate_resource(const Image& image) const;
        void validate_resource(const Font& font) const;
        void validate_transform(const Transform& transform) const;
        void validate_rect_mask(float x, float y, float width, float height) const;
        void validate_convex_mask(const std::vector<vec2>& vertices) const;
        ImageQuad make_image_quad(float x, float y, float width, float height,
                                  const Transform& transform) const;
        ShadowQuad make_shadow_quad(float x, float y, float width, float height,
                                    float blur_radius) const;
        void validate_shadow_radii(float radius_nw, float radius_ne, float radius_se,
                                   float radius_sw) const;
        void validate_inset_shadow(float offset_x, float offset_y, float blur_radius,
                                   float spread_radius) const;
        ShadowKernel make_shadow_kernel(float blur_radius, float spread_radius,
                                        float target_alpha) const;
        ShadowKernel make_blur_kernel(float blur_radius, float target_alpha) const;
        ShadowKernel make_filter_kernel(float blur_radius, float spread_radius,
                                        float target_alpha, std::size_t maximum_samples,
                                        const char* limit_message) const;
        Image& acquire_path_paint_texture(const std::vector<std::uint8_t>& pixels);
        Image& acquire_cached_path_paint_texture(const Paint& paint, vec2 minimum,
                                                 vec2 maximum);
        void reset_transient_path_resources() noexcept;

        virtual void register_image(Image* image) = 0;
        virtual void register_font(Font* font) = 0;
        virtual void update_image(Image* image) = 0;
        virtual void draw_tessellated_path(const Path& path, const Paint& paint,
                                           float line_width, const Transform& transform) = 0;
        virtual void draw_tessellated_path_blur(
            const Path& path, const Paint& paint, float line_width,
            const ShadowKernel& kernel, float source_alpha,
            const Transform& transform) = 0;
        virtual void draw_text_alpha_mask(float x, float y, std::u32string text,
                                          float erosion_radius,
                                          const Transform& transform) = 0;
        virtual void draw_image_alpha_mask(float x, float y, float width, float height,
                                           Image& image, float erosion_radius,
                                           const Transform& transform) = 0;
        virtual void draw_tessellated_path_eroded_shadow(
            const Path& path, float erosion_radius, const ShadowKernel& kernel,
            const Transform& transform) = 0;
        virtual void draw_tessellated_path_inset_shadow(
            const Path& path, float line_width, float offset_x, float offset_y,
            const ShadowKernel& kernel, float dilation_radius,
            const Transform& transform) = 0;
        virtual void draw_text_inset_alpha_mask(float x, float y, std::u32string text,
                                                float sample_offset_x,
                                                float sample_offset_y,
                                                float dilation_radius,
                                                const Transform& transform) = 0;
        virtual void draw_image_inset_alpha_mask(float x, float y, float width, float height,
                                                 Image& image, float sample_offset_x,
                                                 float sample_offset_y,
                                                 float dilation_radius,
                                                 const Transform& transform) = 0;
        virtual void draw_inset_shadow(const InsetShadow& shadow) = 0;
        virtual void prepare() = 0;

        int _width = 0;
        int _height = 0;
        CanvasMetrics _metrics;
        ResourceLimits _resource_limits;

        color _clear_color;
        color _fill_color;
        color _stroke_color;
        float _line_width = 1;
        int _font_size = 10;
        Font* _font = nullptr;
        Font* _default_font = nullptr;
        std::vector<std::unique_ptr<Image>> _owned_images;
        std::vector<std::unique_ptr<Font>> _owned_fonts;
        struct CachedPathPaint
        {
            Paint paint;
            vec2 minimum;
            vec2 maximum;
            Image* texture = nullptr;
            std::size_t frame_generation = 0;
            bool occupied = false;
        };
        std::vector<CachedPathPaint> _cached_path_paints;
        std::size_t _path_paint_frame_generation = 1;
        std::size_t _path_paint_eviction_cursor = 0;
        std::size_t _active_path_paint_textures = 0;
        std::vector<Image*> _transient_path_paint_textures;
        std::size_t _transient_path_paint_texture_cursor = 0;
    };
} // namespace gcanvas

#endif // GCANVAS_CONTEXT_HPP
