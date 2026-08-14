#ifndef GCANVAS_VULKAN_UTILS_HPP
#define GCANVAS_VULKAN_UTILS_HPP

#include <cstring>
#include <string>
#include <vector>

#include "vulkan_shared_info.hpp"

namespace gcanvas::vku
{
    struct best_device_info
    {
        std::string device_name;
        int device_index;
        uint32_t queue_family_index;
        uint32_t queue_count;
    };

    /* --------------- PRINTEER --------------- */
    void err(std::string message);
    void print_layers();
    void print_extensions();
    void print_physical_devices();
    void print_selected_device();

    void err_check(const VkResult& result);
    std::string device_type(const VkPhysicalDeviceType& deviceType);

    /* --------------- GENERATOR --------------- */

    VkCommandBuffer beginSingleTimeCommands(const VkCommandPool& commandPool);
    void endSingleTimeCommands(VkCommandBuffer commandBuffer, const VkCommandPool& commandPool,
                               const VkQueue& queue);

    void create_buffer(const VkDeviceSize& deviceSize, const VkBufferUsageFlags& bufferUsageFlags,
                       VkBuffer& buffer, const VkMemoryPropertyFlags& memoryPropertyFlags,
                       VmaAllocation& allocation,
                       VmaAllocationCreateFlags allocation_flags = 0,
                       VmaAllocationInfo* allocation_info = nullptr);
    void copy_buffer(const VkBuffer& src, VkBuffer& dest, const VkDeviceSize& deviceSize,
                     const VkCommandPool& commandPool, const VkQueue& queue);
    template <typename T>
    void create_and_upload_buffer(std::vector<T> data, const VkBufferUsageFlags& usageFlags,
                                  VkBuffer& buffer, VmaAllocation& allocation,
                                  const VkCommandPool& commandPool, const VkQueue& queue)
    {

        // --------------- Create Staging Buffer and Memory ---------------

        VkDeviceSize bufferSize = sizeof(T) * data.size();
        VkBuffer stagingBuffer{};
        VmaAllocation stagingAllocation = nullptr;

        vku::create_buffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           stagingAllocation);

        try
        {
            void* rawData;
            vku::err_check(vmaMapMemory(VulkanSharedInfo::getInstance()->allocator,
                                        stagingAllocation, &rawData));
            std::memcpy(rawData, data.data(), bufferSize);
            vmaUnmapMemory(VulkanSharedInfo::getInstance()->allocator, stagingAllocation);

            // --------------- Create Buffer and Memory ---------------

            vku::create_buffer(bufferSize, usageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT, buffer,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocation);

            vku::copy_buffer(stagingBuffer, buffer, bufferSize, commandPool, queue);
        }
        catch (...)
        {
            if (buffer != VK_NULL_HANDLE || allocation != nullptr)
                vmaDestroyBuffer(VulkanSharedInfo::getInstance()->allocator, buffer, allocation);
            if (stagingBuffer != VK_NULL_HANDLE || stagingAllocation != nullptr)
                vmaDestroyBuffer(VulkanSharedInfo::getInstance()->allocator, stagingBuffer,
                                 stagingAllocation);
            buffer = VK_NULL_HANDLE;
            allocation = nullptr;
            throw;
        }

        // --------------- Cleanup ---------------

        vmaDestroyBuffer(VulkanSharedInfo::getInstance()->allocator, stagingBuffer,
                         stagingAllocation);
    }

    void create_shader_module(const std::string& filename, VkShaderModule* shaderModule);
    void create_shader_module(std::vector<unsigned char> data, VkShaderModule* shaderModule);
    std::vector<char> read_shader(const std::string& filename);

    /* --------------- SELECTOR --------------- */

    best_device_info select_physical_device(VkPhysicalDevice* physicalDevices, uint32_t count);
    VkPresentModeKHR select_present_mode(const VkPresentModeKHR* presentModes,
                                         uint32_t presentModeCount, bool vsync);

} // namespace gcanvas::vku

#endif // GCANVAS_VULKAN_UTILS_HPP
