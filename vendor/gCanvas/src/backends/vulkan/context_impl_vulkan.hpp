#ifndef GCANVAS_VULKAN_CONTEXT_HPP
#define GCANVAS_VULKAN_CONTEXT_HPP

#include <array>
#include <atomic>
#include <limits>
#include <string>
#include <vector>

#include "gcanvas/backends/vulkan.hpp"
#include "gcanvas/color.hpp"
#include "gcanvas/context.hpp"
#include "gcanvas/vec2.hpp"
#include "image_impl_vulkan.hpp"
#include "vertex.hpp"
#include "vulkan_shared_info.hpp"
#include "vulkan_utils.hpp"

namespace gcanvas
{
    class ContextImplVulkan : public Context
    {
    public:
        struct uniform_rect
        {
            struct effect_values
            {
                float is_msdf;
                float shadow_blur_y;
                float reserved;
            };

            class color color;      // 16   1
            vec2 vertices[4];       // 32   2
            float border_radius[4]; // 16   1
            float sampler_index;    //
            float use_tint;         //
            vec2 resolution;        // 16   1
            vec2 uvs[2];            // 16   1
            float line_width[4];    // 16   1
            float shadow_blur_x;    //
            effect_values effects;  // is_msdf, shadow_blur_y, reserved
        };                          // 128

        static constexpr std::size_t uniform_rect_capacity = 65536U * 2U;
        static constexpr VkDeviceSize uniform_rect_buffer_size =
            uniform_rect_capacity * sizeof(uniform_rect);
        // A submitted slot remains immutable until its fence signals; two slots let CPU recording
        // overlap the previous GPU submission without retaining any scene or draw command.
        static constexpr std::size_t frames_in_flight = 2U;
        static constexpr std::uint32_t invalid_image_index =
            std::numeric_limits<std::uint32_t>::max();

        struct draw_sequence
        {
            int start_index;
            VkRect2D scissor;
            std::uint32_t mask_instance_index = 0;
            std::uint32_t mask_triangle_count = 0;
            bool convex_mask_active = false;
        };
        struct path_draw
        {
            std::uint32_t instance_index;
            std::uint32_t mask_count;
            bool even_odd;
            std::uint32_t inset_sample_count = 0;
            std::uint32_t shifted_base_count = 0;
            std::uint32_t shifted_extra_count = 0;
            bool shifted_even_odd = false;
            std::uint32_t erosion_mask_count = 0;
            std::uint32_t erosion_sample_count = 0;
            std::uint32_t sample_offset_index = 0;
            std::uint32_t blur_sample_count = 0;
        };

        struct frame_resources
        {
            VkBuffer storage_buffer = VK_NULL_HANDLE;
            VmaAllocation storage_allocation = nullptr;
            void* storage_mapped_data = nullptr;
            VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
            VkSemaphore image_available = VK_NULL_HANDLE;
            VkSemaphore rendering_complete = VK_NULL_HANDLE;
            VkFence render_fence = VK_NULL_HANDLE;
        };

        std::unique_ptr<ImageImplVulkan> dummy;
        std::vector<ImageImplVulkan*> images;

        // std::vector<vertex> rect_vertices = {{vec2(0)}, {vec2(1, 0)}, {vec2(0, 1)}, {vec2(1)}};
        std::vector<uint32_t> rect_indices = {0, 1, 2, 1, 3, 2};

        std::vector<point_vertex> rect_vertices = {
            {vec2(0)}, {vec2(1, 0)}, {vec2(0, 1)}, {vec2(1)}};

        std::vector<draw_sequence> draw_sequence_chain = {};
        std::vector<path_draw> path_draws = {};
        std::vector<vec2> path_sample_offsets = {};

        /// std::vector<uniform_rect> uniforms = {};
        std::vector<uniform_rect> storage = {};
        std::uint32_t texture_array_size = 0;

        vulkan::Host _host;
        bool _vsync = true;

        std::atomic<bool> resizing = false;
        std::atomic<bool> rendering = false;
        bool headless = false;
        bool dirty = true;
        bool swapchain_suboptimal = false;

        // Valid only while rendering is true: submitted by draw_frame(), consumed by
        // present_frame(). The context is single-thread driven despite atomic lifecycle flags.
        uint32_t imageIndex = invalid_image_index;

        VkClearValue clearValue = {};
        VkFormat selectedImageFormat;
        VkExtent2D swapchainExtent{};

        VkSurfaceKHR surface{};
        VkQueue queue{};
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        std::vector<VkImage> swapchainImages;
        uint32_t swapchainImageCount = 0;
        std::vector<VkImageView> imageViews;
        VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
        std::vector<VkImage> stencilImages;
        std::vector<VmaAllocation> stencilAllocations;
        std::vector<VkImageView> stencilImageViews;
        VkShaderModule fragShaderModule{};
        VkShaderModule vertShaderModule{};
        VkPipelineLayout pipelineLayout{};
        VkRenderPass renderPass{};
        VkPipeline pipeline{};
        VkPipeline clippedPipeline{};
        VkPipeline convexMaskPipeline{};
        VkPipeline evenOddMaskPipeline{};
        VkPipeline unionMaskPipeline{};
        VkPipeline pathPaintPipeline{};
        VkPipeline evenOddClippedMaskPipeline{};
        VkPipeline unionClippedMaskPipeline{};
        VkPipeline clippedPathPaintPipeline{};
        VkPipeline pathClearPipeline{};
        VkPipeline shiftedEvenOddMaskPipeline{};
        VkPipeline shiftedUnionMaskPipeline{};
        VkPipeline shiftedEvenOddClippedMaskPipeline{};
        VkPipeline shiftedUnionClippedMaskPipeline{};
        VkPipeline insetPathPaintPipeline{};
        VkPipeline clippedInsetPathPaintPipeline{};
        VkPipeline shiftedPathClearPipeline{};
        std::vector<VkFramebuffer> frameBuffers;
        VkCommandPool commandPool{};
        std::vector<VkCommandBuffer> commandBuffers;

        VkBuffer indexBuffer{};
        VmaAllocation indexBufferAllocation = nullptr;

        VkDescriptorSetLayout descriptorSetLayout{};
        VkDescriptorPool descriptorPool{};
        std::array<frame_resources, frames_in_flight> frameResources{};
        std::vector<VkFence> imagesInFlight;
        std::size_t currentFrame = 0U;

        explicit ContextImplVulkan(const vulkan::CreateInfo& create_info);
        explicit ContextImplVulkan(const vulkan::ExternalCreateInfo& create_info);
        ~ContextImplVulkan() override;

        Backend backend() const noexcept override { return Backend::Vulkan; }
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

        void initialize_limits();
        void create_surface();
        void create_queue();
        void create_swapchain();
        void check_surface_support();
        void create_image_views();
        void create_stencil_resources();
        void destroy_stencil_resources() noexcept;
        void create_render_pass();
        void create_descriptor_set_layout();
        void create_pipeline();
        void destroy_pipeline_resources() noexcept;
        void create_framebuffer();
        void create_command_pool();
        void create_command_buffers();
        void create_index_buffers();
        void create_storage_buffer();
        void create_descriptor_pool();
        void create_descriptor_set();
        void record_command_buffer(std::uint32_t image_index, VkDescriptorSet descriptor_set);
        void record_draw_commands(VkCommandBuffer command_buffer, VkFramebuffer framebuffer,
                                  VkDescriptorSet descriptor_set);
        void record_draw_sequences(VkCommandBuffer command_buffer, VkDescriptorSet descriptor_set);
        void record_external_frame();
        void sync_external_target();
        void append_uniform(uniform_rect value);
        void clear_draw_queue();
        void reset_frame_state() noexcept;
        void create_semaphores();
        void initialize_resources();

        void update_storage(frame_resources& frame);
        void update_swapchain(uint32_t width, uint32_t height);
        void update_image_descriptor(ImageImplVulkan& image);
        void cleanup() noexcept;

        bool shared_retained_ = false;
        vulkan::ExternalTarget* external_target_ = nullptr;

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

#endif // GCANVAS_VULKAN_CONTEXT_HPP
