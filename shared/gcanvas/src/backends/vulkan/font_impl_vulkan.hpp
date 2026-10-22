#ifndef GCANVAS_FONT_IMPL_VULKAN_HPP
#define GCANVAS_FONT_IMPL_VULKAN_HPP

#include "gcanvas/font.hpp"

#include <vulkan/vulkan.h>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace gcanvas
{
    class FontImplVulkan : public Font
    {
    public:
        FontImplVulkan(std::string file_path);
        FontImplVulkan(const unsigned char* buffer, size_t size);
        ~FontImplVulkan() override = default;

        bool _loaded = false;
        bool _uploaded = false;

        void upload(const VkCommandPool& commandPool, const VkQueue& queue);

    protected:
        std::unique_ptr<Image> create_atlas(int width, int height, int components,
                                            const unsigned char* data) override;
    };

} // namespace gcanvas

#endif // GCANVAS_FONT_IMPL_VULKAN_HPP
