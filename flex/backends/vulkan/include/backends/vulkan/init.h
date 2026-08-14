/* Flex Vulkan backend integration entry. */
#pragma once

#include "flex/bridge/renderer.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <cstdint>

namespace flex {

namespace vulkan_backend {

/**
 * Non-owning per-frame Vulkan destination.
 *
 * The host owns every Vulkan handle and must keep the instance, physical
 * device, logical device and queue alive until the renderer is destroyed. Before begin_frame(),
 * command_buffer must be recording, image must be available to the host, and
 * image_layout must describe that image's actual current layout. The renderer
 * records native gCanvas draw commands and layout transitions directly into
 * command_buffer, then updates image_layout to final_layout. Queue submission and synchronization
 * remain the host's responsibility. The host must finish any prior submission
 * that references this renderer before the next begin_frame() or before
 * destroying the renderer. Renderer construction and image/font uploads may
 * submit and wait on the supplied queue, so the host must externally synchronize
 * access to it. Extent or device changes require a new renderer;
 * image and command_buffer may be changed between synchronized frames. A
 * renderer is single-threaded: the host must not mutate this struct while any
 * renderer method is executing.
 */
struct VulkanCanvas {
  VkInstance instance = VK_NULL_HANDLE;
  std::uint32_t api_version = VK_API_VERSION_1_0;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  std::uint32_t queue_family_index = VK_QUEUE_FAMILY_IGNORED;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  VkImage image = VK_NULL_HANDLE;
  VkExtent2D extent{};
  VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
  VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageLayout final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  VkPipelineStageFlags source_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  VkAccessFlags source_access_mask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  VkPipelineStageFlags final_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  VkAccessFlags final_access_mask = VK_ACCESS_MEMORY_READ_BIT;
};

/** Returns whether @p format is supported by the native gCanvas Vulkan render pass. */
bool is_supported_format(VkFormat format) noexcept;

/** Returns whether all required handles, extent, layout, format and stages are valid. */
bool is_valid_canvas(const VulkanCanvas &canvas) noexcept;

/** Registers the Vulkan factory and selects it only when no default exists. */
inline bool register_backend();

/**
 * Creates a renderer for @p canvas.
 *
 * Returns nullptr when the contract above is incomplete or gCanvas cannot
 * initialize its borrowed-device resources.
 * After creation, begin_frame() throws std::invalid_argument for a changed
 * device/extent or mismatched frame size, and begin/end ordering violations
 * throw std::logic_error.
 */
std::unique_ptr<Renderer> create_renderer(VulkanCanvas *canvas);

} // namespace vulkan_backend

/** Opaque factory form used by the backend-neutral renderer registry. */
std::unique_ptr<Renderer> create_vulkan_renderer(CanvasHandle canvas);

namespace vulkan_backend {

inline bool register_backend() {
  flex::register_renderer_backend(RendererBackend::Vulkan,
                                  static_cast<RendererFactory>(&flex::create_vulkan_renderer));
  if (!flex::default_renderer_factory()) {
    return flex::set_default_renderer_backend(RendererBackend::Vulkan);
  }
  return true;
}

inline std::unique_ptr<Renderer> create_renderer(VulkanCanvas *canvas) {
  return flex::create_vulkan_renderer(static_cast<CanvasHandle>(canvas));
}

} // namespace vulkan_backend
} // namespace flex
