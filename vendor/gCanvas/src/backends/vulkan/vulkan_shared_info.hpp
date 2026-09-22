#ifndef GCANVAS_VULKAN_SHARED_INFO_HPP
#define GCANVAS_VULKAN_SHARED_INFO_HPP

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstdint>
#include <string>
#include <vector>

namespace gcanvas
{
    class VulkanSharedInfo
    {
    public:
        std::string hardware_name = "";
        uint32_t queueFamilyIndex = -1;
        uint32_t queueCount = -1;
        uint32_t physicalDeviceCount = -1;

        VkInstance instance = VK_NULL_HANDLE;
        std::vector<VkPhysicalDevice> physicalDevices;
        VkDevice device = VK_NULL_HANDLE;
        VkPhysicalDevice bestPhysicalDevice = VK_NULL_HANDLE;
        VmaAllocator allocator = nullptr;

        static VulkanSharedInfo* getInstance();
        static void retain(const std::vector<std::string>& required_extensions);
        static void retain_external(VkInstance instance, VkPhysicalDevice physical_device,
                                    VkDevice device, std::uint32_t queue_family_index,
                                    std::uint32_t api_version);
        static void release() noexcept;

    private:
        static VulkanSharedInfo* _instance;
        static std::size_t _reference_count;

        explicit VulkanSharedInfo(std::vector<std::string> required_extensions);
        VulkanSharedInfo(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device,
                         std::uint32_t queue_family_index, std::uint32_t api_version);
        VulkanSharedInfo(const VulkanSharedInfo&)
        {
        }
        ~VulkanSharedInfo();

        void create_instance();
        void create_physical_devices();
        void create_logical_device();
        void create_allocator();
        void create_debug_messenger();
        void destroy_debug_messenger() noexcept;
        void report_backend() const;

        std::vector<std::string> enabled_extensions_;
        VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
        bool owns_device_ = true;
        std::uint32_t api_version_ = VK_API_VERSION_1_2;
    };

} // namespace gcanvas

#endif // GCANVAS_VULKAN_SHARED_INFO_HPP
