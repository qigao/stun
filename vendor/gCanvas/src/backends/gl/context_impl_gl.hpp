#ifndef GCANVAS_GL_CONTEXT_HPP
#define GCANVAS_GL_CONTEXT_HPP

#include "gcanvas/context.hpp"
#include "gl_backend.hpp"

#include <string>
#include <vector>
#include <array>
#include <atomic>

#include "gcanvas/vec2.hpp"
#include "gcanvas/color.hpp"
#include "image_impl_gl.hpp"

//#define UNIFORM_BUFFER_ARRAY_MAX_COUNT 65536*2
//#define UNIFORM_RECT_BUFFER_ARRAY_MAX_SIZE UNIFORM_BUFFER_ARRAY_MAX_COUNT * sizeof(uniform_rect)

namespace gcanvas
{
    class ContextImplGl : public Context
    {
    public:
        static constexpr int shader_texture_array_size = 10;
        int MAX_UNIFORM_RECT_PER_BLOCK_COUNT = -1;

        struct uniform_rect
        {
            struct effect_values
            {
                float is_msdf;
                float shadow_blur_y;
                float reserved;
            };

            class color color;      // 32   1
            vec2 vertices[4];       // 64   2
            float border_radius[4]; // 32   1        
            float sampler_index;    //  
            float use_tint;         // 
            vec2 resolution;        // 32   1
            vec2 uvs[2];            // 32   1
            float line_width[4];    // 32   1
            float shadow_blur_x;    //
            effect_values effects;  // is_msdf, shadow_blur_y, reserved
        };                          // 256


        struct point_vertex
        {
            vec2 position;
        };

        enum draw_call_type
        {
            UNSET,
            COLOR,
            PATH,
            PATH_ERODE,
            PATH_INSET,
            SCISSOR,
            CONVEX_MASK,
            SCISSOR_CLEAR,
            TEXTURE_INCREMENT
        };
        struct draw_call_chain
        {
            int instance_index;
            int scissor_index;
            draw_call_type type;
            int path_index = -1;
        };
        struct path_draw
        {
            int instance_index;
            int mask_count;
            bool even_odd;
        };
        struct path_inset_draw
        {
            int instance_index;
            int original_mask_count;
            int shifted_base_count;
            int shifted_extra_count;
            int sample_count;
            bool original_even_odd;
            bool shifted_even_odd;
        };
        struct path_erode_draw
        {
            int instance_index;
            int base_mask_count;
            int erosion_mask_count;
            int sample_count;
            bool even_odd;
        };
        struct convex_mask_draw
        {
            int instance_index;
            int triangle_count;
        };
        struct scissor_primitive
        {
            GLint x;
            GLint y;
            GLsizei width;
            GLsizei height;
        };

       
        std::unique_ptr<ImageImplGl> dummy;
        std::vector<ImageImplGl*> images;

        //std::vector<vertex> rect_vertices = {{vec2(0)}, {vec2(1, 0)}, {vec2(0, 1)}, {vec2(1)}};
        std::vector<uint32_t> rect_indices = {0, 1, 2, 1, 3, 2};
        
        std::vector<point_vertex> rect_vertices = {
            {vec2(0)}, {vec2(1, 0)}, {vec2(0, 1)}, {vec2(1)}
        };

        std::vector<scissor_primitive> scissor_primitives = {};
        std::vector<draw_call_chain> draw_call_indices = {};
        std::vector<path_draw> path_draws = {};
        std::vector<path_erode_draw> path_erode_draws = {};
        std::vector<path_inset_draw> path_inset_draws = {};
        std::vector<convex_mask_draw> convex_mask_draws = {};
        int current_color_call_cnt = 0;
        draw_call_type current_type = UNSET;

        ///std::vector<uniform_rect> uniforms = {};
        std::vector<uniform_rect> storage = {};
        int last_uniform_cnt = 0;

        int texture_array_size = shader_texture_array_size;

        detail::GlHost _host;
        detail::GlPresentationMode _presentation;
        Backend _backend = Backend::OpenGL;
        const char* _vertex_shader_source = nullptr;
        const char* _fragment_shader_source = nullptr;

        std::atomic<bool> resizing = false;
        std::atomic<bool> rendering = false;
        bool headless = false;
        bool dirty = true;
        

        int shaderProgram = 0;
        int instance_offset_location = -1;

        GLuint vertex_array_object{};
        GLuint vertex_buffer{};
        GLuint index_buffer{};

        GLuint storageBuffer{};

        explicit ContextImplGl(const detail::GlCreateInfo& create_info);
        ~ContextImplGl() override;

        Backend backend() const noexcept override { return _backend; }
        void stroke_rect(float x, float y, float width, float height) override;
        void stroke_rounded_rect(float x, float y, float width, float height,
                                 float border_radius) override;
        void stroke_rounded_rect(float x, float y, float width, float height, float radius_nw,
                                 float radius_ne, float radius_se, float radius_sw) override;
        void stroke_circle(float x, float y, float radius) override;
        void stroke_ellipse(float x, float y, float radius_x, float radius_y) override;
        void fill_rect(float x, float y, float width, float height) override;
        void fill_rounded_rect(float x, float y, float width, float height,
                               float border_radius) override;
        void fill_rounded_rect(float x, float y, float width, float height, float radius_nw,
                               float radius_ne, float radius_se, float radius_sw) override;
        void fill_circle(float x, float y, float radius) override;
        void fill_ellipse(float x, float y, float radius_x, float radius_y) override;
        void draw_text(float x, float y, std::string text) override;
        void draw_text(float x, float y, std::u32string text) override;
        void draw_text(float x, float y, std::string text,
                       const Transform& transform) override;
        void draw_text(float x, float y, std::u32string text,
                       const Transform& transform) override;
        void draw_image(float x, float y, float width, float height, Image& image,
                        bool tint = false) override;
        void draw_image(float x, float y, float width, float height, Image& image,
                        const Transform& transform, bool tint = false) override;
        void draw_image(float x, float y, float width, float height, Image& image, float src_x,
                        float src_y, float src_width, float src_height,
                        bool tint = false) override;
        void draw_rounded_image(float x, float y, float width, float height, Image& image,
                                float border_radius, bool tint = false) override;
        void draw_rounded_image(float x, float y, float width, float height, Image& image,
                                float radius_nw, float radius_ne, float radius_se, float radius_sw,
                                bool tint = false) override;
        void draw_rounded_image(float x, float y, float width, float height, Image& image,
                                float radius_nw, float radius_ne, float radius_se, float radius_sw,
                                float src_x, float src_y, float src_width, float src_height,
                                bool tint = false) override;
        void draw_circle_shadow(float x, float y, float radius, float shadow_size) override;
        void draw_rect_shadow(float x, float y, float width, float height,
                              float shadow_size) override;
        void draw_rounded_rect_shadow(float x, float y, float width, float height,
                                      float border_radius, float shadow_size) override;
        void draw_rounded_rect_shadow(float x, float y, float width, float height, float radius_nw,
                                      float radius_ne, float radius_se, float radius_sw,
                                      float shadow_size) override;
        void set_clear_color(color value) override;
        void set_rect_mask(float x, float y, float width, float height) override;
        void set_convex_mask(const std::vector<vec2>& vertices) override;
        void remove_rect_mask() override;
        void draw_frame() override;
        void present_frame() override;
        std::vector<std::uint8_t> read_pixels() override;
        void resize_context(int width, int height) override;
        void set_vsync(bool enabled) override;
        Image& create_image(const std::string& file_path, ImageConfig config = {}) override;
        Image& create_image(int width, int height, int components, const unsigned char* data,
                            std::size_t size, ImageConfig config = {}) override;
        Font& create_font(const std::string& file_path) override;
        Font& create_font(const unsigned char* buffer, std::size_t size) override;

        void configure_surface();
        void create_uniform_buffer();
        //void create_storage_buffer();
        void create_vertex_array();
        void create_shader_programm();
        void initialize_resources();
        void update_viewport(uint32_t width, uint32_t height);
        void cleanup() noexcept;

        bool shared_retained_ = false;

        void update_uniform_buffer();
        //void update_storage_buffer();

        void queue_color_call();
        void clear_draw_queue() noexcept;
    protected:
        void register_image(Image* image) override;
        void register_font(Font* font) override;
        void update_image(Image* image) override;
        void prepare() override;
        void draw_tessellated_path(const Path& path, const Paint& paint, float line_width,
                                   const Transform& transform) override;
        void draw_tessellated_path_blur(const Path& path, const Paint& paint,
                                        float line_width, const ShadowKernel& kernel,
                                        float source_alpha,
                                        const Transform& transform) override;
        void draw_text_alpha_mask(float x, float y, std::u32string text, float erosion_radius,
                                  const Transform& transform) override;
        void draw_image_alpha_mask(float x, float y, float width, float height, Image& image,
                                   float erosion_radius, const Transform& transform) override;
        void draw_tessellated_path_eroded_shadow(
            const Path& path, float erosion_radius, const ShadowKernel& kernel,
            const Transform& transform) override;
        void draw_tessellated_path_inset_shadow(
            const Path& path, float line_width, float offset_x, float offset_y,
            const ShadowKernel& kernel, float dilation_radius,
            const Transform& transform) override;
        void draw_text_inset_alpha_mask(float x, float y, std::u32string text,
                                        float sample_offset_x, float sample_offset_y,
                                        float dilation_radius,
                                        const Transform& transform) override;
        void draw_image_inset_alpha_mask(float x, float y, float width, float height,
                                         Image& image, float sample_offset_x,
                                         float sample_offset_y,
                                         float dilation_radius,
                                         const Transform& transform) override;
        void draw_inset_shadow(const InsetShadow& shadow) override;
        void draw_text_impl(float x, float y, const std::u32string& text,
                            const Transform& transform, bool alpha_mask,
                            const vec2* inset_sample_offset = nullptr,
                            float morphology_radius = 0.0f);
    };

} // namespace gcanvas

#endif // GCANVAS_GL_CONTEXT_HPP
