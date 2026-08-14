#include "vulkan_shared_info.hpp"

#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>
#include <stdexcept>

#include "gcanvas/gcanvas.hpp"
#include "vulkan_utils.hpp"

namespace gcanvas
{
    namespace
    {
#if defined(GCANVAS_ENABLE_VULKAN_VALIDATION)
        constexpr const char* validation_layer_name = "VK_LAYER_KHRONOS_validation";

        VKAPI_ATTR VkBool32 VKAPI_CALL validation_callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void*)
        {
            const char* level =
                severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? "ERROR" : "WARNING";
            std::cerr << "VULKAN VALIDATION [" << level << "]: "
                      << (callback_data != nullptr && callback_data->pMessage != nullptr
                              ? callback_data->pMessage
                              : "no diagnostic message")
                      << std::endl;
            return VK_FALSE;
        }
#endif
    }

    VulkanSharedInfo* VulkanSharedInfo::_instance = nullptr;
    std::size_t VulkanSharedInfo::_reference_count = 0;

    VulkanSharedInfo::VulkanSharedInfo(std::vector<std::string> required_extensions)
        : enabled_extensions_(std::move(required_extensions))
    {
        try
        {
            create_instance();
            create_debug_messenger();
            create_physical_devices();
            create_logical_device();
            create_allocator();
            report_backend();
        }
        catch (...)
        {
            if (allocator != nullptr)
                vmaDestroyAllocator(allocator);
            if (device != VK_NULL_HANDLE)
                vkDestroyDevice(device, nullptr);
            destroy_debug_messenger();
            if (instance != VK_NULL_HANDLE)
                vkDestroyInstance(instance, nullptr);
            throw;
        }
    }

    VulkanSharedInfo::VulkanSharedInfo(VkInstance external_instance,
                                       VkPhysicalDevice physical_device,
                                       VkDevice external_device,
                                       std::uint32_t queue_family_index,
                                       std::uint32_t api_version)
        : queueFamilyIndex(queue_family_index), instance(external_instance), device(external_device),
          bestPhysicalDevice(physical_device), owns_device_(false), api_version_(api_version)
    {
        physicalDevices.push_back(physical_device);
        physicalDeviceCount = 1;
        queueCount = 1;
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physical_device, &properties);
        hardware_name = properties.deviceName;
        create_allocator();
    }

    VulkanSharedInfo::~VulkanSharedInfo()
    {
        if (allocator != nullptr)
            vmaDestroyAllocator(allocator);
        if (owns_device_ && device != VK_NULL_HANDLE)
            vkDestroyDevice(device, nullptr);
        destroy_debug_messenger();
        if (owns_device_ && instance != VK_NULL_HANDLE)
            vkDestroyInstance(instance, nullptr);
    }

    VulkanSharedInfo* VulkanSharedInfo::getInstance()
    {
        if (_instance == nullptr)
        {
            throw std::logic_error("Vulkan shared state has not been retained");
        }

        return _instance;
    }

    void VulkanSharedInfo::retain(const std::vector<std::string>& required_extensions)
    {
        if (_instance == nullptr)
        {
            _instance = new VulkanSharedInfo(required_extensions);
        }
        else
        {
            for (const auto& extension : required_extensions)
            {
                if (std::find(_instance->enabled_extensions_.begin(),
                              _instance->enabled_extensions_.end(), extension) ==
                    _instance->enabled_extensions_.end())
                {
                    throw std::invalid_argument(
                        "Vulkan host requires an extension absent from the existing instance");
                }
            }
        }
        ++_reference_count;
    }

    void VulkanSharedInfo::retain_external(VkInstance external_instance,
                                           VkPhysicalDevice physical_device,
                                           VkDevice external_device,
                                           std::uint32_t queue_family_index,
                                           std::uint32_t api_version)
    {
        if (external_instance == VK_NULL_HANDLE || physical_device == VK_NULL_HANDLE ||
            external_device == VK_NULL_HANDLE || api_version < VK_API_VERSION_1_0)
        {
            throw std::invalid_argument("External Vulkan handles must be valid");
        }
        if (_instance == nullptr)
        {
            _instance = new VulkanSharedInfo(external_instance, physical_device, external_device,
                                             queue_family_index, api_version);
        }
        else if (_instance->instance != external_instance ||
                 _instance->bestPhysicalDevice != physical_device ||
                 _instance->device != external_device ||
                 _instance->queueFamilyIndex != queue_family_index ||
                 _instance->api_version_ != api_version)
        {
            throw std::invalid_argument(
                "External Vulkan context conflicts with retained gCanvas device state");
        }
        ++_reference_count;
    }

    void VulkanSharedInfo::release() noexcept
    {
        if (_reference_count == 0)
        {
            return;
        }
        --_reference_count;
        if (_reference_count == 0)
        {
            delete _instance;
            _instance = nullptr;
        }
    }

    void VulkanSharedInfo::create_instance()
    {
        // --------------- Create Application Info ---------------

        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pNext = nullptr;
        applicationInfo.pApplicationName = GCANVAS_LIBRARY_NAME;
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pEngineName = GCANVAS_LIBRARY_NAME;
        applicationInfo.engineVersion =
            VK_MAKE_VERSION(GCANVAS_VERSION_MAJOR, GCANVAS_VERSION_MINOR, GCANVAS_VERSION_PATCH);
        applicationInfo.apiVersion = VK_API_VERSION_1_2;

        // --------------- Get Required Instance Extensions ---------------

#if defined(GCANVAS_ENABLE_VULKAN_VALIDATION)
        if (std::find(enabled_extensions_.begin(), enabled_extensions_.end(),
                      VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == enabled_extensions_.end())
        {
            enabled_extensions_.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
#endif

        std::vector<const char*> extensions;
        extensions.reserve(enabled_extensions_.size());
        for (const auto& extension : enabled_extensions_)
        {
            extensions.push_back(extension.c_str());
        }

        std::vector<const char*> validationLayers = {};
#if defined(GCANVAS_ENABLE_VULKAN_VALIDATION)
        // --------------- Find Layer ---------------

        uint32_t layerCount = 0;
        vku::err_check(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vku::err_check(
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()));

        for (const VkLayerProperties& layerProperties : availableLayers)
        {
            if (strcmp(layerProperties.layerName, validation_layer_name) == 0)
            {
                validationLayers.push_back(validation_layer_name);
                break;
            }
        }

        if (validationLayers.empty())
        {
            throw std::runtime_error(
                "GCANVAS_ENABLE_VULKAN_VALIDATION requires VK_LAYER_KHRONOS_validation");
        }
#endif

        // --------------- Create Instance Create Info ---------------

        VkInstanceCreateInfo instanceCreateInfo{};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pNext = nullptr;
        instanceCreateInfo.flags = 0;
        instanceCreateInfo.pApplicationInfo = &applicationInfo;
        instanceCreateInfo.enabledLayerCount = (uint32_t)validationLayers.size();
        instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
        instanceCreateInfo.enabledExtensionCount = (uint32_t)extensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

        // --------------- Create Instance ---------------

        vku::err_check(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));
    }

    void VulkanSharedInfo::create_physical_devices()
    {
        // --------------- Get Physical Devices ---------------
        vku::err_check(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));

        if (physicalDeviceCount == 0)
        {
            throw std::runtime_error("Could not find a Vulkan physical device");
        }

        physicalDevices.resize(physicalDeviceCount);
        vku::err_check(
            vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data()));
        physicalDevices.resize(physicalDeviceCount);

        // --------------- Select Best Physical Device ---------------

        vku::best_device_info bdi =
            vku::select_physical_device(physicalDevices.data(), physicalDeviceCount);
        bestPhysicalDevice = physicalDevices[bdi.device_index];
        queueFamilyIndex = bdi.queue_family_index;
        queueCount = bdi.queue_count;
        hardware_name = bdi.device_name;
    }

    void VulkanSharedInfo::create_logical_device()
    {
        // --------------- Create Device Queue Create Info ---------------

        const float queuePriority = 1.0f;

        VkDeviceQueueCreateInfo deviceQueueCreateInfo{};
        deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        deviceQueueCreateInfo.pNext = nullptr;
        deviceQueueCreateInfo.flags = 0;
        deviceQueueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        deviceQueueCreateInfo.queueCount = queueCount;
        deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

        // --------------- Create Device Create Info ---------------

        VkPhysicalDeviceFeatures availableFeatures{};
        vkGetPhysicalDeviceFeatures(bestPhysicalDevice, &availableFeatures);
        if (availableFeatures.shaderSampledImageArrayDynamicIndexing != VK_TRUE)
        {
            throw std::runtime_error(
                "Selected Vulkan device does not support dynamic sampler array indexing");
        }
        VkPhysicalDeviceFeatures physicalDeviceFeatures{};
        physicalDeviceFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE;

        const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = nullptr;
        deviceCreateInfo.flags = 0;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
        deviceCreateInfo.enabledLayerCount = 0;
        deviceCreateInfo.ppEnabledLayerNames = nullptr;
        deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
        deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;

        // --------------- Create Device ---------------

        vku::err_check(vkCreateDevice(bestPhysicalDevice, &deviceCreateInfo, nullptr, &device));
    }

    void VulkanSharedInfo::create_allocator()
    {
        VmaAllocatorCreateInfo create_info{};
        create_info.physicalDevice = bestPhysicalDevice;
        create_info.device = device;
        create_info.instance = instance;
        create_info.vulkanApiVersion = api_version_;
        vku::err_check(vmaCreateAllocator(&create_info, &allocator));
    }

    void VulkanSharedInfo::create_debug_messenger()
    {
#if defined(GCANVAS_ENABLE_VULKAN_VALIDATION)
        VkDebugUtilsMessengerCreateInfoEXT create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        create_info.pfnUserCallback = validation_callback;
        const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (create == nullptr)
            throw std::runtime_error("Vulkan debug-utils extension entry point is unavailable");
        vku::err_check(create(instance, &create_info, nullptr, &debug_messenger_));
#endif
    }

    void VulkanSharedInfo::destroy_debug_messenger() noexcept
    {
        if (debug_messenger_ == VK_NULL_HANDLE || instance == VK_NULL_HANDLE)
            return;
        const auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy != nullptr)
            destroy(instance, debug_messenger_, nullptr);
        debug_messenger_ = VK_NULL_HANDLE;
    }

    void VulkanSharedInfo::report_backend() const
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(bestPhysicalDevice, &properties);

        std::cout << "BACKEND: Vulkan " << VK_VERSION_MAJOR(properties.apiVersion) << "."
                  << VK_VERSION_MINOR(properties.apiVersion) << "."
                  << VK_VERSION_PATCH(properties.apiVersion) << " - " << properties.deviceName
                  << " Build " << properties.driverVersion << std::endl;
    }
} // namespace gcanvas
