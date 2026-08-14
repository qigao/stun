#include "backends/vulkan/init.h"
#include "tinytest.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace flex;

namespace {

constexpr std::uint32_t test_extent = 16;
constexpr VkDeviceSize test_buffer_size =
    static_cast<VkDeviceSize>(test_extent) * test_extent * sizeof(std::uint32_t);
constexpr std::uint32_t invalid_index = (std::numeric_limits<std::uint32_t>::max)();

enum class SetupResult { ready, no_device, failure };

struct VulkanFixture {
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  std::uint32_t queue_family = invalid_index;
  VkCommandPool command_pool = VK_NULL_HANDLE;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory image_memory = VK_NULL_HANDLE;
  VkBuffer readback_buffer = VK_NULL_HANDLE;
  VkDeviceMemory readback_memory = VK_NULL_HANDLE;
  const char *failure_stage = "none";

  ~VulkanFixture() {
    if (device != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(device);
      if (readback_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, readback_buffer, nullptr);
      }
      if (readback_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, readback_memory, nullptr);
      }
      if (image != VK_NULL_HANDLE) {
        vkDestroyImage(device, image, nullptr);
      }
      if (image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, image_memory, nullptr);
      }
      if (command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, command_pool, nullptr);
      }
      vkDestroyDevice(device, nullptr);
    }
    if (instance != VK_NULL_HANDLE) {
      vkDestroyInstance(instance, nullptr);
    }
  }

  std::uint32_t find_memory_type(std::uint32_t type_bits, VkMemoryPropertyFlags required) const {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);
    for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
      const bool type_matches = (type_bits & (1u << index)) != 0;
      const bool flags_match = (properties.memoryTypes[index].propertyFlags & required) == required;
      if (type_matches && flags_match) {
        return index;
      }
    }
    return invalid_index;
  }

  bool allocate_and_bind_image() {
    VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_B8G8R8A8_UNORM;
    image_info.extent = {test_extent, test_extent, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device, &image_info, nullptr, &image) != VK_SUCCESS) {
      failure_stage = "vkCreateImage";
      return false;
    }

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, image, &requirements);
    const auto memory_type =
        find_memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memory_type == invalid_index) {
      failure_stage = "device-local image memory";
      return false;
    }

    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memory_type;
    if (vkAllocateMemory(device, &allocation, nullptr, &image_memory) != VK_SUCCESS ||
        vkBindImageMemory(device, image, image_memory, 0) != VK_SUCCESS) {
      failure_stage = "image memory allocation";
      return false;
    }
    return true;
  }

  bool allocate_and_bind_readback_buffer() {
    VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = test_buffer_size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &buffer_info, nullptr, &readback_buffer) != VK_SUCCESS) {
      failure_stage = "vkCreateBuffer";
      return false;
    }

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, readback_buffer, &requirements);
    const auto memory_type =
        find_memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memory_type == invalid_index) {
      failure_stage = "host-coherent readback memory";
      return false;
    }

    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memory_type;
    if (vkAllocateMemory(device, &allocation, nullptr, &readback_memory) != VK_SUCCESS ||
        vkBindBufferMemory(device, readback_buffer, readback_memory, 0) != VK_SUCCESS) {
      failure_stage = "readback memory allocation";
      return false;
    }
    return true;
  }

  SetupResult initialize() {
    VkApplicationInfo application_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application_info.pApplicationName = "Flex Vulkan backend test";
    application_info.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instance_info.pApplicationInfo = &application_info;
    if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS) {
      failure_stage = "vkCreateInstance";
      return SetupResult::failure;
    }

    std::uint32_t physical_device_count = 0;
    if (vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr) != VK_SUCCESS) {
      failure_stage = "vkEnumeratePhysicalDevices";
      return SetupResult::failure;
    }
    if (physical_device_count == 0) {
      return SetupResult::no_device;
    }
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    if (vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data()) !=
        VK_SUCCESS) {
      failure_stage = "vkEnumeratePhysicalDevices data";
      return SetupResult::failure;
    }
    physical_device = physical_devices.front();

    std::uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count, families.data());
    for (std::uint32_t index = 0; index < family_count; ++index) {
      if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
        queue_family = index;
        break;
      }
    }
    if (queue_family == invalid_index) {
      failure_stage = "graphics queue family";
      return SetupResult::failure;
    }

    constexpr float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex = queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;
    VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    if (vkCreateDevice(physical_device, &device_info, nullptr, &device) != VK_SUCCESS) {
      failure_stage = "vkCreateDevice";
      return SetupResult::failure;
    }
    vkGetDeviceQueue(device, queue_family, 0, &queue);

    VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_info.queueFamilyIndex = queue_family;
    if (vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) != VK_SUCCESS) {
      failure_stage = "vkCreateCommandPool";
      return SetupResult::failure;
    }
    VkCommandBufferAllocateInfo command_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    command_info.commandPool = command_pool;
    command_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &command_info, &command_buffer) != VK_SUCCESS) {
      failure_stage = "vkAllocateCommandBuffers";
      return SetupResult::failure;
    }

    if (!allocate_and_bind_image() || !allocate_and_bind_readback_buffer()) {
      return SetupResult::failure;
    }
    return SetupResult::ready;
  }
};

} // namespace

suite("flex::vulkan_backend GPU round-trip") {
  it("uploads one lifecycle-checked BGRA frame into a Vulkan image") {
    VulkanFixture fixture;
    const SetupResult setup = fixture.initialize();
    if (setup == SetupResult::no_device) {
      info("No Vulkan physical device; GPU round-trip is not applicable");
      check_true(true);
      return;
    }
    if (setup != SetupResult::ready) {
      info("Vulkan setup failed at %s", fixture.failure_stage);
      check_int_eq(static_cast<int>(setup), static_cast<int>(SetupResult::ready));
      return;
    }

    VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    const VkResult begin_result = vkBeginCommandBuffer(fixture.command_buffer, &begin_info);
    check_int_eq(begin_result, VK_SUCCESS);
    if (begin_result != VK_SUCCESS) {
      return;
    }

    vulkan_backend::VulkanCanvas canvas;
    canvas.instance = fixture.instance;
    canvas.physical_device = fixture.physical_device;
    canvas.device = fixture.device;
    canvas.queue = fixture.queue;
    canvas.queue_family_index = fixture.queue_family;
    canvas.command_buffer = fixture.command_buffer;
    canvas.image = fixture.image;
    canvas.extent = {test_extent, test_extent};
    canvas.format = VK_FORMAT_B8G8R8A8_UNORM;
    canvas.image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    canvas.final_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    canvas.final_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    canvas.final_access_mask = VK_ACCESS_TRANSFER_READ_BIT;

    auto renderer = vulkan_backend::create_renderer(&canvas);
    check_not_null(renderer.get());
    if (!renderer) {
      return;
    }
    renderer->begin_frame(static_cast<float>(test_extent), static_cast<float>(test_extent), 1.0f);
    check_throws_as(renderer->begin_frame(static_cast<float>(test_extent),
                                          static_cast<float>(test_extent), 1.0f),
                    std::logic_error);
    renderer->clear(Color{1.0f, 0.0f, 0.0f, 1.0f});
    canvas.image = reinterpret_cast<VkImage>(static_cast<std::uintptr_t>(1));
    check_throws_as(renderer->end_frame(), std::invalid_argument);
    canvas.image = fixture.image;
    renderer->end_frame();
    check_throws_as(renderer->end_frame(), std::logic_error);

    VkBufferImageCopy readback_copy{};
    readback_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    readback_copy.imageSubresource.mipLevel = 0;
    readback_copy.imageSubresource.baseArrayLayer = 0;
    readback_copy.imageSubresource.layerCount = 1;
    readback_copy.imageExtent = {test_extent, test_extent, 1};
    vkCmdCopyImageToBuffer(fixture.command_buffer, fixture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, fixture.readback_buffer, 1,
                           &readback_copy);

    const VkResult end_result = vkEndCommandBuffer(fixture.command_buffer);
    check_int_eq(end_result, VK_SUCCESS);
    if (end_result != VK_SUCCESS) {
      renderer.reset();
      return;
    }
    VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &fixture.command_buffer;
    const VkResult submit_result = vkQueueSubmit(fixture.queue, 1, &submit_info, VK_NULL_HANDLE);
    check_int_eq(submit_result, VK_SUCCESS);
    if (submit_result != VK_SUCCESS) {
      renderer.reset();
      return;
    }
    const VkResult wait_result = vkQueueWaitIdle(fixture.queue);
    check_int_eq(wait_result, VK_SUCCESS);
    if (wait_result != VK_SUCCESS) {
      renderer.reset();
      return;
    }

    void *mapped = nullptr;
    const VkResult map_result =
        vkMapMemory(fixture.device, fixture.readback_memory, 0, test_buffer_size, 0, &mapped);
    check_int_eq(map_result, VK_SUCCESS);
    if (map_result != VK_SUCCESS) {
      renderer.reset();
      return;
    }
    check_not_null(mapped);
    if (mapped) {
      const auto *pixel = static_cast<const std::uint8_t *>(mapped);
      check_uint_eq(pixel[0], 0u);
      check_uint_eq(pixel[1], 0u);
      check_uint_eq(pixel[2], 255u);
      check_uint_eq(pixel[3], 255u);
      vkUnmapMemory(fixture.device, fixture.readback_memory);
    }

    renderer.reset();
  }
}
