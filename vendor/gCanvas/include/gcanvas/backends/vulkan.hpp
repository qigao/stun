#ifndef GCANVAS_BACKENDS_VULKAN_HPP
#define GCANVAS_BACKENDS_VULKAN_HPP

#include "gcanvas/context.hpp"

#include <memory>
#include <cstdint>
#include <vulkan/vulkan.h>

namespace gcanvas
{
    namespace vulkan
    {
        struct Host
        {
            void* user_data = nullptr;
            const char* const* (*required_instance_extensions)(void* user_data,
                                                                std::uint32_t* count) = nullptr;
            int (*create_surface)(void* user_data, std::uintptr_t instance,
                                  std::uint64_t* surface) = nullptr;
        };

        struct CreateInfo
        {
            CanvasMetrics metrics;
            ResourceLimits resource_limits;
            Host host;
            bool vsync = true;
        };

        /**
         * Mutable, non-owning destination supplied by an embedding Vulkan host.
         *
         * The device identity, queue and extent are fixed for the context lifetime. The host may
         * replace image and command_buffer between synchronized frames. command_buffer must be
         * recording when Context::draw_frame() is called. The host must complete prior submissions
         * that reference the context before changing this object or destroying the context. Context
         * creation and image/font uploads may submit short-lived work to queue and wait for it; the
         * host must externally synchronize all access to that queue.
         */
        struct ExternalTarget
        {
            VkInstance instance = VK_NULL_HANDLE;
            std::uint32_t api_version = VK_API_VERSION_1_0;
            VkPhysicalDevice physical_device = VK_NULL_HANDLE;
            VkDevice device = VK_NULL_HANDLE;
            VkQueue queue = VK_NULL_HANDLE;
            std::uint32_t queue_family_index = 0;
            VkCommandBuffer command_buffer = VK_NULL_HANDLE;
            VkImage image = VK_NULL_HANDLE;
            VkExtent2D extent{};
            VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
            VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImageLayout final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            VkPipelineStageFlags final_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            VkAccessFlags final_access_mask = VK_ACCESS_MEMORY_READ_BIT;
        };

        struct ExternalCreateInfo
        {
            CanvasMetrics metrics;
            ResourceLimits resource_limits;
            ExternalTarget* target = nullptr;
        };

        /**
         * Creates a Vulkan canvas using borrowed surface integration callbacks.
         * host.user_data must outlive the context. Extension names are copied during creation.
         */
        GCANVAS_API std::unique_ptr<Context> create_context(const CreateInfo& create_info);
        GCANVAS_API std::unique_ptr<Context> create_external_context(
            const ExternalCreateInfo& create_info);
    }
}

#endif
