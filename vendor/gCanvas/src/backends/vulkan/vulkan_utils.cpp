#include "vulkan_utils.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace gcanvas::vku
{
    namespace
    {
        int device_type_rank(VkPhysicalDeviceType type) noexcept
        {
            switch (type)
            {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                return 4;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                return 3;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                return 2;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                return 1;
            default:
                return 0;
            }
        }

        bool supports_swapchain(VkPhysicalDevice device)
        {
            std::uint32_t count = 0;
            err_check(vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr));
            std::vector<VkExtensionProperties> extensions(count);
            err_check(vkEnumerateDeviceExtensionProperties(device, nullptr, &count,
                                                            extensions.data()));
            return std::any_of(extensions.begin(), extensions.end(), [](const auto& extension) {
                return std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
            });
        }
    }

    void err(std::string message)
    {
        std::cerr << "VK ERROR: " << message << std::endl;
    }

    std::string device_type(const VkPhysicalDeviceType& deviceType)
    {
        std::string dtype = "";

        switch (deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            dtype = "Integrate GPU";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            dtype = "Discrete GPU";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            dtype = "Virtual GPU";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            dtype = "CPU";
            break;
        default:
            dtype = "Unknown";
            break;
        }

        return dtype;
    }

    void create_shader_module(const std::string& filename, VkShaderModule* shaderModule)
    {
        std::vector<char> spirvCode = read_shader(filename);

        VkShaderModuleCreateInfo shaderModuleCreateInfo{};
        shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.pNext = nullptr;
        shaderModuleCreateInfo.flags = 0;
        shaderModuleCreateInfo.codeSize = spirvCode.size();
        shaderModuleCreateInfo.pCode = (uint32_t*)spirvCode.data();

        vku::err_check(vkCreateShaderModule(VulkanSharedInfo::getInstance()->device,
                                            &shaderModuleCreateInfo, nullptr, shaderModule));
    }

    void create_shader_module(std::vector<unsigned char> data, VkShaderModule* shaderModule)
    {
        VkShaderModuleCreateInfo shaderModuleCreateInfo{};
        shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.pNext = nullptr;
        shaderModuleCreateInfo.flags = 0;
        shaderModuleCreateInfo.codeSize = data.size();
        shaderModuleCreateInfo.pCode = (uint32_t*)data.data();

        vku::err_check(vkCreateShaderModule(VulkanSharedInfo::getInstance()->device,
                                            &shaderModuleCreateInfo, nullptr, shaderModule));
    }

    std::vector<char> read_shader(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (file)
        {
            size_t fileSize = (size_t)file.tellg();
            std::vector<char> fileBuffer(fileSize);
            file.seekg(0);
            file.read(fileBuffer.data(), fileSize);
            file.close();
            return fileBuffer;
        }

        return std::vector<char>();
    }

    best_device_info select_physical_device(VkPhysicalDevice* physicalDevices, uint32_t count)
    {
        int bestDeviceIndex = -1;
        uint32_t bestQueueFamily = 0;
        int bestRank = std::numeric_limits<int>::min();

        for (uint32_t d = 0; d < count; ++d)
        {
            VkPhysicalDeviceFeatures features{};
            vkGetPhysicalDeviceFeatures(physicalDevices[d], &features);
            if (features.shaderSampledImageArrayDynamicIndexing != VK_TRUE ||
                !supports_swapchain(physicalDevices[d]))
                continue;

            uint32_t queueFamilyPropertyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[d], &queueFamilyPropertyCount,
                                                     nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilyProperties(
                queueFamilyPropertyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[d], &queueFamilyPropertyCount,
                                                     queueFamilyProperties.data());

            uint32_t graphicsQueueFamily = queueFamilyPropertyCount;
            for (uint32_t f = 0; f < queueFamilyPropertyCount; ++f)
            {
                if (queueFamilyProperties[f].queueCount > 0 &&
                    (queueFamilyProperties[f].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
                {
                    graphicsQueueFamily = f;
                    break;
                }
            }
            if (graphicsQueueFamily == queueFamilyPropertyCount)
                continue;

            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(physicalDevices[d], &properties);
            const int rank = device_type_rank(properties.deviceType);
            if (rank > bestRank)
            {
                bestRank = rank;
                bestDeviceIndex = static_cast<int>(d);
                bestQueueFamily = graphicsQueueFamily;
            }
        }

        if (bestDeviceIndex < 0)
        {
            throw std::runtime_error(
                "Could not find a Vulkan device with graphics, swapchain, and dynamic sampler "
                "indexing support");
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevices[bestDeviceIndex], &properties);

        return best_device_info{properties.deviceName, bestDeviceIndex, bestQueueFamily, 1};
    }

    VkCommandBuffer beginSingleTimeCommands(const VkCommandPool& commandPool)
    {
        // --------------- Create Command Buffer Allocate Info ---------------

        VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.pNext = nullptr;
        commandBufferAllocateInfo.commandPool = commandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = 1;

        // --------------- Allocate Command Buffer ---------------

        VkCommandBuffer commandBuffer{};
        vku::err_check(vkAllocateCommandBuffers(VulkanSharedInfo::getInstance()->device,
                                                &commandBufferAllocateInfo, &commandBuffer));

        VkCommandBufferBeginInfo commandBufferBeginInfo;
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.pNext = nullptr;
        commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        commandBufferBeginInfo.pInheritanceInfo = nullptr;

        try
        {
            err_check(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
        }
        catch (...)
        {
            vkFreeCommandBuffers(VulkanSharedInfo::getInstance()->device, commandPool, 1,
                                 &commandBuffer);
            throw;
        }

        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer, const VkCommandPool& commandPool,
                               const VkQueue& queue)
    {
        try
        {
            vku::err_check(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.pWaitSemaphores = nullptr;
        submitInfo.pWaitDstStageMask = nullptr;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = nullptr;

            vku::err_check(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
            vku::err_check(vkQueueWaitIdle(queue));
        }
        catch (...)
        {
            vkFreeCommandBuffers(VulkanSharedInfo::getInstance()->device, commandPool, 1,
                                 &commandBuffer);
            throw;
        }

        vkFreeCommandBuffers(VulkanSharedInfo::getInstance()->device, commandPool, 1,
                             &commandBuffer);
    }

    void create_buffer(const VkDeviceSize& deviceSize, const VkBufferUsageFlags& bufferUsageFlags,
                       VkBuffer& buffer, const VkMemoryPropertyFlags& memoryPropertyFlags,
                       VmaAllocation& allocation,
                       VmaAllocationCreateFlags allocation_flags,
                       VmaAllocationInfo* allocation_info)
    {
        // --------------- Create Buffer Create Info ---------------

        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.pNext = nullptr;
        bufferCreateInfo.flags = 0;
        bufferCreateInfo.size = deviceSize;
        bufferCreateInfo.usage = bufferUsageFlags;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferCreateInfo.queueFamilyIndexCount = 0;
        bufferCreateInfo.pQueueFamilyIndices = nullptr;

        VmaAllocationCreateInfo allocationCreateInfo{};
        allocationCreateInfo.usage = (memoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0
                                         ? VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
                                         : VMA_MEMORY_USAGE_AUTO;
        allocationCreateInfo.requiredFlags = memoryPropertyFlags;
        allocationCreateInfo.flags = allocation_flags;
        if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
        {
            allocationCreateInfo.flags |=
                (bufferUsageFlags & VK_BUFFER_USAGE_TRANSFER_DST_BIT) != 0
                    ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
                    : VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        }

        vku::err_check(vmaCreateBuffer(VulkanSharedInfo::getInstance()->allocator,
                                       &bufferCreateInfo, &allocationCreateInfo, &buffer,
                                       &allocation, allocation_info));
    }

    void copy_buffer(const VkBuffer& src, VkBuffer& dest, const VkDeviceSize& deviceSize,
                     const VkCommandPool& commandPool, const VkQueue& queue)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool);

        VkBufferCopy bufferCopy{};
        bufferCopy.srcOffset = 0;
        bufferCopy.dstOffset = 0;
        bufferCopy.size = deviceSize;

        vkCmdCopyBuffer(commandBuffer, src, dest, 1, &bufferCopy);

        endSingleTimeCommands(commandBuffer, commandPool, queue);
    }

    VkPresentModeKHR select_present_mode(const VkPresentModeKHR* presentModes,
                                         uint32_t presentModeCount, bool vsync)
    {
        if (presentModes == nullptr || presentModeCount == 0)
            throw std::invalid_argument("Vulkan present modes must not be empty");
        const auto supports = [&](VkPresentModeKHR mode) {
            return std::find(presentModes, presentModes + presentModeCount, mode) !=
                   presentModes + presentModeCount;
        };

        if (!vsync && supports(VK_PRESENT_MODE_IMMEDIATE_KHR))
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        if (supports(VK_PRESENT_MODE_MAILBOX_KHR))
            return VK_PRESENT_MODE_MAILBOX_KHR;
        if (supports(VK_PRESENT_MODE_FIFO_KHR))
            return VK_PRESENT_MODE_FIFO_KHR;
        throw std::runtime_error("Vulkan surface exposes no supported presentation mode");
    }

    void print_layers()
    {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        VkLayerProperties* layerProperties = new VkLayerProperties[layerCount];
        vkEnumerateInstanceLayerProperties(&layerCount, layerProperties);

        std::cout << "\"VkLayerProperties\": [\n";
        for (uint32_t i = 0; i < layerCount; ++i)
        {
            std::cout << "{\n"
                      << "\t\"layerName\": \"" << layerProperties[i].layerName << "\",\n"
                      << "\t\"specVersion\": " << layerProperties[i].specVersion << ",\n"
                      << "\t\"implementationVersion\": " << layerProperties[i].implementationVersion
                      << ",\n"
                      << "\t\"description\": \"" << layerProperties[i].description << "\"\n"
                      << "}" << (i + 1 == layerCount ? "\n" : ",\n");
        }
        std::cout << "],\n";

        delete[] layerProperties;
    }

    void print_extensions()
    {
        uint32_t extencionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extencionCount, nullptr);
        VkExtensionProperties* extensionProperties = new VkExtensionProperties[extencionCount];
        vkEnumerateInstanceExtensionProperties(nullptr, &extencionCount, extensionProperties);

        std::cout << "\"VkExtensionProperties\": [\n";
        for (uint32_t i = 0; i < extencionCount; ++i)
        {
            std::cout << "{\n"
                      << "\t\"extensionName\": \"" << extensionProperties[i].extensionName
                      << "\",\n"
                      << "\t\"specVersion\": " << extensionProperties[i].specVersion << "\n"
                      << "}" << (i + 1 == extencionCount ? "\n" : ",\n");
        }
        std::cout << "],\n";

        delete[] extensionProperties;
    }

    void print_physical_devices()
    {
        std::cout << "\"VkPhysicalDevices\": [\n";
        for (uint32_t i = 0; i < VulkanSharedInfo::getInstance()->physicalDeviceCount; ++i)
        {
            std::cout << "\t{\n";

            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(VulkanSharedInfo::getInstance()->physicalDevices[i],
                                          &properties);

            std::cout << "\t\"VkPhysicalDeviceProperties\": {\n"
                      << "\t\t\"apiVersion\": \"" << VK_VERSION_MAJOR(properties.apiVersion) << "."
                      << VK_VERSION_MINOR(properties.apiVersion) << "."
                      << VK_VERSION_PATCH(properties.apiVersion) << "\",\n"
                      << "\t\t\"driverVersion\": " << properties.driverVersion << ",\n"
                      << "\t\t\"vendorID\": " << properties.vendorID << ",\n"
                      << "\t\t\"deviceID\": " << properties.deviceID << ",\n"
                      << "\t\t\"deviceType\": \"" << device_type(properties.deviceType) << "\",\n"
                      << "\t\t\"deviceName\": \"" << properties.deviceName << "\",\n"
                      << "\t\t\"pipelineCacheUUID\": \"" << properties.pipelineCacheUUID
                      << "\"\n"
                      //<< "\t\t\"limits\": " << properties.limits << ",\n"
                      //<< "\t\t\"sparseProperties\": \"" << properties.sparseProperties << "\",\n"
                      << "\t},\n";

            VkPhysicalDeviceFeatures features{};
            vkGetPhysicalDeviceFeatures(VulkanSharedInfo::getInstance()->physicalDevices[i],
                                        &features);

            std::cout << "\t\"VkPhysicalDeviceFeatures\": {\n"
                      << "\t\t\"geometryShader\": " << std::boolalpha << features.geometryShader
                      << ",\n"
                      << "\t\t\"tessellationShader\": " << std::boolalpha
                      << features.tessellationShader << "\n"
                      << "\t},\n";

            VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties{};
            vkGetPhysicalDeviceMemoryProperties(VulkanSharedInfo::getInstance()->physicalDevices[i],
                                                &physicalDeviceMemoryProperties);

            uint32_t queueFamilyPropertyCount;
            vkGetPhysicalDeviceQueueFamilyProperties(
                VulkanSharedInfo::getInstance()->physicalDevices[i], &queueFamilyPropertyCount,
                NULL);

            VkQueueFamilyProperties* queueFamilyProperties =
                new VkQueueFamilyProperties[queueFamilyPropertyCount];
            vkGetPhysicalDeviceQueueFamilyProperties(
                VulkanSharedInfo::getInstance()->physicalDevices[i], &queueFamilyPropertyCount,
                queueFamilyProperties);

            std::cout << "\t\"VkQueueFamilyProperties\": [\n";
            for (uint32_t q = 0; q < queueFamilyPropertyCount; ++q)
            {
                std::cout
                    << "\t\t{\n"
                    << "\t\t\t\"VK_QUEUE_GRAPHICS_BIT\": " << std::boolalpha
                    << ((queueFamilyProperties[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) << ",\n"
                    << "\t\t\t\"VK_QUEUE_COMPUTE_BIT\": " << std::boolalpha
                    << ((queueFamilyProperties[q].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) << ",\n"
                    << "\t\t\t\"VK_QUEUE_TRANSFER_BIT\": " << std::boolalpha
                    << ((queueFamilyProperties[q].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0) << ",\n"
                    << "\t\t\t\"VK_QUEUE_SPARSE_BINDING_BIT\": " << std::boolalpha
                    << ((queueFamilyProperties[q].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) != 0)
                    << ",\n"
                    << "\t\t\t\"queueCount\": " << queueFamilyProperties[q].queueCount << ",\n"
                    << "\t\t\t\"timestampValidBits\": "
                    << queueFamilyProperties[q].timestampValidBits << "\n"
                    << "\t\t}" << (q + 1 == queueFamilyPropertyCount ? "\n" : ",\n");
            }
            std::cout << "\t]\n";

            std::cout << "\t}"
                      << (i + 1 == VulkanSharedInfo::getInstance()->physicalDeviceCount ? "\n"
                                                                                        : ",\n");

            delete[] queueFamilyProperties;
        }

        std::cout << "],\n";
    }

    void print_selected_device()
    {

        std::cout << "\"selected_device\": {\n"
                  << "\t\"deviceName\": " << VulkanSharedInfo::getInstance()->hardware_name << ",\n"
                  << "\t\"queueCount\": " << VulkanSharedInfo::getInstance()->queueCount << ",\n"
                  << "\t\"queueFamilyIndex\": " << VulkanSharedInfo::getInstance()->queueFamilyIndex
                  << "\n"
                  << "}\n";
    }

    void err_check(const VkResult& result)
    {
        if (result != VK_SUCCESS)
        {
            switch (result)
            {
            case VK_NOT_READY:
                err("A fence or query has not yet completed");
                break;
            case VK_TIMEOUT:
                err("A wait operation has not completed in the specified time");
                break;
            case VK_EVENT_SET:
                err("An event is signaled");
                break;
            case VK_EVENT_RESET:
                err("An event is unsignaled");
                break;
            case VK_INCOMPLETE:
                err("A return array was too small for the result");
                break;
            case VK_SUBOPTIMAL_KHR:
                err("A swapchain no longer matches the surface properties exactly, but can still "
                    "be used to present to the surface successfully.");
                break;
            case VK_ERROR_OUT_OF_HOST_MEMORY:
                err("A host memory allocation has failed.");
                break;
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                err("A device memory allocation has failed.");
                break;
            case VK_ERROR_INITIALIZATION_FAILED:
                err("Initialization of an object could not be completed for "
                    "implementation-specific reasons.");
                break;
            case VK_ERROR_DEVICE_LOST:
                err("The logical or physical device has been lost. See Lost Device");
                break;
            case VK_ERROR_MEMORY_MAP_FAILED:
                err("Mapping of a memory object has failed.");
                break;
            case VK_ERROR_LAYER_NOT_PRESENT:
                err("A requested layer is not present or could not be loaded.");
                break;
            case VK_ERROR_EXTENSION_NOT_PRESENT:
                err("A requested extension is not supported.");
                break;
            case VK_ERROR_FEATURE_NOT_PRESENT:
                err("A requested feature is not supported.");
                break;
            case VK_ERROR_INCOMPATIBLE_DRIVER:
                err("The requested version of Vulkan is not supported by the driver or is "
                    "otherwise incompatible for implementation-specific reasons.");
                break;
            case VK_ERROR_TOO_MANY_OBJECTS:
                err("Too many objects of the type have already been created.");
                break;
            case VK_ERROR_FORMAT_NOT_SUPPORTED:
                err("A requested format is not supported on this device.");
                break;
            case VK_ERROR_FRAGMENTED_POOL:
                err("A pool allocation has failed due to fragmentation of the pool's memory. This "
                    "must only be returned if no attempt to allocate host or device memory was "
                    "made to accommodate the new allocation. This should be returned in preference "
                    "to VK_ERROR_OUT_OF_POOL_MEMORY, but only if the implementation is certain "
                    "that the pool allocation failure was due to fragmentation.");
                break;
            case VK_ERROR_SURFACE_LOST_KHR:
                err("A surface is no longer available.");
                break;
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
                err("The requested window is already in use by Vulkan or another API in a manner "
                    "which prevents it from being used again.");
                break;
            case VK_ERROR_OUT_OF_DATE_KHR:
                err("A surface has changed in such a way that it is no longer compatible with the "
                    "swapchain, and further presentation requests using the swapchain will fail. "
                    "Applications must query the new surface properties and recreate their "
                    "swapchain if they wish to continue presenting to the surface.");
                break;
            // case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            //	err("The display used by a swapchain does not use the same presentable image layout,
            // or is incompatible in a way that prevents sharing an image."); 	break; case
            // VK_ERROR_INVALID_SHADER_NV: 	err("One or more shaders failed to compile or link. More
            // details are reported back to the application via VK_EXT_debug_report if enabled.");
            //	break;
            case VK_ERROR_OUT_OF_POOL_MEMORY:
                err("A pool memory allocation has failed. This must only be returned if no attempt "
                    "to allocate host or device memory was made to accommodate the new allocation. "
                    "If the failure was definitely due to fragmentation of the pool, "
                    "VK_ERROR_FRAGMENTED_POOL should be returned instead.");
                break;
            case VK_ERROR_INVALID_EXTERNAL_HANDLE:
                err("An external handle is not a valid handle of the specified type.");
                break;
            case VK_ERROR_FRAGMENTATION:
                err("A descriptor pool creation has failed due to fragmentation.");
                break;
            case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
                err("A buffer creation or memory allocation failed because the requested address "
                    "is not available. A shader group handle assignment failed because the "
                    "requested shader group handle information is no longer valid.");
                break;
            // case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
            //	err("An operation on a swapchain created with
            // VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT failed as it did not have
            // exlusive full-screen access. This may occur due to implementation-dependent reasons,
            // outside of the application's control."); 	break;
            default:
                err("An unknown error has occurred; either the application has provided invalid "
                    "input, or an implementation failure has occurred");
                break;
            }

            throw std::runtime_error("Vulkan operation failed with VkResult " +
                                     std::to_string(static_cast<int>(result)));
        }
    }
} // namespace gcanvas::vku
