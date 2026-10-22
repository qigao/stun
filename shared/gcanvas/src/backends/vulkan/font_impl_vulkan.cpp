#include "font_impl_vulkan.hpp"

#include <iostream>
#include <map>
#include <vector>

#include "image_impl_vulkan.hpp"

namespace gcanvas
{
    FontImplVulkan::FontImplVulkan(std::string file_path)
    {
        load_from_file(file_path);

        _loaded = true;
    }

    FontImplVulkan::FontImplVulkan(const unsigned char* buffer, size_t size)
    {
        load_from_memory(buffer, size);

        _loaded = true;
    }

    void FontImplVulkan::upload(const VkCommandPool& commandPool, const VkQueue& queue)
    {
        if (!_loaded)
            return;

        auto* iiv = static_cast<ImageImplVulkan*>(_texture_atlas.get());
        iiv->upload(commandPool, queue);

        _uploaded = true;
    }

    std::unique_ptr<Image> FontImplVulkan::create_atlas(int width, int height, int components,
                                                       const unsigned char* data)
    {
        const auto size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
                          static_cast<std::size_t>(components);
        return std::make_unique<ImageImplVulkan>(width, height, components, data, size,
                                                 ImageConfig{});
    }

} // namespace gcanvas
