#ifndef GCANVAS_IMAGE_IMPL_VULKAN_HPP
#define GCANVAS_IMAGE_IMPL_VULKAN_HPP

#include <map>
#include <string>

#include "gcanvas/image.hpp"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace gcanvas
{
    class ImageImplVulkan : public Image
    {
    public:
        ImageImplVulkan(std::string file_path, ImageConfig imageConfig);
        ImageImplVulkan(int width, int height, int components, const unsigned char* data,
                        std::size_t size, ImageConfig imageConfig);
        ~ImageImplVulkan();

        VkImage _image{};
        VmaAllocation _allocation = nullptr;
        VkImageView _imageView{};
        VkImageLayout _imageLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        VkSampler _sampler{};
        int _sampler_index = -1;
        uint32_t _mipLevels = 1;
        ImageConfig _imageConfig = {};

        VmaAllocation _stagingAllocation = nullptr;
        VkBuffer _stagingBuffer = {};

        VkFormat _format = {};
        bool _uploaded = false;

        unsigned char* _padded_data = nullptr;

        void upload(const VkCommandPool& commandPool, const VkQueue& queue);
        void upload_update(const VkCommandPool& commandPool, const VkQueue& queue);
        void write_buffer(const VkCommandPool& commandPool, const VkQueue& queue, VkBuffer buffer);
        void change_layout(const VkCommandPool& commandPool, const VkQueue& queue,
                           const VkImageLayout& layout);
        void generate_mipmaps(const VkCommandPool& commandPool, const VkQueue& queue,
                              const VkFormat& format);

        VkSampler get_sampler();
        VkImageView get_image_view();

    private:
        void init_format();
        void pad_data();
    };

} // namespace gcanvas

#endif // GCANVAS_IMAGE_IMPL_VULKAN_HPP
